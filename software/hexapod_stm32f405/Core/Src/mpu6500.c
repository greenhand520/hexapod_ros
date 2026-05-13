//
// Created by greenhand520 on 2026/5/12.
//
// MPU6500 6-axis IMU driver (SPI + DMA, FreeRTOS)
//

#include "mpu6500.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <string.h>

static const MPU6500_Config_t *s_cfg = NULL;
static uint8_t s_initialized = 0;

/* SPI 收发缓冲区 */
/* 读取寄存器时：发送 [REG|0x80] + 14 字节 dummy，接收 1+14 字节 */
/* 14 字节数据：AX(2) + AY(2) + AZ(2) + TEMP(2) + GX(2) + GY(2) + GZ(2) */
#define MPU6500_DATA_LEN  14
static uint8_t s_tx_buf[1 + MPU6500_DATA_LEN];  /* 1 byte addr + 14 bytes dummy */
static uint8_t s_rx_buf[1 + MPU6500_DATA_LEN];  /* 1 byte dummy + 14 bytes data */

/* DMA 传输完成标志 */
static volatile uint8_t s_spi_dma_done = 0;
static volatile uint8_t s_spi_dma_busy = 0;

/* 处理后的传感器数据 */
static MPU6500_Data_t s_imu_data;

/* 量程灵敏度因子（在 Init 中设置） */
static float s_accel_sensitivity = 16384.0f;  /* 默认 ±2g */
static float s_gyro_sensitivity  = 131.0f;    /* 默认 ±250°/s */

/* ================================================================
 *  CS 片选控制
 * ================================================================ */

static inline void CS_LOW(void) {
    HAL_GPIO_WritePin(s_cfg->CS_GPIO_Port, s_cfg->CS_GPIO_Pin, GPIO_PIN_RESET);
}

static inline void CS_HIGH(void) {
    HAL_GPIO_WritePin(s_cfg->CS_GPIO_Port, s_cfg->CS_GPIO_Pin, GPIO_PIN_SET);
}

/* ================================================================
 *  SPI 基础读写（阻塞方式，用于初始化阶段）
 * ================================================================ */

/**
 * @brief 写单个寄存器（阻塞）
 */
static int MPU6500_WriteReg(const uint8_t reg, const uint8_t value) {
  const uint8_t tx[2] = { reg, value };
    CS_LOW();
  const HAL_StatusTypeDef ret = HAL_SPI_Transmit(s_cfg->hspi, tx, 2, 100);
    CS_HIGH();
    return (ret == HAL_OK) ? MPU6500_OK : MPU6500_ERROR;
}

/**
 * @brief 读单个寄存器（阻塞）
 */
static int MPU6500_ReadReg(const uint8_t reg, uint8_t *value) {
  const uint8_t tx[2] = { reg | 0x80, 0x00 };  /* bit7=1 表示读操作 */
    uint8_t rx[2] = { 0 };
    CS_LOW();
    const HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(s_cfg->hspi, tx, rx, 2, 100);
    CS_HIGH();
    if (ret != HAL_OK) return MPU6500_ERROR;
    *value = rx[1];
    return MPU6500_OK;
}

/**
 * @brief 读多个寄存器（阻塞）
 */
static int MPU6500_ReadRegs(const uint8_t start_reg, uint8_t *buf,
                            const uint8_t len) {
    uint8_t tx[1 + 14]; /* 最大 15 字节 */
    uint8_t rx[1 + 14];
    if (len > 14) return MPU6500_ERROR;

    memset(tx, 0x00, sizeof(tx));
    tx[0] = start_reg | 0x80;

    CS_LOW();
    const HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(s_cfg->hspi, tx, rx, 1 + len, 100);
    CS_HIGH();
    if (ret != HAL_OK) return MPU6500_ERROR;

    memcpy(buf, &rx[1], len);
    return MPU6500_OK;
}

/* ================================================================
 *  HAL SPI DMA 完成回调
 * ================================================================ */

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == s_cfg->hspi->Instance) {
        CS_HIGH();
        s_spi_dma_busy = 0;
        s_spi_dma_done = 1;

        /* 唤醒 IMUTask */
        extern osThreadId_t IMUTaskHandle;
        if (IMUTaskHandle != NULL) {
            osThreadFlagsSet(IMUTaskHandle, 0x02);
        }
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == s_cfg->hspi->Instance) {
        CS_HIGH();
        s_spi_dma_busy = 0;
    }
}

