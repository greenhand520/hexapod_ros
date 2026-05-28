//
// Created by greenhand520 on 2026/5/14.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

/* ================================================================
 *  充电电流调节参数
 * ================================================================ */
/** 最大充电电流 (mA) */
#define CHARGE_CURRENT_MAX_MA 10000.0
/** 最小充电电流 (mA) */
#define CHARGE_CURRENT_MIN_MA 500.0
/** 默认充电电压 (mV) */
#define CHARGE_VOLTAGE_DEFAULT 16800


/* ================================================================
 *  电池温度阈值 (°C)
 * ================================================================ */
/** 低于此温度停止充电 */
#define BAT_TEMP_COLD_LIMIT 0.0f
/** 低温限制 */
#define BAT_TEMP_COOL_LIMIT 10.0f
/** 开始降流 */
#define BAT_TEMP_WARM_LIMIT 45.0f
#define CHG_TEMP_WARM_LIMIT 70.0f
/** 停止充电 */
#define BAT_TEMP_HOT_LIMIT 50.0f
#define CHG_TEMP_HOT_LIMIT 80.0f

extern osThreadId_t taskBoardStateHandle;

void Task_BoardState(void *argument);

void Task_CreateBoardState(void);

#ifdef __cplusplus
}
#endif