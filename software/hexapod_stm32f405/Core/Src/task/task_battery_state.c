//
// Created by greenhand520 on 2026/5/14.
//

#include "log.h"

#include "rcl/publisher.h"
#include "sensor_msgs/msg/battery_state.h"
#include "task/task_battery_state.h"
#include "task/task_board_state.h"

volatile BQ40Z50_SharedData g_bq40z50_data = {0};
static float bat_cells_voltage[4];
static float bat_cells_temp[4];
static char serial_buf[16];

extern SemaphoreHandle_t g_i2c1_mutex;
extern sensor_msgs__msg__BatteryState s_msg_batt;
extern volatile uint8_t g_uros_connected;
extern rcl_publisher_t battery_state_pub;



void Task_BatteryState(void *argument) {
  (void)argument;
  // 等待 DefaultTask 初始化完成
  osDelay(3000);

  for (;;) {
    if (g_i2c1_mutex != NULL) {
      float temp = 0;
      uint16_t voltage = 0, remaining = 0, fcc = 0, cy = 0;
      int16_t current = 0;
      uint8_t soc = 0;
      uint16_t vc1 = 0, vc2 = 0, vc3 = 0, vc4 = 0, serial = 0;
      BQ40Z50_TempDetail temp_detail;

      BQ40Z50_GetTemperatureC(&temp);
      BQ40Z50_GetVoltageMv(&voltage);
      BQ40Z50_GetCurrentMa(&current);
      BQ40Z50_GetRelativeSOC(&soc);
      BQ40Z50_GetRemainingCapacityMah(&remaining);
      BQ40Z50_GetFullChargeCapacityMah(&fcc);
      BQ40Z50_GetCycleCount(&cy);
      BQ40Z50_GetCellVoltage1Mv(&vc1);
      BQ40Z50_GetCellVoltage2Mv(&vc2);
      BQ40Z50_GetCellVoltage3Mv(&vc3);
      BQ40Z50_GetCellVoltage4Mv(&vc4);
      BQ40Z50_GetSerial(&serial);
      BQ40Z50_GetDAStatus2(&temp_detail);

      g_bq40z50_data.serial = serial;
      g_bq40z50_data.temp_c = temp;
      g_bq40z50_data.voltage_mv = voltage;
      g_bq40z50_data.current_ma = current;
      g_bq40z50_data.soc = soc;
      g_bq40z50_data.remaining_mah = remaining;
      g_bq40z50_data.full_cap_mah = fcc;
      g_bq40z50_data.cycle_count = cy;
      g_bq40z50_data.cell1_mv = vc1;
      g_bq40z50_data.cell2_mv = vc2;
      g_bq40z50_data.cell3_mv = vc3;
      g_bq40z50_data.cell4_mv = vc4;
      g_bq40z50_data.present = BQ40Z50_IsConnected();
      g_bq40z50_data.temp_detail = temp_detail;

      const int len = snprintf(serial_buf, sizeof(serial_buf),
                             "BQ40Z50_%04X", serial);
      s_msg_batt.serial_number.data = serial_buf;
      s_msg_batt.serial_number.size = len;
      s_msg_batt.serial_number.capacity = sizeof(serial_buf);

      if (g_bq40z50_data.present) {
        s_msg_batt.present = 1;
        s_msg_batt.voltage = (float)voltage / 1000.0f;
        s_msg_batt.temperature = temp;
        s_msg_batt.current = (float)current / 1000.0f;
        // 当前剩余电量
        s_msg_batt.charge = (float)remaining / 1000.0f;
        // 电池当前满充容量
        s_msg_batt.capacity = (float)fcc / 1000.0f;
        s_msg_batt.design_capacity = s_msg_batt.capacity;
        // 电量百分比
        s_msg_batt.percentage = (float)soc / 100.0f;

        s_msg_batt.power_supply_status =
            sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_GOOD;
        if (current < -100) {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_DISCHARGING;
        } else if (current > 100) {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_CHARGING;
        } else {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_NOT_CHARGING;
        }

        if (soc >= 100) {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_FULL;
        }

        if (temp >= BAT_TEMP_HOT_LIMIT) {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_OVERHEAT;
        } else if (temp <= BAT_TEMP_COLD_LIMIT) {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_COLD;
        }

        if (voltage >= CHARGE_VOLTAGE_DEFAULT + 200) {
          s_msg_batt.power_supply_status =
              sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        }

        bat_cells_voltage[0] = (float)vc1 / 1000;
        bat_cells_voltage[1] = (float)vc2 / 1000;
        bat_cells_voltage[2] = (float)vc3 / 1000;
        bat_cells_voltage[3] = (float)vc4 / 1000;
        s_msg_batt.cell_voltage.data = bat_cells_voltage;
        s_msg_batt.cell_voltage.size = 4;
        s_msg_batt.cell_voltage.capacity = 4;

        bat_cells_temp[0] = temp_detail.ts1_temp;
        bat_cells_temp[1] = temp_detail.ts2_temp;
        bat_cells_temp[2] = temp_detail.ts3_temp;
        bat_cells_temp[3] = temp_detail.ts4_temp;
        s_msg_batt.cell_temperature.data = bat_cells_temp;
        s_msg_batt.cell_temperature.size = 4;
        s_msg_batt.cell_temperature.capacity = 4;
        // 默认锂离子电池
        s_msg_batt.power_supply_technology = 2;
      } else {
        // 电池不存在
        s_msg_batt.power_supply_status = sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_UNKNOWN;
        s_msg_batt.present = 0;
      }

      if (g_uros_connected) {
        const rcl_ret_t rcl_ret =
            rcl_publish(&battery_state_pub, &s_msg_batt, NULL);
        if (rcl_ret != RCL_RET_OK) {
          LOG_ERR("Failed to publish battery state with error %d", rcl_ret);
        }
      }
    }
    osDelay(BQ40Z50_READ_INTERVAL_MS);
  }
}

void Task_CreateBatteryStateTask(void) {
  const osThreadAttr_t task_attr = {
      .name = "BQ40Z50Task",
      .stack_size = 1024 * 3, // 3KB - 多次 I2C 通信 + 消息填充 + publish
      .priority = (osPriority_t)osPriorityNormal4,
  };
  taskBatteryStateHandle = osThreadNew(Task_BatteryState, NULL, &task_attr);
}
