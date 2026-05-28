//
// Created by greenhand520 on 2026/5/14.
//

#include "cmsis_os2.h"

#include "bsp/adc.h"
#include "bsp/bq24725.h"
#include "bsp/husb238a.h"
#include "task/task_battery_state.h"
#include "task/task_board_state.h"
#include "log.h"

#include "rcl/publisher.h"
#include "hexapod_sensor_interface/msg/board_state.h"

static hexapod_sensor_interface__msg__BoardState s_msg_board;
extern rcl_publisher_t board_state_pub;
/** micro-ROS 连接状态标志 */
extern volatile uint8_t g_uros_connected;
extern volatile BQ40Z50_SharedData g_bq40z50_data;

/**
 * @brief 充电电流温度调节函数，根据电池温度和充电电路温度计算目标充电电流
 *        充电策略:
 *        电池温度 < 0°C 或 充电电路温度 > 60°C    → 0mA（停止充电）
 *        电池温度 0~10°C                        → 500mA（低温慢充）
 *        电池温度 10~45°C 且 充电电路温度 < 55°C  → 动态计算（见下方电流策略）
 *        电池温度 45~50°C 或 充电电路温度 55~60°C → 线性降至 500mA
 *        电池温度 > 50°C                        → 0mA（停止充电）
 *        电流策略：
 *        常温时的充电电流 = HUSB238A中诱骗的功率 * 0.85 / BQ40Z50中获取的当前电池电压，
 *        如果该电流大于CHARGE_CURRENT_MAX_MA，则取CHARGE_CURRENT_MAX_MA
 * @param batt_temp 电池温度
 * @param charger_temp 充电电路温度
 * @param husb_power_mW HUSB238A诱骗的功率 (mW)
 * @param batt_voltage_mv BQ40Z50当前电池电压 (mV)
 * @return 充电电流 (mA)
 */
static uint16_t calc_charge_current_ma(const float batt_temp,
                                       const float charger_temp,
                                       const float husb_power_mW,
                                       const float batt_voltage_mv) {
  // 电池温度保护
  if (batt_temp < BAT_TEMP_COLD_LIMIT || batt_temp > BAT_TEMP_HOT_LIMIT) {
    // 停止充电
    return 0;
  }

  uint16_t current_ma;

  if (batt_temp < BAT_TEMP_COOL_LIMIT) {
    // 低温慢充: 0~10°C → 500mA
    current_ma = CHARGE_CURRENT_MIN_MA;
  } else if (batt_temp <= BAT_TEMP_WARM_LIMIT) {
    // 正常范围: 10~45°C → 动态计算
    // 充电电流 = HUSB238A诱骗功率 * 0.85 / 电池电压
    if (husb_power_mW > 0.0f && batt_voltage_mv > 0.0f) {
      float calc_ma = husb_power_mW * 0.85f / batt_voltage_mv;
      if (calc_ma > CHARGE_CURRENT_MAX_MA) {
        calc_ma = CHARGE_CURRENT_MAX_MA;
      }
      current_ma = (uint16_t)calc_ma;
    } else {
      // HUSB238A功率或电池电压无效时，使用最小充电电流
      current_ma = CHARGE_CURRENT_MIN_MA;
    }
  } else {
    // 高温降流: 45~50°C → 线性从 MAX 降至 500mA
    float ratio = (BAT_TEMP_HOT_LIMIT - batt_temp) /
                  (BAT_TEMP_HOT_LIMIT - BAT_TEMP_WARM_LIMIT);
    if (ratio < 0.0f) {
      ratio = 0.0f;
    }
    current_ma =
        (uint16_t)(CHARGE_CURRENT_MIN_MA +
                   ratio * (CHARGE_CURRENT_MAX_MA / 2 - CHARGE_CURRENT_MIN_MA));
  }

  // 充电电路温度保护（NTC2 = TEMP2_ADC = charger_state.temperature）
  if (charger_temp > CHG_TEMP_HOT_LIMIT) {
    // 充电电路过热，停止充电
    return 0;
  }
  if (charger_temp > CHG_TEMP_WARM_LIMIT) {
    // 充电电路 70~80°C → 线性降流
    float ratio = (CHG_TEMP_HOT_LIMIT - charger_temp) /
                  (CHG_TEMP_HOT_LIMIT - CHG_TEMP_WARM_LIMIT);
    if (ratio < 0.0f) {
      ratio = 0.0f;
    }
    const uint16_t reduced =
        (uint16_t)(CHARGE_CURRENT_MIN_MA +
                   ratio * (current_ma - CHARGE_CURRENT_MIN_MA));
    if (reduced < current_ma) {
      current_ma = reduced;
    }
    if (current_ma < CHARGE_CURRENT_MIN_MA && current_ma != 0)
      current_ma = CHARGE_CURRENT_MIN_MA;
  }

  // 钳位到合理范围
  if (current_ma > CHARGE_CURRENT_MAX_MA)
    current_ma = CHARGE_CURRENT_MAX_MA;

  return current_ma;
}

