//
// Created by greenhand520 on 2026/5/12.
//
// 所有自定义 FreeRTOS 任务逻辑放在此处，
// 避免 STM32CubeMX 重新生成代码时覆盖。

#pragma once

#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  外部任务句柄（main.c 中由 CubeMX 创建）
 * ================================================================ */
extern osThreadId_t ADCTaskHandle;

/* ================================================================
 *  各任务的主体函数声明
 *  CubeMX 在 main.c 中使用 StartADCTask / StartBQ24735ServiceTask 等名称
 *  这些函数的实现放在 app.c 中
 * ================================================================ */

/**
 * @brief ADC 采集与数据处理任务
 *        - 等待 DMA 完成（通过 osThreadFlags）
 *        - 调用 ADC_ProcessData() 转换物理量
 *        - 提供 UART/printf 输出（调试用）
 * @param argument: 未使用
 */
void App_ADCTask(void *argument);

/**
 * @brief MPU6500 IMU 数据采集任务
 *        - 等待 MPU_INT 中断（通过 osThreadFlags）
 *        - SPI DMA 读取 14 字节传感器数据
 *        - 处理原始数据并转换为物理量
 * @param argument: 未使用
 */
void App_IMUTask(void *argument);

#ifdef __cplusplus
}
#endif