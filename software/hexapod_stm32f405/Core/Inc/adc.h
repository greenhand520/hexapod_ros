//
// Created by greenhand520 on 2026/5/12.
//

#pragma once

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  ADC DMA 通道顺序（对应 CubeMX 中 Rank 1~6）
 * ================================================================ */
#define ADC_CH_BC_IOUT       0   /* Rank1: Channel10 - BQ24725 充电电流检测 (PC0) */
#define ADC_CH_TEMP1         1   /* Rank2: Channel11 - NTC 热敏电阻1 (PC1)         */
#define ADC_CH_TEMP2         2   /* Rank3: Channel12 - NTC 热敏电阻2 (PC2)         */
#define ADC_CH_SVM           3   /* Rank4: Channel13 - 舵机供电电压检测 (PC3)       */
#define ADC_CH_CV            4   /* Rank5: Channel15 - 充电电压检测 (PC5)           */
#define ADC_CH_MCU_TEMP      5   /* Rank6: 内部温度传感器                           */
#define ADC_CH_COUNT         6

/* ================================================================
 *  硬件参数宏（根据实际电路修改）
 * ================================================================ */

/* VDDA 参考电压 (V) */
#define ADC_VDDA             3.3f

/* 12-bit ADC 满量程 */
#define ADC_RESOLUTION       4095.0f

/* ---- NTC B3950 热敏电阻参数 ---- */
#define NTC_BETA             3950.0f       /* Beta 值 (K)              */
#define NTC_R0               10000.0f      /* 25°C 时标称阻值 (Ω)     */
#define NTC_T0               298.15f       /* 25°C = 298.15 K          */
#define NTC_PULLUP_R         10000.0f      /* 上拉电阻阻值 (Ω)        */
                                            /* 电路: VDD→R_pullup→ADC→NTC→GND */

/* ---- 舵机供电电压分压比 (V_servo = V_adc × SVM_DIVIDER_RATIO) ---- */
/* 典型: R_top=30kΩ, R_bot=10kΩ → ratio = (30+10)/10 = 4.0 */
#define SVM_DIVIDER_RATIO    4.0f

/* ---- 充电电压分压比 (V_charge = V_adc × CV_DIVIDER_RATIO) ---- */
/* 典型: R_top=56kΩ, R_bot=10kΩ → ratio = (56+10)/10 = 6.6 */
#define CV_DIVIDER_RATIO     6.6f

/* ---- BQ24725 IOUT 检测参数 ---- */
/* I_charge = V_adc × 128 / BC_IMON_RESISTOR */
#define BC_IMON_RESISTOR     100.0f        /* IOUT 引脚检测电阻 (Ω)   */

/* ---- STM32F405 内部温度传感器参数 ---- */
#define MCU_TEMP_V25         0.76f         /* 25°C 时的感测电压 (V)   */
#define MCU_TEMP_AVG_SLOPE   0.0025f       /* 平均斜率 (V/°C)         */

/* ================================================================
 *  处理后的 ADC 结果结构体
 * ================================================================ */

typedef struct {
    float bc_iout_voltage;     /* BQ24725 IOUT 引脚电压 (V)   */
    float bc_iout_mA;          /* BQ24725 充电电流 (mA)       */
    float temp1_resistance;    /* NTC1 当前阻值 (Ω)           */
    float temp1_celsius;       /* NTC1 温度 (°C)              */
    float temp2_resistance;    /* NTC2 当前阻值 (Ω)           */
    float temp2_celsius;       /* NTC2 温度 (°C)              */
    float svm_voltage;         /* 舵机供电电压 (V)             */
    float cv_voltage;          /* 充电电压 (V)                */
    float mcu_temp_celsius;    /* MCU 内部温度 (°C)           */
    uint16_t raw[ADC_CH_COUNT]; /* 各通道原始 ADC 值 (0~4095) */
} ADC_Results_t;

/* ================================================================
 *  API
 * ================================================================ */

/**
 * @brief 启动 ADC DMA 采集 + TIM2 触发
 *        应在 MX_ADC1_Init() 和 MX_TIM2_Init() 之后调用
 *        会自动获取当前 FreeRTOS 任务句柄用于 DMA 完成通知
 */
void ADC_Start(void);

/**
 * @brief ADC 数据处理（从 DMA 缓冲区转换为物理量）
 *        在收到 DMA 完成通知后调用
 */
void ADC_ProcessData(void);

/**
 * @brief 获取最新的 ADC 处理结果（完整结构体拷贝）
 */
void ADC_GetResults(ADC_Results_t *result);

/**
 * @brief 获取某个通道的原始 ADC 值
 * @param ch  ADC_CH_xxx 宏
 */
uint16_t ADC_GetRaw(uint8_t ch);

/**
 * @brief 获取 BQ24725 IOUT 引脚电压 (V)
 */
float ADC_GetBC_IOUT_Voltage(void);

/**
 * @brief 获取 BQ24725 充电电流 (mA)
 */
float ADC_GetBC_IOUT_mA(void);

/**
 * @brief 获取 NTC1 温度 (°C)
 */
float ADC_GetTemp1(void);

/**
 * @brief 获取 NTC2 温度 (°C)
 */
float ADC_GetTemp2(void);

/**
 * @brief 获取舵机供电电压 (V)
 */
float ADC_GetSVM(void);

/**
 * @brief 获取充电电压 (V)
 */
float ADC_GetCV(void);

/**
 * @brief 获取 MCU 内部温度 (°C)
 */
float ADC_GetMCUTemp(void);

#ifdef __cplusplus
}
#endif