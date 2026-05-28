//
// Created by greenhand520 on 2026/4/28.
//
/**
 * STM32 FreeRTOS + I2C HAL driver for the BQ40Z50 Battery Manager
 * 寄存器参考https://www.ti.com.cn/product/zh-cn/BQ40Z50-R1
 * 注意：BQ40Z50 与 BQ24725 共享同一条 I2C 总线，需要通过共享的 FreeRTOS
 * 互斥锁进行总线仲裁
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "semphr.h"

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

/** BQ40Z50 7-bit I2C 地址 */
#define BQ40Z50_ADDR (0x0Bu << 1)

/* ================================================================
 *  BQ40Z50 SMBus 命令码
 * ================================================================ */
/** BQ40Z50的序列号 */
#define BQ40Z50_CMD_SERIAL 0x01
/** 综合电池温度，单位 0.1°K，temperature = Temperature() / 10.0 - 273.15 */
#define BQ40Z50_CMD_TEMPERATURE 0x08 /* Word, 0.1K */
/** 所有电芯电压之和，单位 mV */
#define BQ40Z50_CMD_VOLTAGE 0x09 /* Word, mV */
/** 库仑计测量的电流，单位 mA，有符号 */
#define BQ40Z50_CMD_CURRENT 0x0A         /* Word, mA (signed) */
#define BQ40Z50_CMD_AVERAGE_CURRENT 0x0B /* Word, mA (signed) */
#define BQ40Z50_CMD_MAX_ERROR 0x0C       /* Byte, % */
#define BQ40Z50_CMD_RELATIVE_SOC 0x0D    /* Byte, % */
#define BQ40Z50_CMD_ABSOLUTE_SOC 0x0E    /* Byte, % */
/**
 * 预测的剩余电池容量。当 BatteryMode[CAPM]=0 时单位为 mAh，当 CAPM=1 时单位为
 * 10 mWh
 */
#define BQ40Z50_CMD_REMAINING_CAPACITY 0x0F /* Word, mAh */
/**
 * 电池充满时的预测容量。充电过程中不会更新。当 CAPM=0 时单位 mAh，CAPM=1
 * 时单位 10 mW
 */
#define BQ40Z50_CMD_FULL_CHARGE_CAPACITY 0x10 /* Word, mAh */
#define BQ40Z50_CMD_RUNTIME_TO_EMPTY 0x11     /* Word, min */
#define BQ40Z50_CMD_AVG_TIME_TO_EMPTY 0x12    /* Word, min */
#define BQ40Z50_CMD_AVG_TIME_TO_FULL 0x13     /* Word, min */
#define BQ40Z50_CMD_CHARGING_CURRENT 0x14     /* Word, mA */
#define BQ40Z50_CMD_CHARGING_VOLTAGE 0x15     /* Word, mV */
/**
 * 电池状态
 * (Bit 6)   0	 充电模式 1	放电或休眠模式
 * (Bit 5)   1	 已充满
 * (Bit 4)   1	 已完全放电
 * (Bit 11)  1	 终止放电报警
 * (Bit 12)  1	 过温报警
 * (Bit 15)  1	 过充报警
 */
#define BA40Z50_CMD_BATTERY_STATUS 0x16
/**
 * 电池的理论（设计）容量。默认值 4400 mAh / 6336 cWh。当 CAPM=0 时单位
 * mAh，CAPM=1 时单位 10 mWh
 */
#define BQ40Z50_CMD_DESIGN_CAPACITY 0x18
#define BQ40Z50_CMD_CYCLE_COUNT 0x17 /* Word */
/** 返回电池包的序列号，默认 0x0001，需将十六进制值转为字符串格式 */
#define BQ40Z50_CMD_BAT_SERIAL 0x1c
/** 电池化学类型字符串，默认 "LION" */
#define BQ40Z50_CMD_DEVICE_CHEMISTRY 0x22
#define BQ40Z50_CMD_CELL_VOLTAGE_4 0x3C /* Word, mV */
#define BQ40Z50_CMD_CELL_VOLTAGE_3 0x3D /* Word, mV */
#define BQ40Z50_CMD_CELL_VOLTAGE_2 0x3E /* Word, mV */
#define BQ40Z50_CMD_CELL_VOLTAGE_1 0x3F /* Word, mV */

