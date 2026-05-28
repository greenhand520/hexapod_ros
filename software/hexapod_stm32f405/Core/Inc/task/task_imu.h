//
// Created by greenhand520 on 2026/5/14.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

extern osThreadId_t taskIMUHandle;

/**
 * @brief MPU6500 IMU 采集任务
 *        DMA 完成后处理数据并通过 /sensor/imu 发布
 *        ROS2 标准: 加速度 m/s², 角速度 rad/s
 * @param argument
 */
void Task_IMU(void *argument);

void Task_CreateIMUTask(void);

#ifdef __cplusplus
}
#endif
