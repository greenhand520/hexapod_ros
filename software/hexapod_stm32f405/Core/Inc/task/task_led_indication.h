//
// Created by greenhand520 on 2026/5/14.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

// 125ms ≈ 8Hz, 与 ADC 同步
#define WS2812_UPDATE_MS 125

/* ================================================================
 *  LED 索引定义
 *
 *  灯1: 错误状态 (绿色=正常, 纯红=错误)
 *  灯2: 舵机供电温度 (灭=VM_EN关闭, 绿/橙/警告红=温度)
 *  灯3: 充电电路温度 (灭=未充电, 绿/橙/警告红=温度)
 *  灯4: 电池温度 (蓝=过冷, 绿/橙/警告红=温度)
 *  灯5: 电池电量 (绿/浅绿/橙/警告红)
 *  灯6: micro-ROS 连接状态 (绿=已连接, 纯红=未连接)
 * ================================================================ */
#define WS2812_LED_ERROR 0
#define WS2812_LED_SERVO_TEMP 1
#define WS2812_LED_CHARGER_TEMP 2
#define WS2812_LED_BATT_TEMP 3
#define WS2812_LED_BATT_SOC 4
#define WS2812_LED_UROS_LINK 5

/** 风扇开启温度 (°C) */
#define TEMP_FAN_ON_C 45.0f
/** 风扇关闭温度 (°C, 迟滞) */
#define TEMP_FAN_OFF_C 40.0f
/** LED 橙色阈值 */
#define TEMP_WARN_C 40.0f
/** LED 红色阈值 */
#define TEMP_DANGER_C 55.0f
/** 电池低温蓝色阈值 */
#define TEMP_BATT_COLD_C 10.0f
/** 电池过冷蓝色 (排除传感器异常) */
#define TEMP_BATT_COLD_WARN_C (-5.0f)

/** ADC 8Hz → 2分钟 = 960 次采样，过温持续 2min 后触发保护 */
#define OVERTEMP_TIMEOUT_TICKS 240

/**
 * @brief WS2812指示灯的更新任务与风扇控制
 *        每 125ms 更新一次 (与 ADC 8Hz 同步)
 *        风扇控制逻辑：
 *         温度持续 > 55°C 超过 2min → 关闭充电/舵机供电
 *         温度 > 45°C → 开启风扇 (FAN_EN=HIGH)、显示警告红
 *         温度 < 40°C → 关闭风扇
 */
void Task_LedIndication(void *argument);

extern osThreadId_t taskLedIndictionHandle;

void Task_CreateLedIndictionTask(void);

#ifdef __cplusplus
}
#endif