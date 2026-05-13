//
// Created by eartholnpc on 2026/4/28.
//

#pragma once

/*
 * BQ40Z50.h
 *
 * STM32 FreeRTOS + I2C HAL driver for the BQ40Z50 Battery Manager
 *
 * 基于 SparkFun BQ40Z50 Arduino Library 移植
 * https://github.com/sparkfun/SparkFun_BQ40Z50_Battery_Manager_Arduino_Library
 *
 * 注意：BQ40Z50 与 BQ24725 共享同一条 I2C 总线，
 *       需要通过共享的 FreeRTOS 互斥锁进行总线仲裁。
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 *  BQ40Z50 SMBus 命令码
 * ================================================================ */

#define BQ40Z50_CMD_TEMPERATURE             0x08  /* Word, 0.1K */
#define BQ40Z50_CMD_VOLTAGE                 0x09  /* Word, mV */
#define BQ40Z50_CMD_CURRENT                 0x0A  /* Word, mA (signed) */
#define BQ40Z50_CMD_AVERAGE_CURRENT         0x0B  /* Word, mA (signed) */
#define BQ40Z50_CMD_MAX_ERROR               0x0C  /* Byte, % */
#define BQ40Z50_CMD_RELATIVE_SOC            0x0D  /* Byte, % */
#define BQ40Z50_CMD_ABSOLUTE_SOC            0x0E  /* Byte, % */
#define BQ40Z50_CMD_REMAINING_CAPACITY      0x0F  /* Word, mAh */
#define BQ40Z50_CMD_FULL_CHARGE_CAPACITY    0x10  /* Word, mAh */
#define BQ40Z50_CMD_RUNTIME_TO_EMPTY        0x11  /* Word, min */
#define BQ40Z50_CMD_AVG_TIME_TO_EMPTY       0x12  /* Word, min */
#define BQ40Z50_CMD_AVG_TIME_TO_FULL        0x13  /* Word, min */
#define BQ40Z50_CMD_CHARGING_CURRENT        0x14  /* Word, mA */
#define BQ40Z50_CMD_CHARGING_VOLTAGE        0x15  /* Word, mV */
#define BQ40Z50_CMD_CYCLE_COUNT             0x17  /* Word */
#define BQ40Z50_CMD_CELL_VOLTAGE_4          0x3C  /* Word, mV */
#define BQ40Z50_CMD_CELL_VOLTAGE_3          0x3D  /* Word, mV */
#define BQ40Z50_CMD_CELL_VOLTAGE_2          0x3E  /* Word, mV */
#define BQ40Z50_CMD_CELL_VOLTAGE_1          0x3F  /* Word, mV */

typedef struct {
    I2C_HandleTypeDef  *hi2c;       /* CubeMX 生成的 I2C 句柄（与 BQ24725 共用） */
    SemaphoreHandle_t   bus_mutex;  /* 共享总线互斥锁（与 BQ24725 共用同一个） */
} BQ40Z50_Config;

#define BQ40Z50_OK      0
#define BQ40Z50_ERROR  (-1)

/* ================================================================
 *  API
 * ================================================================ */

bool BQ40Z50_Init(BQ40Z50_Config *conf);
bool BQ40Z50_IsConnected(void);

/* 温度 */
int BQ40Z50_GetTemperatureC(float *temp_c);
int BQ40Z50_GetTemperatureF(float *temp_f);

/* 电压 / 电流 */
int BQ40Z50_GetVoltageMv(uint16_t *mv);
int BQ40Z50_GetCurrentMa(int16_t *ma);
int BQ40Z50_GetAverageCurrentMa(int16_t *ma);

/* 电量状态 */
int BQ40Z50_GetMaxError(uint8_t *percent);
int BQ40Z50_GetRelativeSOC(uint8_t *percent);
int BQ40Z50_GetAbsoluteSOC(uint8_t *percent);

/* 容量 */
int BQ40Z50_GetRemainingCapacityMah(uint16_t *mah);
int BQ40Z50_GetFullChargeCapacityMah(uint16_t *mah);

/* 时间 */
int BQ40Z50_GetRunTimeToEmptyMin(uint16_t *min);
int BQ40Z50_GetAvgTimeToEmptyMin(uint16_t *min);
int BQ40Z50_GetAvgTimeToFullMin(uint16_t *min);

/* 充电推荐值（Smart Battery → 告诉充电器该用什么电流/电压） */
int BQ40Z50_GetChargingCurrentMa(uint16_t *ma);
int BQ40Z50_GetChargingVoltageMv(uint16_t *mv);

/* 循环次数 */
int BQ40Z50_GetCycleCount(uint16_t *count);

/* 单体电压 */
int BQ40Z50_GetCellVoltage1Mv(uint16_t *mv);
int BQ40Z50_GetCellVoltage2Mv(uint16_t *mv);
int BQ40Z50_GetCellVoltage3Mv(uint16_t *mv);
int BQ40Z50_GetCellVoltage4Mv(uint16_t *mv);