/** @brief 充电状态枚举 */
typedef enum {
  CHARGE_STATUS_UNKNOWN     = 0,/**< 未知 */
  CHARGE_STATUS_NOT_CHARGED = 1,/**< 未充电 */
  CHARGE_STATUS_PRE_CHARGE  = 2,/**< 预充电 */
  CHARGE_STATUS_CC          = 3,/**< 恒流充电 */
  CHARGE_STATUS_CV          = 4,/**< 恒压充电 */
  CHARGE_STATUS_FULL        = 5,/**< 已充满 */
  CHARGE_STATUS_HUSB_FAULT  = 6,/**< HUSB238A 故障 */
  CHARGE_STATUS_UNSUPPORTED = 7,/**< 充电器不支持充电 */
} ChargeStatus;

/** @brief 禁止充电并更新状态 */
static void disable_charging(uint16_t *s_charge_current_ma) {
  if (*s_charge_current_ma != 0) {
    uint16_t opt_raw = 0;
    if (BQ24725_GetChargeOption(&opt_raw) == SUCCESS) {
      BQ24725_charge_options opt;
      BQ24725_FormOptionsStruct(opt_raw, &opt);
      opt.charge_inhibit = charge_inhibit;
      BQ24725_SetChargeOption(&opt);
    }
    *s_charge_current_ma = 0;
  }
}

