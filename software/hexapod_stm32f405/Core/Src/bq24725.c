//
// Created by greenhand520 on 2026/4/28.
//

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdbool.h>
#include <string.h>

#include "bq24725.h"

#define BQ24725_ADDR (0x09u) /* 7-bit 地址 0b0001001 */
#define BQ24725_ADDR_WRITE ((BQ24725_ADDR << 1) | 0)
#define BQ24725_ADDR_READ ((BQ24725_ADDR << 1) | 1)
#define BQ24725_I2C_TIMEOUT (100u) /* ms */

/* 寄存器地址 */
#define BQ24725_REG_DEVICE_ID       0xFF
#define BQ24725_REG_MANUFACTURE_ID  0xFE
#define BQ24725_REG_CHARGE_CURRENT  0x14
#define BQ24725_REG_CHARGE_VOLTAGE  0x15
#define BQ24725_REG_INPUT_CURRENT   0x3F
#define BQ24725_REG_CHARGE_OPTION   0x12


/* 数据掩码 */
#define CHARGE_CURRENT_MASK 0x1FC0
#define CHARGE_VOLTAGE_MASK 0x7FF0
#define INPUT_CURRENT_MASK 0x1F80

static BQ24725Config *s_conf = NULL;
static bool s_initialized = false;
static uint16_t s_imonbuf = 0;

/* POR 默认配置 */
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

/* ================================================================
 *  SMBus 读写（基于 STM32 HAL I2C）
 *
 *  SMBus word 读: [S][ADDR+W][REG][Sr][ADDR+R][DATA_L][DATA_H][P]
 *  SMBus word 写: [S][ADDR+W][REG][DATA_L][DATA_H][P]
 * ================================================================ */

static int BQ24725_I2CReadWord(I2C_HandleTypeDef *hi2c, const uint8_t reg,
                          uint16_t *data) {
  uint8_t buf[2] = {0};

  const HAL_StatusTypeDef ret =
      HAL_I2C_Mem_Read(hi2c, BQ24725_ADDR_WRITE, reg, I2C_MEMADD_SIZE_8BIT, buf,
                       2, BQ24725_I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return BQ24725_ERROR;
  }

  *data = (uint16_t)((buf[1] << 8) | buf[0]);
  return BQ24725_OK;
}

static int BQ24725_I2CWriteWord(I2C_HandleTypeDef *hi2c, const uint8_t reg,
                           const uint16_t data) {
  uint8_t buf[2];

  buf[0] = (uint8_t)(data & 0xFF);
  buf[1] = (uint8_t)((data >> 8) & 0xFF);

  const HAL_StatusTypeDef ret =
      HAL_I2C_Mem_Write(hi2c, BQ24725_ADDR_WRITE, reg, I2C_MEMADD_SIZE_8BIT,
                        buf, 2, BQ24725_I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return BQ24725_ERROR;
  }

  return BQ24725_OK;
}

/* ================================================================
 *  内部读写封装（带寄存器合法性检查 + 互斥锁）
 * ================================================================ */

static int BQ24725_ReadReg(const uint8_t reg, uint16_t *data) {
  configASSERT(s_initialized);

  switch (reg) {
  case BQ24725_REG_DEVICE_ID:
  case BQ24725_REG_MANUFACTURE_ID:
  case BQ24725_REG_CHARGE_CURRENT:
  case BQ24725_REG_CHARGE_VOLTAGE:
  case BQ24725_REG_INPUT_CURRENT:
  case BQ24725_REG_CHARGE_OPTION:
    break;
  default:
    configASSERT(0); /* 未知寄存器 */
    return BQ24725_ERROR;
  }

  xSemaphoreTake(s_conf->bus_mutex, portMAX_DELAY);
  const int ret = BQ24725_I2CReadWord(s_conf->hi2c, reg, data);
  xSemaphoreGive(s_conf->bus_mutex);
  return ret;
}

static int BQ24725_WriteReg(const uint8_t reg, const uint16_t data) {
  configASSERT(s_initialized);

  switch (reg) {
  case BQ24725_REG_CHARGE_CURRENT:
  case BQ24725_REG_CHARGE_VOLTAGE:
  case BQ24725_REG_INPUT_CURRENT:
  case BQ24725_REG_CHARGE_OPTION:
    break;
  default:
    configASSERT(0);
    return BQ24725_ERROR;
  }

  xSemaphoreTake(s_conf->bus_mutex, portMAX_DELAY);
  const int ret = BQ24725_I2CWriteWord(s_conf->hi2c, reg, data);
  xSemaphoreGive(s_conf->bus_mutex);
  return ret;
}

/* ================================================================
 *  ACOK 外部中断回调（需在 stm32f4xx_it.c 的 EXTI 回调中调用）
 * ================================================================ */

/*
 * 使用方法：在 CubeMX 生成的 HAL_GPIO_EXTI_Callback 中调用：
 *
 *   void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 *   {
 *       if (GPIO_Pin == BQ24725_ACOK_PIN) {
 *           BQ24725_EXTI_Callback();
 *       }
 *   }
 */