#define BQ40Z50_CMD_MANUFACTURER_ACCESS 0x00
#define BA40Z50_CMD_MANUFACTURER_BLOCK_ACCESS 0x44
// DAStatus2 子命令
#define BQ40Z50_SUBCMD_DASTATUS2 0x0072

// typedef enum
// {
//   UNKNOWN = 0U,
//   NIMH = 1,
//   LION = 2,
//   LIPO = 3,
//   LIFE = 4,
//   NICD = 5,
//   LIMN = 6
// } BQ40Z50_BatteryChemistry;

typedef struct {
  /** CubeMX 生成的 I2C 句柄（与 BQ24725 共用） */
  I2C_HandleTypeDef *hi2c;
  /** 共享总线互斥锁（与 BQ24725 共用同一个） */
  SemaphoreHandle_t bus_mutex;
} BQ40Z50_Config;

typedef struct {
  /** 内部温度 (°C) */
  float int_temp;
  /**  TS1 (C) */
  float ts1_temp;
  /** TS2 (°C) */
  float ts2_temp;
  /** TS3 (°C) */
  float ts3_temp;
  /** TS4 (°C) */
  float ts4_temp;
  /** 电芯温度 (°C) */
  float cell_temp;
  /** FET温度 (°C) */
  float fet_temp;
  /** 计算温度 (°C) */
  float gauging_temp;
} BQ40Z50_TempDetail;

bool BQ40Z50_Init(BQ40Z50_Config *conf);
bool BQ40Z50_IsConnected(void);

ErrorStatus BQ40Z50_GetSerial(uint16_t *serial_number);
ErrorStatus BQ40Z50_GetTemperatureC(float *temp_c);
ErrorStatus BQ40Z50_GetTemperatureF(float *temp_f);

/**
 * @brief 读取 DAStatus2（4路NTC温度）
 * @param temp 温度数据结构体指针
 * @return HAL_StatusTypeDef
 */
ErrorStatus BQ40Z50_GetDAStatus2(BQ40Z50_TempDetail *temp);

ErrorStatus BQ40Z50_GetVoltageMv(uint16_t *mv);
ErrorStatus BQ40Z50_GetCurrentMa(int16_t *ma);
ErrorStatus BQ40Z50_GetAverageCurrentMa(int16_t *ma);

ErrorStatus BQ40Z50_GetMaxError(uint8_t *percent);
ErrorStatus BQ40Z50_GetRelativeSOC(uint8_t *percent);
ErrorStatus BQ40Z50_GetAbsoluteSOC(uint8_t *percent);

ErrorStatus BQ40Z50_GetRemainingCapacityMah(uint16_t *mah);
ErrorStatus BQ40Z50_GetFullChargeCapacityMah(uint16_t *mah);

ErrorStatus BQ40Z50_GetRunTimeToEmptyMin(uint16_t *min);
ErrorStatus BQ40Z50_GetAvgTimeToEmptyMin(uint16_t *min);
ErrorStatus BQ40Z50_GetAvgTimeToFullMin(uint16_t *min);

/**
 * @brief 充电电流推荐
 * @param ma 推荐的充电电流
 */
ErrorStatus BQ40Z50_GetChargingCurrentMa(uint16_t *ma);

/**
 * @brief 充电电压推荐
 * @param mv 锐减的充电电压
 */
ErrorStatus BQ40Z50_GetChargingVoltageMv(uint16_t *mv);

/**
 * 循环次数
 * @param count 循环次数
 */
ErrorStatus BQ40Z50_GetCycleCount(uint16_t *count);

/* ================================================================
 *  单体电压
 * ================================================================ */
ErrorStatus BQ40Z50_GetCellVoltage1Mv(uint16_t *mv);
ErrorStatus BQ40Z50_GetCellVoltage2Mv(uint16_t *mv);
ErrorStatus BQ40Z50_GetCellVoltage3Mv(uint16_t *mv);
ErrorStatus BQ40Z50_GetCellVoltage4Mv(uint16_t *mv);

ErrorStatus BQ40Z50_GetChemistry(uint16_t *chemistry);

#ifdef __cplusplus
}
#endif