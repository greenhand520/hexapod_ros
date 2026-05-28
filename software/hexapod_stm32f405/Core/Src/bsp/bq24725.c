//
// Created by greenhand520 on 2026/4/28.
//

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <string.h>

#include "bsp/bq24725.h"
#include "bsp/i2c.h"

/** 操作超时时间 100ms */
#define BQ24725_I2C_TIMEOUT (100u)

static BQ24725Config *s_conf = NULL;
static bool s_initialized = false;

/** 默认配置 */
const BQ24725_charge_options BQ24725_charge_options_POR_default = {
    .ACOK_deglitch_time = t150ms,
    .WATCHDOG_timer = disabled,
    .BAT_depletion_threshold = FT70_97pct,
    .EMI_sw_freq_adj = dec18pct,
    .EMI_sw_freq_adj_en = sw_freq_adj_disable,
    .IFAULT_HI_threshold = l700mV,
    .LEARN_en = LEARN_disable,
    .IOUT = charge_current,
    .ACOC_threshold = l1_66X,
    .charge_inhibit = charge_enable};

/** @brief ACOK 外部中断回调 */
void BQ24725_ACOK_EXTI_Callback(void) {
  if (s_conf && s_conf->ACOK_cb) {
    s_conf->ACOK_cb();
  }
}

uint16_t BQ24725_FormOptionsData(const BQ24725_charge_options *opts) {
  return opts->ACOK_deglitch_time | opts->WATCHDOG_timer |
         opts->BAT_depletion_threshold | opts->EMI_sw_freq_adj |
         opts->EMI_sw_freq_adj_en | opts->IFAULT_HI_threshold | opts->LEARN_en |
         opts->IOUT | opts->ACOC_threshold | opts->charge_inhibit;
}
void BQ24725_FormOptionsStruct(const uint16_t data,
                               BQ24725_charge_options *opt) {
  opt->ACOK_deglitch_time = data & BQ24725_ACOK_deglitch_time_MASK;
  opt->WATCHDOG_timer = data & BQ24725_WATCHDOG_timer_MASK;
  opt->BAT_depletion_threshold = data & BQ24725_BAT_depletion_threshold_MASK;
  opt->EMI_sw_freq_adj = data & BQ24725_EMI_sw_freq_adj_MASK;
  opt->EMI_sw_freq_adj_en = data & BQ24725_EMI_sw_freq_adj_en_MASK;
  opt->IFAULT_HI_threshold = data & BQ24725_IFAULT_HI_threshold_MASK;
  opt->LEARN_en = data & BQ24725_LEARN_en_MASK;
  opt->IOUT = data & BQ24725_IOUT_MASK;
  opt->ACOC_threshold = data & BQ24725_ACOC_threshold_MASK;
  opt->charge_inhibit = data & BQ24725_charge_inhibit_MASK;
}

void BQ24725_Init(BQ24725Config *conf) {
  configASSERT(conf);
  configASSERT(!s_initialized);
  s_conf = conf;
  s_initialized = true;
}

ErrorStatus BQ24725_SetChargeOption(const BQ24725_charge_options *option) {
  const uint16_t data = BQ24725_FormOptionsData(option);
  return I2C1_Mutex_WriteWord(BQ24725_ADDR, BQ24725_REG_CHARGE_OPTION, data);
}

ErrorStatus BQ24725_SetChargeCurrent(const uint16_t mA) {
  const uint16_t data = mA & CHARGE_CURRENT_MASK;
  return I2C1_Mutex_WriteWord(BQ24725_ADDR, BQ24725_REG_CHARGE_CURRENT, data);
}

ErrorStatus BQ24725_SetChargeVoltage(const uint16_t mV) {
  const uint16_t data = mV & CHARGE_VOLTAGE_MASK;
  return I2C1_Mutex_WriteWord(BQ24725_ADDR, BQ24725_REG_CHARGE_VOLTAGE, data);
}

ErrorStatus BQ24725_SetInputCurrent(const uint16_t mA) {
  const uint16_t data = mA & INPUT_CURRENT_MASK;
  return I2C1_Mutex_WriteWord(BQ24725_ADDR, BQ24725_REG_INPUT_CURRENT, data);
}

ErrorStatus BQ24725_GetDeviceID(uint16_t *data) {
  return I2C1_Mutex_ReadWord(BQ24725_ADDR, BQ24725_REG_DEVICE_ID, data);
}

ErrorStatus BQ24725_GetManufactureID(uint16_t *data) {
  return I2C1_Mutex_ReadWord(BQ24725_ADDR, BQ24725_REG_MANUFACTURE_ID, data);
}

ErrorStatus BQ24725_GetChargeCurrent(uint16_t *data) {
  return I2C1_Mutex_ReadWord(BQ24725_ADDR, BQ24725_REG_CHARGE_CURRENT, data);
}

ErrorStatus BQ24725_GetChargeVoltage(uint16_t *data) {
  return I2C1_Mutex_ReadWord(BQ24725_ADDR, BQ24725_REG_CHARGE_VOLTAGE, data);
}

ErrorStatus BQ24725_GetInputCurrent(uint16_t *data) {
  return I2C1_Mutex_ReadWord(BQ24725_ADDR, BQ24725_REG_INPUT_CURRENT, data);
}

ErrorStatus BQ24725_GetChargeOption(uint16_t *data) {
  return I2C1_Mutex_ReadWord(BQ24725_ADDR, BQ24725_REG_CHARGE_OPTION, data);
}

GPIO_PinState BQ24725_ACOK_Read(void) {
  return HAL_GPIO_ReadPin(s_conf->ACOK_GPIO_Port, s_conf->ACOK_GPIO_Pin);
}

uint16_t BQ24725_IMON_Read(void) {
  extern float ADC_GetBC_IOUT_Voltage(void);
  return (uint16_t)(ADC_GetBC_IOUT_Voltage() / 3.3f * 4095.0f);
}