void BQ24725_EXTI_Callback(void) {
  if (s_conf && s_conf->ACOK_cb) {
    s_conf->ACOK_cb();
  }
}

/* ================================================================
 *  公开 API
 * ================================================================ */

uint16_t BQ24725_FormOptionsData(const BQ24725_charge_options *opts) {
  return opts->ACOK_deglitch_time | opts->WATCHDOG_timer |
         opts->BAT_depletion_threshold | opts->EMI_sw_freq_adj |
         opts->EMI_sw_freq_adj_en | opts->IFAULT_HI_threshold | opts->LEARN_en |
         opts->IOUT | opts->ACOC_threshold | opts->charge_inhibit;
}
void BQ24725_FormOptionsStruct(const uint16_t data, BQ24725_charge_options *opt) {
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

void BQ24725_Start(BQ24725Config *conf) {
  configASSERT(conf);
  configASSERT(!s_initialized);

  s_conf = conf;

  /* 创建 I2C 互斥锁 */
  // s_i2c_mutex = xSemaphoreCreateMutex();
  // s_i2c_mutex = conf->bus_mutex;
  // configASSERT(s_i2c_mutex);

  /* ---- ACOK GPIO 配置 ---- */
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = s_conf->ACOK_GPIO_Pin;
  gpio.Mode = GPIO_MODE_IT_RISING_FALLING; /* 双边沿中断 */
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(s_conf->ACOK_GPIO_Port, &gpio);

  /* 使能 EXTI 中断（NVIC），用户需在 CubeMX 中正确配置 EXTI line */
  /* 这里以 pin 对应的 EXTI line 为例自动计算 IRQn */
  uint8_t pin_pos = 0;
  uint16_t pin = s_conf->ACOK_GPIO_Pin;
  while (pin >>= 1) {
    pin_pos++;
  }
  const IRQn_Type irqn = (IRQn_Type)(EXTI0_IRQn + pin_pos);
  HAL_NVIC_SetPriority(irqn, 6, 0); /* FreeRTOS 推荐优先级 >= 5 */
  HAL_NVIC_EnableIRQ(irqn);

  s_initialized = true;
}

/* ---- 设备/厂商 ID ---- */

int BQ24725_GetDeviceID(uint16_t *data) {
  return BQ24725_ReadReg(BQ24725_REG_DEVICE_ID, data);
}

int BQ24725_GetManufactureID(uint16_t *data) {
  return BQ24725_ReadReg(BQ24725_REG_MANUFACTURE_ID, data);
}

/* ---- 充电电流 ---- */

int BQ24725_GetChargeCurrent(uint16_t *data) {
  return BQ24725_ReadReg(BQ24725_REG_CHARGE_CURRENT, data);
}

int BQ24725_SetChargeCurrent(const uint16_t mA) {
  const uint16_t data = mA & CHARGE_CURRENT_MASK;
  return BQ24725_WriteReg(BQ24725_REG_CHARGE_CURRENT, data);
}

/* ---- 充电电压 ---- */

int BQ24725_GetChargeVoltage(uint16_t *data) {
  return BQ24725_ReadReg(BQ24725_REG_CHARGE_VOLTAGE, data);
}

int BQ24725_SetChargeVoltage(const uint16_t mV) {
  const uint16_t data = mV & CHARGE_VOLTAGE_MASK;
  return BQ24725_WriteReg(BQ24725_REG_CHARGE_VOLTAGE, data);
}

/* ---- 输入电流限制 ---- */

int BQ24725_GetInputCurrent(uint16_t *data) {
  return BQ24725_ReadReg(BQ24725_REG_INPUT_CURRENT, data);
}

int BQ24725_SetInputCurrent(const uint16_t mA) {
  const uint16_t data = mA & INPUT_CURRENT_MASK;
  return BQ24725_WriteReg(BQ24725_REG_INPUT_CURRENT, data);
}

/* ---- Charge Option ---- */

int BQ24725_GetChargeOption(uint16_t *data) {
  return BQ24725_ReadReg(BQ24725_REG_CHARGE_OPTION, data);
}

int BQ24725_SetChargeOption(const BQ24725_charge_options *option) {
  const uint16_t data = BQ24725_FormOptionsData(option);
  return BQ24725_WriteReg(BQ24725_REG_CHARGE_OPTION, data);
}

/* ---- ACOK 电平读取 ---- */

int BQ24725_ACOK_Read(void) {
  return (int)HAL_GPIO_ReadPin(s_conf->ACOK_GPIO_Port, s_conf->ACOK_GPIO_Pin);
}

/* ---- IMON ADC 值读取 ---- */
/* 注意：BQ24725 充电电流现在统一由 adc.c 通过 DMA 多通道采集
 * 使用 ADC_GetBC_IOUT_mA() 或 ADC_GetBC_IOUT_Voltage() 代替
 * 保留此函数用于向后兼容 */
uint16_t BQ24725_IMON_Read(void) {
  extern float ADC_GetBC_IOUT_Voltage(void);
  /* 返回电压值的 ADC 原始值近似 (保留向后兼容) */
  return (uint16_t)(ADC_GetBC_IOUT_Voltage() / 3.3f * 4095.0f);
}
