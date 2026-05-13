//
// Created by greenhand520 on 2026/4/28.
//

#include "bq40z50.h"

/* BQ40Z50 7-bit I2C 地址 */
#define BQ40Z50_I2C_ADDR (0x0Bu)

/* 与 BQ24725 代码保持一致的地址格式（左移 1 位） */
#define BQ40Z50_ADDR_WRITE ((BQ40Z50_I2C_ADDR << 1) | 0)

/* I2C 传输超时 (ms) */
#define BQ40Z50_I2C_TIMEOUT (100u)

static BQ40Z50_Config *s_conf = NULL;
static bool s_initialized = false;

/* ================================================================
 *  I2C Word / Byte 底层读取
 *
 *  与 BQ24725 中的 SMBus_ReadWord / SMBus_WriteWord 风格一致
 * ================================================================ */

/**
 * @brief  I2C Read Word（2 字节，小端序）
 *         对应 SMBus Read Word 协议: [S][ADDR+W][CMD][Sr][ADDR+R][LB][HB][P]
 */
static int BQ40Z50_I2CReadWord(const uint8_t cmd, uint16_t *data) {
  uint8_t buf[2] = {0};

  const HAL_StatusTypeDef ret =
      HAL_I2C_Mem_Read(s_conf->hi2c, BQ40Z50_ADDR_WRITE, cmd,
                       I2C_MEMADD_SIZE_8BIT, buf, 2, BQ40Z50_I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return BQ40Z50_ERROR;
  }

  /* 小端序: buf[0]=LSB, buf[1]=MSB */
  *data = (uint16_t)((buf[1] << 8) | buf[0]);
  return BQ40Z50_OK;
}

/**
 * @brief  I2C Read Byte（1 字节）
 *         对应 SMBus Read Byte 协议: [S][ADDR+W][CMD][Sr][ADDR+R][DATA][P]
 */
static int BQ40Z50_I2CReadByte(const uint8_t cmd, uint8_t *data) {
  uint8_t buf[1] = {0};

  const HAL_StatusTypeDef ret =
      HAL_I2C_Mem_Read(s_conf->hi2c, BQ40Z50_ADDR_WRITE, cmd,
                       I2C_MEMADD_SIZE_8BIT, buf, 1, BQ40Z50_I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return BQ40Z50_ERROR;
  }

  *data = buf[0];
  return BQ40Z50_OK;
}

/* ================================================================
 *  带共享互斥锁的读取封装
 * ================================================================ */

static int BQ40Z50_ReadWord(const uint8_t cmd, uint16_t *data) {
  configASSERT(s_initialized);

  xSemaphoreTake(s_conf->bus_mutex, portMAX_DELAY);
  const int ret = BQ40Z50_I2CReadWord(cmd, data);
  xSemaphoreGive(s_conf->bus_mutex);
  return ret;
}

static int BQ40Z50_ReadByte(const uint8_t cmd, uint8_t *data) {
  configASSERT(s_initialized);

  xSemaphoreTake(s_conf->bus_mutex, portMAX_DELAY);
  const int ret = BQ40Z50_I2CReadByte(cmd, data);
  xSemaphoreGive(s_conf->bus_mutex);
  return ret;
}

/* ================================================================
 *  初始化
 * ================================================================ */

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
  /* 尝试读取 Voltage 寄存器来检测设备是否在线 */
  const int ret = BQ40Z50_ReadWord(BQ40Z50_CMD_VOLTAGE, &dummy);
  return (ret == BQ40Z50_OK);
}

/* ================================================================
 *  温度
 * ================================================================ */

int BQ40Z50_GetTemperatureC(float *temp_c) {
  uint16_t raw;
  const int ret = BQ40Z50_ReadWord(BQ40Z50_CMD_TEMPERATURE, &raw);
  if (ret != BQ40Z50_OK)
    return ret;

  /* 原始值单位 0.1K → 摄氏度 */
  *temp_c = (float)raw / 10.0f - 273.15f;
  return BQ40Z50_OK;
}

