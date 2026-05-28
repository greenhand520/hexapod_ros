//
// Created by greenhand520 on 2026/4/28.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * STM32 FreeRTOS + I2C HAL driver for the BQ24725 Battery Charge Manager
 *
 * 注意：BQ40Z50 与 BQ24725 共享同一条 I2C 总线，
 * 需要通过共享的 FreeRTOS 互斥锁进行总线仲裁。
 */
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>

#include "main.h"

/** BQ24725 7-bit 地址,左移1位(自动补0)为HAL库格式 */
#define BQ24725_ADDR (0x09u << 1)

#define BQ24725_REG_DEVICE_ID 0xFF
#define BQ24725_REG_MANUFACTURE_ID 0xFE
#define BQ24725_REG_CHARGE_CURRENT 0x14
#define BQ24725_REG_CHARGE_VOLTAGE 0x15
#define BQ24725_REG_INPUT_CURRENT 0x3F
#define BQ24725_REG_CHARGE_OPTION 0x12

/* ================================================================
 *  数据掩码
 * ================================================================ */
#define CHARGE_CURRENT_MASK 0x1FC0
#define CHARGE_VOLTAGE_MASK 0x7FF0
#define INPUT_CURRENT_MASK 0x1F80

/** Charge Option 位域枚举 */
typedef enum { t150ms = 0, t1300ms = 0x8000 } BQ24725_ACOK_deglitch_time;
#define BQ24725_ACOK_deglitch_time_MASK 0x8000

typedef enum {
  disabled = 0,
  t44s = 0x2000,
  t88s = 0x4000,
  t175s = 0x6000
} BQ24725_WATCHDOG_timer;
#define BQ24725_WATCHDOG_timer_MASK 0x6000

typedef enum {
  FT59_19pct = 0,
  FT62_65pct = 0x800,
  FT66_55pct = 0x1000,
  FT70_97pct = 0x1800
} BQ24725_BAT_depletion_threshold;
#define BQ24725_BAT_depletion_threshold_MASK 0x1800

typedef enum { dec18pct = 0, inc18pct = 0x400 } BQ24725_EMI_sw_freq_adj;
#define BQ24725_EMI_sw_freq_adj_MASK 0x400

typedef enum {
  sw_freq_adj_disable = 0,
  sw_freq_adj_enable = 0x200
} BQ24725_EMI_sw_freq_adj_en;
#define BQ24725_EMI_sw_freq_adj_en_MASK 0x200

typedef enum {
  l300mV = 0,
  l500mV = 0x80,
  l700mV = 0x100,
  l900mV = 0x180
} BQ24725_IFAULT_HI_threshold;
#define BQ24725_IFAULT_HI_threshold_MASK 0x180

typedef enum { LEARN_disable = 0, LEARN_enable = 0x40 } BQ24725_LEARN_en;
#define BQ24725_LEARN_en_MASK 0x40

typedef enum { adapter_current = 0, charge_current = 0x20 } BQ24725_IOUT;
#define BQ24725_IOUT_MASK 0x20

typedef enum {
  ACOC_disable = 0,
  l1_33X = 0x2,
  l1_66X = 0x4,
  l2_22X = 0x6
} BQ24725_ACOC_threshold;
#define BQ24725_ACOC_threshold_MASK 0x6

typedef enum { charge_enable = 0, charge_inhibit = 1 } BQ24725_charge_inhibit;
#define BQ24725_charge_inhibit_MASK 0x1

typedef struct BQ24725_charge_options {
  // ACOK 信号去抖时间
  BQ24725_ACOK_deglitch_time ACOK_deglitch_time;
  // 看门狗定时器
  BQ24725_WATCHDOG_timer WATCHDOG_timer;
  // 电池耗尽阈值
  BQ24725_BAT_depletion_threshold BAT_depletion_threshold;
  // EMI 开关频率调整方向
  BQ24725_EMI_sw_freq_adj EMI_sw_freq_adj;
  // EMI 频率调整使能
  BQ24725_EMI_sw_freq_adj_en EMI_sw_freq_adj_en;
  // 高侧过流保护阈值
  BQ24725_IFAULT_HI_threshold IFAULT_HI_threshold;
  // 电池学习模式
  BQ24725_LEARN_en LEARN_en;
  // IOUT 引脚输出选择
  BQ24725_IOUT IOUT;
  // AC 过流保护阈值
  BQ24725_ACOC_threshold ACOC_threshold;
  // 充电禁止/使能
  BQ24725_charge_inhibit charge_inhibit;
} BQ24725_charge_options;

uint16_t BQ24725_FormOptionsData(const BQ24725_charge_options *opts);

void BQ24725_FormOptionsStruct(uint16_t data, BQ24725_charge_options *opt);

typedef void (*BQ24725_ACOK_Callback_t)(void);

typedef struct BQ24725Config {
  /** CubeMX 生成的 I2C 句柄（与 BQ24725 共用） */
  I2C_HandleTypeDef *hi2c;
  /** 共享总线互斥锁（与 BQ24725 共用同一个） */
  SemaphoreHandle_t bus_mutex;
  /** ACOK 引脚端口 */
  GPIO_TypeDef *ACOK_GPIO_Port;
  /**  ACOK 引脚号 */
  uint16_t ACOK_GPIO_Pin;
  /** ACOK 边沿回调 */
  BQ24725_ACOK_Callback_t ACOK_cb;
} BQ24725Config;

void BQ24725_Init(BQ24725Config *conf);

ErrorStatus BQ24725_SetChargeOption(const BQ24725_charge_options *option);
ErrorStatus BQ24725_SetChargeCurrent(uint16_t mA);
ErrorStatus BQ24725_SetChargeVoltage(uint16_t mV);
ErrorStatus BQ24725_SetInputCurrent(uint16_t mA);

ErrorStatus BQ24725_GetDeviceID(uint16_t *data);
ErrorStatus BQ24725_GetManufactureID(uint16_t *data);
ErrorStatus BQ24725_GetChargeCurrent(uint16_t *data);
ErrorStatus BQ24725_GetChargeVoltage(uint16_t *data);
ErrorStatus BQ24725_GetInputCurrent(uint16_t *data);
ErrorStatus BQ24725_GetChargeOption(uint16_t *data);

/**
 * @brief ACOK 电平读取
 * @return ACOK电平
 */
GPIO_PinState BQ24725_ACOK_Read(void);

/**
 * @brief IMON ADC 值读取
 *        注意：BQ24725 充电电流现在统一由 adc.c 通过 DMA 多通道采集
 *        使用 ADC_GetBC_IOUT_mA() 或 ADC_GetBC_IOUT_Voltage() 代替
 *        保留此函数用于向后兼容
 * @return 返回电压值的 ADC 原始值近似
 */
uint16_t BQ24725_IMON_Read(void);

extern const BQ24725_charge_options BQ24725_charge_options_POR_default;

void BQ24725_ACOK_EXTI_Callback(void);

#ifdef __cplusplus
}
#endif