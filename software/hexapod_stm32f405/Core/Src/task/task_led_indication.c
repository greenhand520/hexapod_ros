//
// Created by greenhand520 on 2026/5/14.
//

#include "task/task_led_indication.h"
#include "bsp/adc.h"
#include "bsp/bq24725.h"
#include "bsp/bq40z50.h"
#include "bsp/ws2812.h"
#include "main.h"

/** 电池过温计数 */
static uint16_t s_batt_hot_cnt = 0;
/** 充电电路过温计数 */
static uint16_t s_chg_hot_cnt = 0;
/** 舵机供电过温计数 */
static uint16_t s_servo_hot_cnt = 0;
/** 电池过温保护已触发 */
static uint8_t s_batt_protect = 0;
/** 充电过温保护已触发 */
static uint8_t s_chg_protect = 0;
/** 舵机供电过温保护已触发 */
static uint8_t s_servo_protect = 0;
/** 风扇状态（迟滞控制） */
static uint8_t s_fan_on = 0;

/** micro-ROS 连接状态标志 */
extern volatile uint8_t g_uros_connected;

/**
 * @brief 温度 → 颜色映射
 *        绿色: < 40°C, 橙色: 40~55°C, 警告红: > 55°C
 * @param temp 舵机供电温度、充电温度 (°C)
 * @return ws2812颜色
 */
static WS2812_Color_t temp_to_color(const float temp) {
  if (temp > TEMP_DANGER_C) {
    return WS2812_COLOR_WARN_RED;
  }
  if (temp > TEMP_WARN_C) {
    return WS2812_COLOR_ORANGE;
  }
  return WS2812_COLOR_GREEN;
}

/**
 * @brief 电池温度 → 颜色映射
 *        蓝色: < 10°C (低温), 绿色: 10~40°C, 橙色: 40~55°C, 警告红: > 55°C
 * @param temp 温度 (°C)
 * @return ws2812颜色
 */
static WS2812_Color_t batt_temp_to_color(const float temp) {
  if (temp > TEMP_DANGER_C) {
    return WS2812_COLOR_WARN_RED;
  }
  if (temp > TEMP_WARN_C) {
    return WS2812_COLOR_ORANGE;
  }
  if (temp < TEMP_BATT_COLD_C && temp > TEMP_BATT_COLD_WARN_C) {
    return WS2812_COLOR_BLUE;
  }
  return WS2812_COLOR_GREEN;
}

/**
 * @brief 电池电量 → 颜色映射
 *        绿色: > 60%, 浅绿色: 30~60%, 橙色: 10~30%, 警告红: 0~10%
 * @param soc 电池电量
 * @return ws2812颜色
 */
static WS2812_Color_t soc_to_color(const uint8_t soc) {
  if (soc > 60)
    return WS2812_COLOR_GREEN;
  if (soc > 30)
    return WS2812_COLOR_LIME;
  if (soc > 10)
    return WS2812_COLOR_ORANGE;
  return WS2812_COLOR_WARN_RED;
}

/**
 * @brief 过温超时检测
 * @param servo_power_temp 舵机供电温度
 * @param charger_temp 电池充电温度
 * @param batt_temp 电池温度
 */
void overtemp_overtime_detect(const float servo_power_temp,
                              const float charger_temp, const float batt_temp) {
  // 电池-过温超时检测
  if (batt_temp > TEMP_DANGER_C) {
    if (s_batt_hot_cnt < OVERTEMP_TIMEOUT_TICKS)
      s_batt_hot_cnt++;
    if (s_batt_hot_cnt >= OVERTEMP_TIMEOUT_TICKS) {
      s_batt_protect = 1;
    }
  } else {
    if (s_batt_hot_cnt > 0)
      s_batt_hot_cnt--;
    if (s_batt_hot_cnt == 0 && batt_temp < TEMP_WARN_C) {
      s_batt_protect = 0;
    }
  }

  // 充电电路-过温超时检测
  if (charger_temp > TEMP_DANGER_C) {
    if (s_chg_hot_cnt < OVERTEMP_TIMEOUT_TICKS)
      s_chg_hot_cnt++;
    if (s_chg_hot_cnt >= OVERTEMP_TIMEOUT_TICKS) {
      s_chg_protect = 1;
    }
  } else {
    if (s_chg_hot_cnt > 0)
      s_chg_hot_cnt--;
    if (s_chg_hot_cnt == 0 && charger_temp < TEMP_WARN_C) {
      s_chg_protect = 0;
    }
  }

  // 舵机供电电路-过温超时检测
  if (servo_power_temp > TEMP_DANGER_C) {
    if (s_servo_hot_cnt < OVERTEMP_TIMEOUT_TICKS)
      s_servo_hot_cnt++;
    if (s_servo_hot_cnt >= OVERTEMP_TIMEOUT_TICKS) {
      s_servo_protect = 1;
    }
  } else {
    if (s_servo_hot_cnt > 0)
      s_servo_hot_cnt--;
    if (s_servo_hot_cnt == 0 && servo_power_temp < TEMP_WARN_C) {
      s_servo_protect = 0;
    }
  }
}

/**
 * @brief 控制风扇的逻辑
 * @param servo_power_temp 舵机供电温度
 * @param charger_temp 电池充电温度
 * @param mcu_temp mcu内部温度
 */
