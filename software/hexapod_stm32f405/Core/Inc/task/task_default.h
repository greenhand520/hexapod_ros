//
// Created by eartholnpc on 2026/5/28.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

extern osThreadId_t taskDefaultHandle;

/**
 * @brief 系统初始化任务（DefaultTask）
 *
 *  执行顺序:
 *    1. 创建 I2C 互斥锁
 *    2. 初始化 BQ24725 充电管理
 *    3. 初始化 BQ40Z50 电池管理
 *    4. 初始化 micro-ROS（UART5 自定义传输 + rclc node/publishers）
 *    5. 播放开机音效 + 打开舵机供电 (VM_EN)
 *    6. 初始化消息实例
 *    7. 启动 ADC DMA
 *    8. 进入 micro-ROS executor spin 循环
 *
 * @param argument: 未使用
 */
void Task_default(void *argument);

void Task_CreateDefaultTask(void);

#ifdef __cplusplus
}
#endif