/* ================================================================
 *  EXTI 中断回调
 * ================================================================ */

void MPU6500_EXTI_Callback(void) {
    /* MPU_INT 触发，唤醒 IMU 任务去读取数据 */
    extern osThreadId_t IMUTaskHandle;
    if (IMUTaskHandle != NULL) {
        osThreadFlagsSet(IMUTaskHandle, 0x02);
    }
}

/* ================================================================
 *  公开 API
 * ================================================================ */

int MPU6500_Init(const MPU6500_Config_t *cfg) {
    uint8_t who_am_i = 0;

    configASSERT(cfg);
    configASSERT(!s_initialized);

    s_cfg = cfg;

    /* 确保 CS 默认高电平 */
    CS_HIGH();

    /* ---- 复位 MPU6500 ---- */
    MPU6500_WriteReg(MPU6500_REG_PWR_MGMT_1, 0x80);  /* DEVICE_RESET */
    osDelay(100);  /* 等待复位完成 */

    /* ---- 唤醒 + 选择时钟源 ---- */
    MPU6500_WriteReg(MPU6500_REG_PWR_MGMT_1, 0x01);  /* CLKSEL = PLL with X gyro ref */
    osDelay(10);

    /* ---- 验证 WHO_AM_I ---- */
    MPU6500_ReadReg(MPU6500_REG_WHO_AM_I, &who_am_i);
    if (who_am_i != MPU6500_WHO_AM_I_VALUE) {
        return MPU6500_ERROR;
    }

    /* ---- 配置 DLPF ---- */
    MPU6500_WriteReg(MPU6500_REG_CONFIG, (uint8_t)s_cfg->dlpf);

    /* ---- 配置采样率分频 ---- */
    MPU6500_WriteReg(MPU6500_REG_SMPLRT_DIV, (uint8_t)(s_cfg->sample_rate_divider & 0xFF));

    /* ---- 配置陀螺仪量程 ---- */
    MPU6500_WriteReg(MPU6500_REG_GYRO_CONFIG, (uint8_t)s_cfg->gyro_range);
    switch (s_cfg->gyro_range) {
        case MPU6500_GYRO_RANGE_250DPS:  s_gyro_sensitivity = 131.0f;   break;
        case MPU6500_GYRO_RANGE_500DPS:  s_gyro_sensitivity = 65.5f;    break;
        case MPU6500_GYRO_RANGE_1000DPS: s_gyro_sensitivity = 32.8f;    break;
        case MPU6500_GYRO_RANGE_2000DPS: s_gyro_sensitivity = 16.4f;    break;
        default:                         s_gyro_sensitivity = 131.0f;   break;
    }

    /* ---- 配置加速度计量程 ---- */
    MPU6500_WriteReg(MPU6500_REG_ACCEL_CONFIG, (uint8_t)s_cfg->accel_range);
    switch (s_cfg->accel_range) {
        case MPU6500_ACCEL_RANGE_2G:  s_accel_sensitivity = 16384.0f; break;
        case MPU6500_ACCEL_RANGE_4G:  s_accel_sensitivity = 8192.0f;  break;
        case MPU6500_ACCEL_RANGE_8G:  s_accel_sensitivity = 4096.0f;  break;
        case MPU6500_ACCEL_RANGE_16G: s_accel_sensitivity = 2048.0f;  break;
        default:                      s_accel_sensitivity = 16384.0f; break;
    }

    /* ---- 加速度计 DLPF 配置 ---- */
    MPU6500_WriteReg(MPU6500_REG_ACCEL_CONFIG_2, (uint8_t)s_cfg->dlpf);

    /* ---- 使能数据就绪中断 ---- */
    MPU6500_WriteReg(MPU6500_REG_INT_PIN_CFG, 0x10);  /* INT_LEVEL=0, LATCH_INT_EN=1 */
    MPU6500_WriteReg(MPU6500_REG_INT_ENABLE,  0x01);  /* DATA_RDY_EN=1 */
    osDelay(10);

    s_initialized = 1;
    memset(&s_imu_data, 0, sizeof(s_imu_data));

    return MPU6500_OK;
}

