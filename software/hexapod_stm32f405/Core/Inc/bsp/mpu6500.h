//
// Created by greenhand520 on 2026/5/12.
//
// MPU6500 6-axis IMU driver (SPI + DMA, FreeRTOS)
// Datasheet: MPU-6500 Product Specification Rev 1.3
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#include <stdint.h>

/* ================================================================
 *  MPU6500 寄存器地址
 * ================================================================ */
#define MPU6500_REG_SELF_TEST_X_GYRO 0x00
#define MPU6500_REG_SELF_TEST_Y_GYRO 0x01
#define MPU6500_REG_SELF_TEST_Z_GYRO 0x02
#define MPU6500_REG_SELF_TEST_X_ACCEL 0x0D
#define MPU6500_REG_SELF_TEST_Y_ACCEL 0x0E
#define MPU6500_REG_SELF_TEST_Z_ACCEL 0x0F
#define MPU6500_REG_XG_OFFSET_H 0x13
#define MPU6500_REG_XG_OFFSET_L 0x14
#define MPU6500_REG_YG_OFFSET_H 0x15
#define MPU6500_REG_YG_OFFSET_L 0x16
#define MPU6500_REG_ZG_OFFSET_H 0x17
#define MPU6500_REG_ZG_OFFSET_L 0x18
#define MPU6500_REG_SMPLRT_DIV 0x19
#define MPU6500_REG_CONFIG 0x1A
#define MPU6500_REG_GYRO_CONFIG 0x1B
#define MPU6500_REG_ACCEL_CONFIG 0x1C
#define MPU6500_REG_ACCEL_CONFIG_2 0x1D
#define MPU6500_REG_LP_ACCEL_ODR 0x1E
#define MPU6500_REG_WOM_THR 0x1F
#define MPU6500_REG_FIFO_EN 0x23
#define MPU6500_REG_I2C_MST_CTRL 0x24
#define MPU6500_REG_I2C_SLV0_ADDR 0x25
#define MPU6500_REG_I2C_SLV0_REG 0x26
#define MPU6500_REG_I2C_SLV0_CTRL 0x27
#define MPU6500_REG_I2C_SLV1_ADDR 0x28
#define MPU6500_REG_I2C_SLV1_REG 0x29
#define MPU6500_REG_I2C_SLV1_CTRL 0x2A
#define MPU6500_REG_I2C_SLV2_ADDR 0x2B
#define MPU6500_REG_I2C_SLV2_REG 0x2C
#define MPU6500_REG_I2C_SLV2_CTRL 0x2D
#define MPU6500_REG_I2C_SLV3_ADDR 0x2E
#define MPU6500_REG_I2C_SLV3_REG 0x2F
#define MPU6500_REG_I2C_SLV3_CTRL 0x30
#define MPU6500_REG_I2C_SLV4_ADDR 0x31
#define MPU6500_REG_I2C_SLV4_REG 0x32
#define MPU6500_REG_I2C_SLV4_DO 0x33
#define MPU6500_REG_I2C_SLV4_CTRL 0x34
#define MPU6500_REG_I2C_SLV4_DI 0x35
#define MPU6500_REG_I2C_MST_STATUS 0x36
#define MPU6500_REG_INT_PIN_CFG 0x37
#define MPU6500_REG_INT_ENABLE 0x38
#define MPU6500_REG_INT_STATUS 0x3A
#define MPU6500_REG_ACCEL_XOUT_H 0x3B
#define MPU6500_REG_ACCEL_XOUT_L 0x3C
#define MPU6500_REG_ACCEL_YOUT_H 0x3D
#define MPU6500_REG_ACCEL_YOUT_L 0x3E
#define MPU6500_REG_ACCEL_ZOUT_H 0x3F
#define MPU6500_REG_ACCEL_ZOUT_L 0x40
#define MPU6500_REG_TEMP_OUT_H 0x41
#define MPU6500_REG_TEMP_OUT_L 0x42
#define MPU6500_REG_GYRO_XOUT_H 0x43
#define MPU6500_REG_GYRO_XOUT_L 0x44
#define MPU6500_REG_GYRO_YOUT_H 0x45
#define MPU6500_REG_GYRO_YOUT_L 0x46
#define MPU6500_REG_GYRO_ZOUT_H 0x47
#define MPU6500_REG_GYRO_ZOUT_L 0x48
#define MPU6500_REG_SIGNAL_PATH_RESET 0x68
#define MPU6500_REG_MOT_DETECT_CTRL 0x69
#define MPU6500_REG_USER_CTRL 0x6A
#define MPU6500_REG_PWR_MGMT_1 0x6B
#define MPU6500_REG_PWR_MGMT_2 0x6C
#define MPU6500_REG_FIFO_COUNTH 0x72
#define MPU6500_REG_FIFO_COUNTL 0x73
#define MPU6500_REG_FIFO_R_W 0x74
#define MPU6500_REG_WHO_AM_I 0x75
#define MPU6500_REG_XA_OFFSET_H 0x77
#define MPU6500_REG_XA_OFFSET_L 0x78
#define MPU6500_REG_YA_OFFSET_H 0x7A
#define MPU6500_REG_YA_OFFSET_L 0x7B
#define MPU6500_REG_ZA_OFFSET_H 0x7D
#define MPU6500_REG_ZA_OFFSET_L 0x7E

// MPU6500 WHO_AM_I 返回值
#define MPU6500_WHO_AM_I_VALUE 0x70

