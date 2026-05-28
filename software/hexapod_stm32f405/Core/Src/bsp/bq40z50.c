//
// Created by greenhand520 on 2026/4/28.
//

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"

#include "bsp/bq40z50.h"
#include "bsp/i2c.h"

static BQ40Z50_Config *s_conf = NULL;
static bool s_initialized = false;

bool BQ40Z50_Init(BQ40Z50_Config *conf) {
  configASSERT(conf);
  configASSERT(conf->hi2c);
  configASSERT(conf->bus_mutex);

  s_conf = conf;
  s_initialized = true;

  return BQ40Z50_IsConnected();
}

bool BQ40Z50_IsConnected(void) {
  uint16_t dummy;
  // 尝试读取 Voltage 寄存器来检测设备是否在线
  const int ret =
      I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_VOLTAGE, &dummy);
  return (ret == HAL_OK);
}

/**
 * @brief 将0.1°K转换为°C
 */
float KelvinToCelsius(const uint16_t temp_0_1k) {
  return ((float)temp_0_1k / 10.0f) - 273.15f;
}

ErrorStatus BQ40Z50_GetSerial(uint16_t *serial_number) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_SERIAL, serial_number);
}

ErrorStatus BQ40Z50_GetTemperatureC(float *temp_c) {
  uint16_t raw;
  const int ret =
      I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_TEMPERATURE, &raw);
  if (ret != SUCCESS) {
    return ret;
  }

  // 原始值单位 0.1K → 摄氏度
  *temp_c = KelvinToCelsius(raw);
  return SUCCESS;
}

ErrorStatus BQ40Z50_GetTemperatureF(float *temp_f) {
  float c;
  const int ret = BQ40Z50_GetTemperatureC(&c);
  if (ret != SUCCESS) {
    return ret;
  }

  *temp_f = c * 9.0f / 5.0f + 32.0f;
  return SUCCESS;
}

ErrorStatus BQ40Z50_GetDAStatus2(BQ40Z50_TempDetail *temp) {
  // Step 1: 向 ManufacturerAccess(0x00) 写入子命令 0x0072
  // 子命令以小端序发送：0x72, 0x00

  xSemaphoreTake(g_i2c1_mutex, portMAX_DELAY);
  const ErrorStatus et = I2C1_WriteWord(
      BQ40Z50_ADDR, BQ40Z50_CMD_MANUFACTURER_ACCESS, BQ40Z50_SUBCMD_DASTATUS2);
  if (et != SUCCESS) {
    return et;
  }
  // 等待芯片准备数据
  osDelay(10);
  // 预留足够缓冲区
  uint8_t rx_data[32];

  // Step 2: 从 ManufacturerBlockAccess(0x44) 读取 Block 数据
  const uint8_t block_len = 16; // R2版本读16字节，R4版本可读24字节

  const HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c1, BQ40Z50_ADDR, BA40Z50_CMD_MANUFACTURER_BLOCK_ACCESS,
      I2C_MEMADD_SIZE_8BIT, rx_data, block_len, 1000);
  if (status != HAL_OK)
    return ERROR;

  // 解析数据（小端模式）
  temp->int_temp = KelvinToCelsius((int16_t)(rx_data[0] | (rx_data[1] << 8)));
  temp->ts1_temp = KelvinToCelsius((int16_t)(rx_data[2] | (rx_data[3] << 8)));
  temp->ts2_temp = KelvinToCelsius((int16_t)(rx_data[4] | (rx_data[5] << 8)));
  temp->ts3_temp = KelvinToCelsius((int16_t)(rx_data[6] | (rx_data[7] << 8)));
  temp->ts4_temp = KelvinToCelsius((int16_t)(rx_data[8] | (rx_data[9] << 8)));
  temp->cell_temp = KelvinToCelsius((int16_t)(rx_data[10] | (rx_data[11] << 8)));
  temp->fet_temp = KelvinToCelsius((int16_t)(rx_data[12] | (rx_data[13] << 8)));
  temp->gauging_temp = KelvinToCelsius((int16_t)(rx_data[14] | (rx_data[15] << 8)));

  xSemaphoreGive(g_i2c1_mutex);
  return SUCCESS;
}

ErrorStatus BQ40Z50_GetVoltageMv(uint16_t *mv) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_VOLTAGE, mv);
}

ErrorStatus BQ40Z50_GetCurrentMa(int16_t *ma) {
  uint16_t raw;
  const int ret = I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CURRENT, &raw);
  if (ret != SUCCESS) {
    return ret;
  }

  // 有符号值：正=放电，负=充电（SMBus 惯例）
  *ma = (int16_t)raw;
  return SUCCESS;
}

ErrorStatus BQ40Z50_GetAverageCurrentMa(int16_t *ma) {
  uint16_t raw;
  const int ret =
      I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_AVERAGE_CURRENT, &raw);
  if (ret != SUCCESS) {
    return ret;
  }

  *ma = (int16_t)raw;
  return SUCCESS;
}

ErrorStatus BQ40Z50_GetMaxError(uint8_t *percent) {
  return I2C1_Mutex_ReadByte(BQ40Z50_ADDR, BQ40Z50_CMD_MAX_ERROR, percent);
}

ErrorStatus BQ40Z50_GetRelativeSOC(uint8_t *percent) {
  return I2C1_Mutex_ReadByte(BQ40Z50_ADDR, BQ40Z50_CMD_RELATIVE_SOC, percent);
}

ErrorStatus BQ40Z50_GetAbsoluteSOC(uint8_t *percent) {
  return I2C1_Mutex_ReadByte(BQ40Z50_ADDR, BQ40Z50_CMD_ABSOLUTE_SOC, percent);
}

ErrorStatus BQ40Z50_GetRemainingCapacityMah(uint16_t *mah) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_REMAINING_CAPACITY, mah);
}

ErrorStatus BQ40Z50_GetFullChargeCapacityMah(uint16_t *mah) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_FULL_CHARGE_CAPACITY,
                             mah);
}

ErrorStatus BQ40Z50_GetRunTimeToEmptyMin(uint16_t *min) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_RUNTIME_TO_EMPTY, min);
}

ErrorStatus BQ40Z50_GetAvgTimeToEmptyMin(uint16_t *min) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_AVG_TIME_TO_EMPTY, min);
}

ErrorStatus BQ40Z50_GetAvgTimeToFullMin(uint16_t *min) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_AVG_TIME_TO_FULL, min);
}

ErrorStatus BQ40Z50_GetChargingCurrentMa(uint16_t *ma) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CHARGING_CURRENT, ma);
}

ErrorStatus BQ40Z50_GetChargingVoltageMv(uint16_t *mv) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CHARGING_VOLTAGE, mv);
}

ErrorStatus BQ40Z50_GetCycleCount(uint16_t *count) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CYCLE_COUNT, count);
}

ErrorStatus BQ40Z50_GetCellVoltage1Mv(uint16_t *mv) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CELL_VOLTAGE_1, mv);
}

ErrorStatus BQ40Z50_GetCellVoltage2Mv(uint16_t *mv) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CELL_VOLTAGE_2, mv);
}

ErrorStatus BQ40Z50_GetCellVoltage3Mv(uint16_t *mv) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CELL_VOLTAGE_3, mv);
}

ErrorStatus BQ40Z50_GetCellVoltage4Mv(uint16_t *mv) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_CELL_VOLTAGE_4, mv);
}

ErrorStatus BQ40Z50_GetChemistry(uint16_t *chemistry) {
  return I2C1_Mutex_ReadWord(BQ40Z50_ADDR, BQ40Z50_CMD_DEVICE_CHEMISTRY, chemistry);
}