int MPU6500_ReadAllDMA(void) {
    if (!s_initialized || s_spi_dma_busy) {
        return MPU6500_ERROR;
    }

    s_spi_dma_busy = 1;
    s_spi_dma_done = 0;

    /* 构造读请求：REG = 0x3B (ACCEL_XOUT_H) | 0x80 (读标志) */
    memset(s_tx_buf, 0x00, sizeof(s_tx_buf));
    s_tx_buf[0] = MPU6500_REG_ACCEL_XOUT_H | 0x80;

    CS_LOW();
    const HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive_DMA(s_cfg->hspi, s_tx_buf, s_rx_buf, 1 + MPU6500_DATA_LEN);
    if (ret != HAL_OK) {
        CS_HIGH();
        s_spi_dma_busy = 0;
        return MPU6500_ERROR;
    }

    return MPU6500_OK;
}

void MPU6500_GetData(MPU6500_Data_t *data) {
    taskENTER_CRITICAL();
    *data = s_imu_data;
    taskEXIT_CRITICAL();
}

int16_t MPU6500_GetAccelX(void) { return s_imu_data.accel_x_raw; }
int16_t MPU6500_GetAccelY(void) { return s_imu_data.accel_y_raw; }
int16_t MPU6500_GetAccelZ(void) { return s_imu_data.accel_z_raw; }
int16_t MPU6500_GetGyroX(void)  { return s_imu_data.gyro_x_raw;  }
int16_t MPU6500_GetGyroY(void)  { return s_imu_data.gyro_y_raw;  }
int16_t MPU6500_GetGyroZ(void)  { return s_imu_data.gyro_z_raw;  }
int16_t MPU6500_GetTempRaw(void) { return s_imu_data.temp_raw;    }
float MPU6500_GetTemperature(void) { return s_imu_data.temperature_c; }

/**
 * @brief 处理 DMA 接收到的原始数据并转换为物理量
 *        应在 DMA 完成后由任务调用
 */
void MPU6500_ProcessData(void) {
    /* 从 rx_buf 解析原始数据（偏移 1 字节，跳过 SPI 读操作的 dummy 字节） */
    s_imu_data.accel_x_raw = (int16_t)((s_rx_buf[1] << 8) | s_rx_buf[2]);
    s_imu_data.accel_y_raw = (int16_t)((s_rx_buf[3] << 8) | s_rx_buf[4]);
    s_imu_data.accel_z_raw = (int16_t)((s_rx_buf[5] << 8) | s_rx_buf[6]);
    s_imu_data.temp_raw    = (int16_t)((s_rx_buf[7] << 8) | s_rx_buf[8]);
    s_imu_data.gyro_x_raw  = (int16_t)((s_rx_buf[9] << 8) | s_rx_buf[10]);
    s_imu_data.gyro_y_raw  = (int16_t)((s_rx_buf[11] << 8) | s_rx_buf[12]);
    s_imu_data.gyro_z_raw  = (int16_t)((s_rx_buf[13] << 8) | s_rx_buf[14]);

    /* 转换为物理量 */
    s_imu_data.accel_x_g = (float)s_imu_data.accel_x_raw / s_accel_sensitivity;
    s_imu_data.accel_y_g = (float)s_imu_data.accel_y_raw / s_accel_sensitivity;
    s_imu_data.accel_z_g = (float)s_imu_data.accel_z_raw / s_accel_sensitivity;

    s_imu_data.gyro_x_dps = (float)s_imu_data.gyro_x_raw / s_gyro_sensitivity;
    s_imu_data.gyro_y_dps = (float)s_imu_data.gyro_y_raw / s_gyro_sensitivity;
    s_imu_data.gyro_z_dps = (float)s_imu_data.gyro_z_raw / s_gyro_sensitivity;

    /* 温度公式: T = (TEMP_OUT / 333.87) + 21.0 (见 MPU6500 datasheet) */
    s_imu_data.temperature_c = (float)s_imu_data.temp_raw / 333.87f + 21.0f;
}