typedef enum {
  // ±2g,  灵敏度 16384 LSB/g
  MPU6500_ACCEL_RANGE_2G = 0x00,
  // ±4g,  灵敏度 8192  LSB/g
  MPU6500_ACCEL_RANGE_4G = 0x08,
  // ±8g,  灵敏度 4096  LSB/g
  MPU6500_ACCEL_RANGE_8G = 0x10,
  // ±16g, 灵敏度 2048  LSB/g
  MPU6500_ACCEL_RANGE_16G = 0x18
} MPU6500_AccelRange_t;

typedef enum {
  // ±250°/s,  灵敏度 131   LSB/(°/s)
  MPU6500_GYRO_RANGE_250DPS = 0x00,
  // ±500°/s,  灵敏度 65.5  LSB/(°/s)
  MPU6500_GYRO_RANGE_500DPS = 0x08,
  // ±1000°/s, 灵敏度 32.8  LSB/(°/s)
  MPU6500_GYRO_RANGE_1000DPS = 0x10,
  // ±2000°/s, 灵敏度 16.4  LSB/(°/s)
  MPU6500_GYRO_RANGE_2000DPS = 0x18
} MPU6500_GyroRange_t;

typedef enum {
  // 加速度计带宽 250Hz
  MPU6500_DLPF_250HZ = 0x00,
  // 加速度计带宽 184Hz
  MPU6500_DLPF_184HZ = 0x01,
  MPU6500_DLPF_92HZ = 0x02,
  MPU6500_DLPF_41HZ = 0x03,
  MPU6500_DLPF_20HZ = 0x04,
  MPU6500_DLPF_10HZ = 0x05,
  MPU6500_DLPF_5HZ = 0x06
} MPU6500_DLPF_t;

typedef struct {
  /* 原始原始值 (raw ADC counts) */
  int16_t accel_x_raw;
  int16_t accel_y_raw;
  int16_t accel_z_raw;
  int16_t gyro_x_raw;
  int16_t gyro_y_raw;
  int16_t gyro_z_raw;
  int16_t temp_raw;

  /* 物理量 */
  // X 轴加速度 (g)
  float accel_x_g;
  // Y 轴加速度 (g)
  float accel_y_g;
  // Z 轴加速度 (g)
  float accel_z_g;
  // X 轴角速度 (°/s)
  float gyro_x_dps;
  // Y 轴角速度 (°/s)
  float gyro_y_dps;
  // Z 轴角速度 (°/s)
  float gyro_z_dps;
  // 芯片温度 (°C)
  float temperature_c;
} MPU6500_Data_t;

typedef struct {
  // SPI 句柄
  SPI_HandleTypeDef *hspi;
  // 片选引脚端口
  GPIO_TypeDef *CS_GPIO_Port;
  // 片选引脚号
  uint16_t CS_GPIO_Pin;
  // 加速度计量程
  MPU6500_AccelRange_t accel_range;
  // 陀螺仪量程
  MPU6500_GyroRange_t gyro_range;
  // 低通滤波器
  MPU6500_DLPF_t dlpf;
  // 采样率分频器: Sample Rate = 1kHz / (1 + div)
  uint16_t sample_rate_divider; 
} MPU6500_Config_t;

#define MPU6500_OK 0
#define MPU6500_ERROR (-1)

/**
 * @brief 初始化 MPU6500（SPI + DMA）
 *        检测 WHO_AM_I，配置量程、DLPF、采样率，使能数据就绪中断
 * @param cfg 配置结构体指针
 * @return MPU6500_OK 或 MPU6500_ERROR
 */
int MPU6500_Init(const MPU6500_Config_t *cfg);

/**
 * @brief 触发一次 DMA 读取 14 字节传感器数据
 *        由 MPU_INT 中断或定时器调用，数据在 DMA 完成回调中更新
 * @return MPU6500_OK 或 MPU6500_ERROR（上一次读取未完成）
 */
int MPU6500_ReadAllDMA(void);

/**
 * @brief 获取最新的传感器数据（含物理量转换）
 * @param data 输出结构体指针
 */
void MPU6500_GetData(MPU6500_Data_t *data);

/**
 * @brief 获取原始加速度计 X 值
 */
int16_t MPU6500_GetAccelX(void);

/**
 * @brief 获取原始加速度计 Y 值
 */
int16_t MPU6500_GetAccelY(void);

/**
 * @brief 获取原始加速度计 Z 值
 */
int16_t MPU6500_GetAccelZ(void);

/**
 * @brief 获取原始陀螺仪 X 值
 */
int16_t MPU6500_GetGyroX(void);

/**
 * @brief 获取原始陀螺仪 Y 值
 */
int16_t MPU6500_GetGyroY(void);

/**
 * @brief 获取原始陀螺仪 Z 值
 */
int16_t MPU6500_GetGyroZ(void);

/**
 * @brief 获取温度原始值
 */
int16_t MPU6500_GetTempRaw(void);

/**
 * @brief 获取温度 (°C)
 */
float MPU6500_GetTemperature(void);

void MPU6500_ProcessData(void);

/**
 * @brief EXTI 中断处理（在 HAL_GPIO_EXTI_Callback 中调用）
 *        当 MPU_INT 引脚触发时唤醒 IMU 任务
 */
void MPU6500_EXTI_Callback(void);

#ifdef __cplusplus
}
#endif