void Task_BoardState(void *argument) {
  (void)argument;
  static uint16_t s_charge_current_ma = CHARGE_CURRENT_MAX_MA;
  osDelay(2000);

  for (;;) {
    // 会阻塞任务直到 DMA 完成回调发送标志位 `0x01`，将任务设置为被动等待状态，
    // 这里for循环最后不需要额外的 `osDelay()`
    osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);
    ADC_ProcessData();

    uint16_t bq_cc = 0, bq_cv = 0, bq_ic = 0;
    BQ24725_GetChargeCurrent(&bq_cc);
    BQ24725_GetChargeVoltage(&bq_cv);
    BQ24725_GetInputCurrent(&bq_ic);

    // 从共享结构体读取 BQ40Z50 数据
    const float batt_temp = g_bq40z50_data.temp_c;
    const int16_t batt_i_ma = g_bq40z50_data.current_ma;
    const uint16_t batt_voltage_mv = g_bq40z50_data.voltage_mv;
    const uint8_t batt_soc = g_bq40z50_data.soc;

    const float charger_temp = ADC_GetTemp2();

    // 从 HUSB238A 获取协商的充电参数，计算诱骗功率 (mW)
    const uint16_t husb_voltage_mv = HUSB238A_GetContractChgVoltage();
    const float husb_current_ma = HUSB238A_GetContractChgCurrent();
    const float husb_power_mW =
        (float)husb_voltage_mv * husb_current_ma / 1000.0f;

    // 检查 HUSB238A 连接和支持状态
    const bool husb_attached = HUSB238A_IsAttached();
    bool husb_support_charge = false;
    if (husb_attached) {
      HUSB238A_IsSupportCharge(&husb_support_charge);
    }
    const bool husb_fault = HUSB238A_IsFault();

    // 根据 HUSB238A 状态决定充电行为和 charge_status
    uint8_t charge_status = CHARGE_STATUS_UNKNOWN;

    if (!husb_attached) {
      // 没有 PD 充电器连接
      charge_status = CHARGE_STATUS_NOT_CHARGED;
      disable_charging(&s_charge_current_ma);
    } else if (husb_fault) {
      // HUSB238A 故障
      charge_status = CHARGE_STATUS_HUSB_FAULT;
      disable_charging(&s_charge_current_ma);
    } else if (!husb_support_charge) {
      // 充电器不支持充电 (无合适的 >=19V PDO)
      charge_status = CHARGE_STATUS_UNSUPPORTED;
      disable_charging(&s_charge_current_ma);
    } else {
      BQ24725_SetInputCurrent((uint16_t)husb_current_ma);
      // 充电器已连接且支持充电
      const uint16_t target_current = calc_charge_current_ma(
          batt_temp, charger_temp, husb_power_mW, batt_voltage_mv);

      if (batt_soc >= 100) {
        // 电池已满
        charge_status = CHARGE_STATUS_FULL;
        disable_charging(&s_charge_current_ma);
      } else if (target_current == 0) {
        // 温度保护导致停止充电
        charge_status = CHARGE_STATUS_NOT_CHARGED;
        disable_charging(&s_charge_current_ma);
      } else {
        // 设置充电电流
        if (target_current > s_charge_current_ma + 200 ||
            target_current < s_charge_current_ma - 200 ||
            (target_current == 0 && s_charge_current_ma != 0)) {
          if (target_current == 0) {
            uint16_t opt_raw = 0;
            if (BQ24725_GetChargeOption(&opt_raw) == SUCCESS) {
              BQ24725_charge_options opt;
              BQ24725_FormOptionsStruct(opt_raw, &opt);
              opt.charge_inhibit = charge_inhibit;
              BQ24725_SetChargeOption(&opt);
            }
          } else {
            BQ24725_SetChargeCurrent(target_current);
            uint16_t opt_raw = 0;
            if (BQ24725_GetChargeOption(&opt_raw) == SUCCESS) {
              BQ24725_charge_options opt;
              BQ24725_FormOptionsStruct(opt_raw, &opt);
              opt.charge_inhibit = charge_enable;
              BQ24725_SetChargeOption(&opt);
            }
          }
          s_charge_current_ma = target_current;
        }

        // 判断充电阶段
        if (batt_i_ma > 0) {
          // 电池正在充电，根据电压和电流判断阶段
          if (batt_voltage_mv >= (uint16_t)(CHARGE_VOLTAGE_DEFAULT - 200)) {
            // 电池电压接近充电电压 → CV 阶段
            charge_status = CHARGE_STATUS_CV;
          } else if (batt_voltage_mv < 12) {
            // 小于12V → 预充电阶段
            charge_status = CHARGE_STATUS_PRE_CHARGE;
          } else {
            // 正常恒流充电 → CC 阶段
            charge_status = CHARGE_STATUS_CC;
          }
        } else {
          // 充电器已连接但电池未在充电
          charge_status = CHARGE_STATUS_NOT_CHARGED;
        }
      }
    }

    /* BoardState */
    s_msg_board.header.stamp.sec = 0;
    s_msg_board.header.stamp.nanosec = 0;
    s_msg_board.servo_power_temperature = ADC_GetTemp1();
    s_msg_board.servo_power_voltage_v = ADC_GetSVM();
    s_msg_board.mcu_temperature = ADC_GetMCUTemp();
    s_msg_board.charger_state.ac_present = (BQ24725_ACOK_Read() != 0);
    s_msg_board.charger_state.charging =
        (bq_cc > 0 && batt_i_ma > 0);
    s_msg_board.charger_state.charge_current_a =
        (float)bq_cc / 1000.0f;
    s_msg_board.charger_state.charge_voltage_v = (float)bq_cv / 1000.0f;
    s_msg_board.charger_state.input_current_a = (float)bq_ic / 1000.0f;
    s_msg_board.charger_state.realtime_current_a =
        ADC_GetBC_IOUT_mA() / 1000.0f;
    s_msg_board.charger_state.realtime_input_voltage_v = ADC_GetCV();
    s_msg_board.charger_state.charge_status = charge_status;
    s_msg_board.charger_state.temperature = charger_temp;

    if (g_uros_connected) {
      const rcl_ret_t rcl_ret =
          rcl_publish(&board_state_pub, &s_msg_board, NULL);
      if (rcl_ret != RCL_RET_OK) {
        LOG_ERR("Failed to publish board state with error %d", rcl_ret);
      }
    }
  }
}

void Task_CreateBoardState(void) {
  const osThreadAttr_t task_attr = {
      .name = "BoardState",
      .stack_size = 1024 * 4, // 4KB - ADC处理 + BQ24725/HUSB238A I2C + 复杂充电逻辑 + publish
      .priority = (osPriority_t)osPriorityNormal5,
  };
  taskBoardStateHandle = osThreadNew(Task_BoardState, NULL, &task_attr);
}
