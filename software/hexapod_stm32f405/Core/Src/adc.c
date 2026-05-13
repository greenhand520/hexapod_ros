//
// Created by greenhand520 on 2026/5/12.
//

#include "adc.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include <math.h>
#include <string.h>

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

/* DMA 接收缓冲区（半字 × 6 通道） */
static volatile uint16_t s_adc_dma_buf[ADC_CH_COUNT];

/* 处理后的物理量结果（双缓冲思想：DMA 回调置标志，任务处理后更新） */
static ADC_Results_t s_results;

/* DMA 传输完成标志 */
static volatile uint8_t s_adc_conv_complete = 0;

/* ADC 模块是否已启动 */
static uint8_t s_adc_started = 0;

/* ================================================================
 *  DMA 传输完成回调（HAL 弱函数覆盖）
 *  所有 ADC DMA 回调统一在此处理
 * ================================================================ */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        s_adc_conv_complete = 1;

        /* DMA 模式为 NORMAL，传输完成后需重新启动以继续采集 */
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_dma_buf, ADC_CH_COUNT);

        /* 唤醒 ADC 任务（如果已创建） */
        extern osThreadId_t ADCTaskHandle;
        if (ADCTaskHandle != NULL) {
            osThreadFlagsSet(ADCTaskHandle, 0x01);
        }
    }
}

/**
 * @brief NTC B3950 热敏电阻：ADC 原始值 → 温度 (°C)
 *        电路: VDD → R_pullup → ADC引脚 → NTC → GND
 *        V_adc = VDD * R_ntc / (R_pullup + R_ntc)
 *        因此: R_ntc = R_pullup * V_adc / (VDD - V_adc)
 */
static float NTC_AdcToCelsius(const uint16_t raw) {
    if (raw == 0 || raw >= 4095) {
        return -273.15f; /* 无效值，返回绝对零度作为错误标志 */
    }

    const float v_adc = (float)raw / ADC_RESOLUTION * ADC_VDDA;
    const float r_ntc = NTC_PULLUP_R * v_adc / (ADC_VDDA - v_adc);

    /* Steinhart-Hart 简化 (B 参数方程):
     * 1/T = 1/T0 + (1/B) * ln(R/R0)
     */
    const float temp_k = 1.0f / (1.0f / NTC_T0 + (1.0f / NTC_BETA) * logf(r_ntc / NTC_R0));
    return temp_k - 273.15f;
}

/**
 * @brief NTC B3950：ADC 原始值 → 当前阻值 (Ω)
 */
static float NTC_AdcToResistance(const uint16_t raw) {
    if (raw == 0 || raw >= 4095) {
        return 0.0f;
    }
    const float v_adc = (float)raw / ADC_RESOLUTION * ADC_VDDA;
    return NTC_PULLUP_R * v_adc / (ADC_VDDA - v_adc);
}

/**
 * @brief ADC 原始值 → 电压 (V)
 */
static float ADC_RawToVoltage(const uint16_t raw) {
    return (float)raw / ADC_RESOLUTION * ADC_VDDA;
}


void ADC_Start(void) {
    if (s_adc_started) {
        return;
    }

    /* 清空 DMA 缓冲区 */
    memset((void *)s_adc_dma_buf, 0, sizeof(s_adc_dma_buf));
    memset(&s_results, 0, sizeof(s_results));
    s_adc_conv_complete = 0;

    /*
     * CubeMX 已经在 MX_ADC1_Init() 中配置好了 6 通道扫描 + DMAContinuousRequests。
     * 这里只需启动 TIM2 作为触发源 + 启动 ADC DMA 即可。
     *
     * 注意：DMA 模式为 NORMAL，每次 DMA 完成后需要重新启动。
     * 但由于 DMAContinuousRequests=ENABLE + TIM2 连续触发，
     * ADC 会持续产生数据。HAL_ADC_Start_DMA 在 NORMAL 模式下
     * 只在首次需要调用，后续在 ConvCplt 回调中重新启动。
     */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_dma_buf, ADC_CH_COUNT);

    /* 启动 TIM2 触发 */
    HAL_TIM_Base_Start(&htim2);

    s_adc_started = 1;
}

void ADC_ProcessData(void) {
    /* 复制 DMA 缓冲区到局部变量（避免 volatile 访问效率问题） */
    uint16_t raw[ADC_CH_COUNT];
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        raw[i] = s_adc_dma_buf[i];
    }

    /* 保存原始值 */
    memcpy(s_results.raw, raw, sizeof(raw));

    /* ---- BC_IOUT: BQ24725 充电电流检测 ---- */
    s_results.bc_iout_voltage = ADC_RawToVoltage(raw[ADC_CH_BC_IOUT]);
    s_results.bc_iout_mA = s_results.bc_iout_voltage * 128.0f / BC_IMON_RESISTOR;

    /* ---- TEMP1: NTC 热敏电阻1 ---- */
    s_results.temp1_resistance = NTC_AdcToResistance(raw[ADC_CH_TEMP1]);
    s_results.temp1_celsius = NTC_AdcToCelsius(raw[ADC_CH_TEMP1]);

    /* ---- TEMP2: NTC 热敏电阻2 ---- */
    s_results.temp2_resistance = NTC_AdcToResistance(raw[ADC_CH_TEMP2]);
    s_results.temp2_celsius = NTC_AdcToCelsius(raw[ADC_CH_TEMP2]);

    /* ---- SVM: 舵机供电电压 ---- */
    s_results.svm_voltage = ADC_RawToVoltage(raw[ADC_CH_SVM]) * SVM_DIVIDER_RATIO;

    /* ---- CV: 充电电压 ---- */
    s_results.cv_voltage = ADC_RawToVoltage(raw[ADC_CH_CV]) * CV_DIVIDER_RATIO;

    /* ---- MCU 内部温度传感器 ---- */
    {
        const float v_sense = ADC_RawToVoltage(raw[ADC_CH_MCU_TEMP]);
        s_results.mcu_temp_celsius = (v_sense - MCU_TEMP_V25) / MCU_TEMP_AVG_SLOPE + 25.0f;
    }

    /* 清除完成标志，准备下一次 DMA 传输 */
    s_adc_conv_complete = 0;
}

void ADC_GetResults(ADC_Results_t *result) {
    taskENTER_CRITICAL();
    *result = s_results;
    taskEXIT_CRITICAL();
}

uint16_t ADC_GetRaw(const uint8_t ch) {
    if (ch >= ADC_CH_COUNT) return 0;
    return s_adc_dma_buf[ch];
}

float ADC_GetBC_IOUT_Voltage(void) {
    return s_results.bc_iout_voltage;
}

float ADC_GetBC_IOUT_mA(void) {
    return s_results.bc_iout_mA;
}

float ADC_GetTemp1(void) {
    return s_results.temp1_celsius;
}

float ADC_GetTemp2(void) {
    return s_results.temp2_celsius;
}

float ADC_GetSVM(void) {
    return s_results.svm_voltage;
}

float ADC_GetCV(void) {
    return s_results.cv_voltage;
}

float ADC_GetMCUTemp(void) {
    return s_results.mcu_temp_celsius;
}