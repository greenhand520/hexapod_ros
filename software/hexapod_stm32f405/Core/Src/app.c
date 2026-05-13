//
// Created by greenhand520 on 2026/5/12.
//
// 自定义 FreeRTOS 任务实现。
// 放在此文件中以避免 STM32CubeMX 重新生成 main.c 时覆盖用户代码。

#include "app.h"
#include "adc.h"
#include "mpu6500.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <stdio.h>

/* ================================================================
 *  ADC 采集任务
 *
 *  工作流程:
 *    1. 启动 ADC DMA + TIM2 触发（仅首次）
 *    2. 等待 DMA 完成中断唤醒（通过 osThreadFlags）
 *    3. 调用 ADC_ProcessData() 将原始值转换为物理量
 *    4. （可选）通过 UART 输出调试信息
 *    5. 循环等待下一次 DMA 完成
 *
 *  采集频率由 TIM2 决定：
 *    TIM2 触发频率 = 84MHz / (8399+1) / (2499+1) ≈ 4 Hz
 * ================================================================ */

void App_ADCTask(void *argument) {
    (void)argument;

    /* 启动 ADC DMA + TIM2 触发 */
    ADC_Start();

    for (;;) {
        /* 阻塞等待 DMA 完成标志（由 HAL_ADC_ConvCpltCallback 设置） */
        osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);

        /* 将 DMA 缓冲区中的原始值转换为物理量 */
        ADC_ProcessData();

        /* ---- 调试输出（可通过 #if 0 关闭） ---- */
#if 0
        {
            float temp1   = ADC_GetTemp1();
            float temp2   = ADC_GetTemp2();
            float svm     = ADC_GetSVM();
            float cv      = ADC_GetCV();
            float imon    = ADC_GetBC_IOUT_mA();
            float mcu_tmp = ADC_GetMCUTemp();
            printf("TEMP1=%.1fC TEMP2=%.1fC SVM=%.2fV CV=%.2fV IMON=%.1fmA MCU=%.1fC\r\n",
                   temp1, temp2, svm, cv, imon, mcu_tmp);
        }
#endif
    }
}

/* ================================================================
 *  MPU6500 IMU 采集任务
 *
 *  工作流程:
 *    1. 初始化 MPU6500（SPI, 量程, DLPF, 采样率）
 *    2. 等待 MPU_INT 中断唤醒（通过 osThreadFlags 0x02）
 *    3. SPI DMA 读取 14 字节原始数据
 *    4. DMA 完成回调中自动设置标志
 *    5. 处理原始数据并转换为物理量
 *
 *  采样率: 1kHz / (1 + sample_rate_divider)
 *  默认: 1kHz / (1 + 9) = 100Hz
 * ================================================================ */

void App_IMUTask(void *argument) {
    (void)argument;

    /* MPU6500 配置 */
    extern SPI_HandleTypeDef hspi1;

    static const MPU6500_Config_t imu_cfg = {
        .hspi                = &hspi1,
        .CS_GPIO_Port        = CS1_MPU_GPIO_Port,
        .CS_GPIO_Pin         = CS1_MPU_Pin,
        .accel_range         = MPU6500_ACCEL_RANGE_4G,
        .gyro_range          = MPU6500_GYRO_RANGE_2000DPS,
        .dlpf                = MPU6500_DLPF_41HZ,
        .sample_rate_divider = 9,   /* 1kHz / 10 = 100Hz */
    };

    /* 初始化 MPU6500 */
    const int ret = MPU6500_Init(&imu_cfg);
    if (ret != MPU6500_OK) {
        /* 初始化失败，挂起任务 */
        for (;;) {
            osDelay(osWaitForever);
        }
    }

    /* 首次触发 DMA 读取 */
    MPU6500_ReadAllDMA();

    for (;;) {
        /* 阻塞等待 MPU_INT 中断或 DMA 完成标志 */
        osThreadFlagsWait(0x02, osFlagsWaitAny, osWaitForever);

        /* DMA 传输完成后处理数据 */
        MPU6500_ProcessData();

        /* 触发下一次 DMA 读取 */
        MPU6500_ReadAllDMA();

        /* ---- 调试输出（可通过 #if 0 关闭） ---- */
#if 0
        {
            float ax = MPU6500_GetAccelX() / 8192.0f;
            float ay = MPU6500_GetAccelY() / 8192.0f;
            float az = MPU6500_GetAccelZ() / 8192.0f;
            float gx = MPU6500_GetGyroX() / 16.4f;
            float gy = MPU6500_GetGyroY() / 16.4f;
            float gz = MPU6500_GetGyroZ() / 16.4f;
            float t  = MPU6500_GetTemperature();
            printf("IMU A:%.2f %.2f %.2f G:%.1f %.1f %.1f T:%.1fC\r\n",
                   ax, ay, az, gx, gy, gz, t);
        }
#endif
    }
}