void fan_ctrl(const float servo_power_temp, const float charger_temp,
              const float mcu_temp) {
  if (!s_fan_on) {
    if (servo_power_temp > TEMP_FAN_ON_C || charger_temp > TEMP_FAN_ON_C ||
        mcu_temp > TEMP_FAN_ON_C) {
      HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_SET);
      s_fan_on = 1;
    }
  } else {
    if (servo_power_temp < TEMP_FAN_OFF_C && charger_temp < TEMP_FAN_OFF_C &&
        mcu_temp < TEMP_FAN_OFF_C) {
      HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_RESET);
      s_fan_on = 0;
    }
  }
}

void do_protection(void) {
  // 电池/充电过温 → 关闭充电
  if (s_batt_protect || s_chg_protect) {
    uint16_t opt_raw = 0;
    if (BQ24725_GetChargeOption(&opt_raw) == SUCCESS) {
      BQ24725_charge_options opt;
      BQ24725_FormOptionsStruct(opt_raw, &opt);
      opt.charge_inhibit = charge_inhibit;
      BQ24725_SetChargeOption(&opt);
    }
    BQ24725_SetChargeCurrent(0);
  }

  // 舵机供电过温 → 关闭 VM_EN
  if (s_servo_protect) {
    HAL_GPIO_WritePin(VM_EN_GPIO_Port, VM_EN_Pin, GPIO_PIN_RESET);
  } else {
    // 记得开启
    HAL_GPIO_WritePin(VM_EN_GPIO_Port, VM_EN_Pin, GPIO_PIN_SET);
  }
}

/**
 * @brief 更新ws2812指示灯的颜色
 * @param servo_power_temp 舵机供电温度
 * @param charger_temp 电池充电温度
 * @param batt_temp 电池温度
 * @param batt_soc 电池电量
 * @param charging 是否在充电
 * @param vm_en 舵机供电启停
 */
void update_status_leds(const float servo_power_temp, const float charger_temp,
                        const float batt_temp, const uint8_t batt_soc,
                        const uint8_t charging, const uint8_t vm_en) {
  // LED1: 错误状态
  if (s_batt_protect || s_chg_protect || s_servo_protect) {
    WS2812_SetColor(WS2812_LED_ERROR, WS2812_COLOR_RED);
  } else {
    WS2812_SetColor(WS2812_LED_ERROR, WS2812_COLOR_GREEN);
  }

  // LED2: 舵机供电温度
  if (!vm_en) {
    WS2812_SetColor(WS2812_LED_SERVO_TEMP, WS2812_COLOR_OFF);
  } else if (s_servo_protect) {
    WS2812_SetColor(WS2812_LED_SERVO_TEMP, WS2812_COLOR_RED);
  } else {
    WS2812_SetColor(WS2812_LED_SERVO_TEMP, temp_to_color(servo_power_temp));
  }

  // LED3: 充电电路温度
  if (!charging) {
    WS2812_SetColor(WS2812_LED_CHARGER_TEMP, WS2812_COLOR_OFF);
  } else if (s_chg_protect || s_batt_protect) {
    WS2812_SetColor(WS2812_LED_CHARGER_TEMP, WS2812_COLOR_RED);
  } else {
    WS2812_SetColor(WS2812_LED_CHARGER_TEMP, temp_to_color(charger_temp));
  }

  // LED4: 电池温度
  if (s_batt_protect) {
    WS2812_SetColor(WS2812_LED_BATT_TEMP, WS2812_COLOR_RED);
  } else {
    WS2812_SetColor(WS2812_LED_BATT_TEMP, batt_temp_to_color(batt_temp));
  }

  // LED5: 电池电量
  WS2812_SetColor(WS2812_LED_BATT_SOC, soc_to_color(batt_soc));

  // LED6: micro-ROS 连接状态
  if (g_uros_connected) {
    WS2812_SetColor(WS2812_LED_UROS_LINK, WS2812_COLOR_GREEN);
  } else {
    WS2812_SetColor(WS2812_LED_UROS_LINK, WS2812_COLOR_RED);
  }

  WS2812_Send();
}

void Task_LedIndication(void *argument) {
  (void)argument;

  WS2812_Init();

  /* 等待系统初始化 */
  osDelay(2000);

  for (;;) {
    // 舵机供电温度
    const float servo_power_temp = ADC_GetTemp1();
    // 充电电路温度
    const float charger_temp = ADC_GetTemp2();
    const float mcu_temp = ADC_GetMCUTemp();
    float batt_temp = 0;
    uint8_t batt_soc = 0;

    BQ40Z50_GetTemperatureC(&batt_temp);
    BQ40Z50_GetRelativeSOC(&batt_soc);

    const uint8_t charging = ADC_IsCharging();
    const uint8_t vm_en =
        (HAL_GPIO_ReadPin(VM_EN_GPIO_Port, VM_EN_Pin) == GPIO_PIN_SET);

    fan_ctrl(servo_power_temp, charger_temp, mcu_temp);
    overtemp_overtime_detect(servo_power_temp, charger_temp, batt_temp);
    // 执行保护动作
    do_protection();
    // 更新ws2812指示灯的颜色
    update_status_leds(servo_power_temp, charger_temp, batt_temp, batt_soc,
                       charging, vm_en);

    osDelay(WS2812_UPDATE_MS);
  }
}

void Task_CreateLedIndictionTask(void) {
  const osThreadAttr_t task_attr = {
      .name = "LEDIndicationTask",
      .stack_size = 1024 * 2, // 2KB - ADC读取 + I2C(BQ40Z50) + GPIO + WS2812更新，逻辑轻量
      .priority = (osPriority_t)osPriorityLow2,
  };
  taskLedIndictionHandle = osThreadNew(Task_LedIndication, NULL, &task_attr);
}