int BQ40Z50_GetTemperatureF(float *temp_f) {
  float c;
  const int ret = BQ40Z50_GetTemperatureC(&c);
  if (ret != BQ40Z50_OK)
    return ret;

  *temp_f = c * 9.0f / 5.0f + 32.0f;
  return BQ40Z50_OK;
}

/* ================================================================
 *  电压 / 电流
 * ================================================================ */

int BQ40Z50_GetVoltageMv(uint16_t *mv) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_VOLTAGE, mv);
}

int BQ40Z50_GetCurrentMa(int16_t *ma) {
  uint16_t raw;
  const int ret = BQ40Z50_ReadWord(BQ40Z50_CMD_CURRENT, &raw);
  if (ret != BQ40Z50_OK)
    return ret;

  /* 有符号值：正=放电，负=充电（SMBus 惯例） */
  *ma = (int16_t)raw;
  return BQ40Z50_OK;
}

int BQ40Z50_GetAverageCurrentMa(int16_t *ma) {
  uint16_t raw;
  const int ret = BQ40Z50_ReadWord(BQ40Z50_CMD_AVERAGE_CURRENT, &raw);
  if (ret != BQ40Z50_OK)
    return ret;

  *ma = (int16_t)raw;
  return BQ40Z50_OK;
}

/* ================================================================
 *  电量状态（Byte 读取）
 * ================================================================ */

int BQ40Z50_GetMaxError(uint8_t *percent) {
  return BQ40Z50_ReadByte(BQ40Z50_CMD_MAX_ERROR, percent);
}

int BQ40Z50_GetRelativeSOC(uint8_t *percent) {
  return BQ40Z50_ReadByte(BQ40Z50_CMD_RELATIVE_SOC, percent);
}

int BQ40Z50_GetAbsoluteSOC(uint8_t *percent) {
  return BQ40Z50_ReadByte(BQ40Z50_CMD_ABSOLUTE_SOC, percent);
}

/* ================================================================
 *  容量
 * ================================================================ */

int BQ40Z50_GetRemainingCapacityMah(uint16_t *mah) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_REMAINING_CAPACITY, mah);
}

int BQ40Z50_GetFullChargeCapacityMah(uint16_t *mah) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_FULL_CHARGE_CAPACITY, mah);
}

/* ================================================================
 *  时间
 * ================================================================ */

int BQ40Z50_GetRunTimeToEmptyMin(uint16_t *min) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_RUNTIME_TO_EMPTY, min);
}

int BQ40Z50_GetAvgTimeToEmptyMin(uint16_t *min) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_AVG_TIME_TO_EMPTY, min);
}

int BQ40Z50_GetAvgTimeToFullMin(uint16_t *min) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_AVG_TIME_TO_FULL, min);
}

/* ================================================================
 *  充电推荐值
 *
 *  Smart Battery 规范：BQ40Z50 通过这两个寄存器告诉充电器
 *  （BQ24725）应该以多大电流/电压充电。
 *  实际使用时可读取后写入 BQ24725 的 SetChargeCurrent / SetChargeVoltage。
 * ================================================================ */

int BQ40Z50_GetChargingCurrentMa(uint16_t *ma) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CHARGING_CURRENT, ma);
}

int BQ40Z50_GetChargingVoltageMv(uint16_t *mv) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CHARGING_VOLTAGE, mv);
}

/* ================================================================
 *  循环次数
 * ================================================================ */

int BQ40Z50_GetCycleCount(uint16_t *count) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CYCLE_COUNT, count);
}

/* ================================================================
 *  单体电压
 * ================================================================ */

int BQ40Z50_GetCellVoltage1Mv(uint16_t *mv) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CELL_VOLTAGE_1, mv);
}

int BQ40Z50_GetCellVoltage2Mv(uint16_t *mv) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CELL_VOLTAGE_2, mv);
}

int BQ40Z50_GetCellVoltage3Mv(uint16_t *mv) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CELL_VOLTAGE_3, mv);
}

int BQ40Z50_GetCellVoltage4Mv(uint16_t *mv) {
  return BQ40Z50_ReadWord(BQ40Z50_CMD_CELL_VOLTAGE_4, mv);
}
