//
// Created by greenhand520 on 2026/4/28.
//

#include "bq24725.h"

/*
 * BQ24725.c
 *
 * STM32 FreeRTOS + SMBus HAL driver for the BQ24725 Battery Charge Controller
 *
 * 硬件连接：
 *   BC_ACOK → PC13 (EXTI13, EXTI15_10_IRQn)
 *   BC_IOUT → PA6  (ADC1 Channel 6)
 *   SMBus   → CubeMX SMBus_PMBus_Stack
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32_SMBUS_stack.h"

#include <string.h>

/* ================================================================
 *  私有常量
 * ================================================================ */

/* SMBus 7-bit 从机地址 */
#define BQ24725_SMBUS_ADDR    (0x09u)

/* SMBus 传输超时 (ms) */
#define BQ24725_SMBUS_TIMEOUT (100u)

/* BQ24725 寄存器地址 */
typedef enum {
    BQ24725_REG_DEVICE_ID       = 0xFF,
    BQ24725_REG_MANUFACTURE_ID  = 0xFE,
    BQ24725_REG_CHARGE_CURRENT  = 0x14,
    BQ24725_REG_CHARGE_VOLTAGE  = 0x15,
    BQ24725_REG_INPUT_CURRENT   = 0x3F,
    BQ24725_REG_CHARGE_OPTION   = 0x12
} BQ24725_Register;

/* 数据掩码 */
#define CHARGE_CURRENT_MASK   0x1FC0
#define CHARGE_VOLTAGE_MASK   0x7FF0
#define INPUT_CURRENT_MASK    0x1F80

/* ================================================================
 *  私有变量
 * ================================================================ */

static BQ24725_Config *s_conf       = NULL;
static bool           s_initialized = false;
static uint16_t       s_imonbuf     = 0;

/* SMBus 互斥锁（多任务保护） */
static SemaphoreHandle_t s_smbus_mutex = NULL;

/* POR 默认配置 */
const BQ24725_charge_options BQ24725_charge_options_POR_default = {
    .ACOK_deglitch_time      = t150ms,
    .WATCHDOG_timer          = t175s,
    .BAT_depletion_threshold = FT70_97pct,
    .EMI_sw_freq_adj         = dec18pct,
    .EMI_sw_freq_adj_en      = sw_freq_adj_disable,
    .IFAULT_HI_threshold     = l700mV,
    .LEARN_en                = LEARN_disable,
    .IOUT                    = adapter_current,
    .ACOC_threshold          = l1_66X,
    .charge_inhibit          = charge_enable
};

/* ================================================================
 *  SMBus Word 底层读写
 *
 *  BQ24725 使用标准 SMBus word 协议：
 *    读: [S][ADDR+W][CMD][Sr][ADDR+R][DATA_L][DATA_H][P]
 *    写: [S][ADDR+W][CMD][DATA_L][DATA_H][P]
 *
 *  HAL_SMBUS_Mem_Read/Write 自动处理 repeated-start 和 PEC。
 *  传 7-bit 地址 0x09，HAL 内部自动处理 R/W 位。
 * ================================================================ */

static int SMBus_ReadWord(SMBUS_HandleTypeDef *hsmbus,
                          uint8_t cmd, uint16_t *data)
{
    uint8_t buf[2] = {0};
    HAL_StatusTypeDef ret;

    ret = HAL_SMBUS_Mem_Read(hsmbus,
                             BQ24725_SMBUS_ADDR,
                             cmd,
                             I2C_MEMADD_SIZE_8BIT,
                             buf, 2,
                             SMBUS_XFER_OPTIONS_LAST_NOSTOP,
                             BQ24725_SMBUS_TIMEOUT);
    if (ret != HAL_OK) {
        return BQ24725_ERROR;
    }

    /* BQ24725 返回小端序: byte0=低字节, byte1=高字节 */
    *data = (uint16_t)((buf[1] << 8) | buf[0]);
    return BQ24725_OK;
}

static int SMBus_WriteWord(SMBUS_HandleTypeDef *hsmbus,
                           uint8_t cmd, uint16_t data)
{
    uint8_t buf[2];
    HAL_StatusTypeDef ret;

    buf[0] = (uint8_t)(data & 0xFF);
    buf[1] = (uint8_t)((data >> 8) & 0xFF);

    ret = HAL_SMBUS_Mem_Write(hsmbus,
                              BQ24725_SMBUS_ADDR,
                              cmd,
                              I2C_MEMADD_SIZE_8BIT,
                              buf, 2,
                              SMBUS_XFER_OPTIONS_LAST_NOSTOP,
                              BQ24725_SMBUS_TIMEOUT);
    if (ret != HAL_OK) {
        return BQ24725_ERROR;
    }

    return BQ24725_OK;
}

/* ================================================================
 *  内部读写封装（寄存器合法性检查 + FreeRTOS 互斥锁）
 * ================================================================ */

static int BQ24725_ReadReg(uint8_t reg, uint16_t *data)
{
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
        configASSERT(0);
        return BQ24725_ERROR;
    }

    int ret;
    xSemaphoreTake(s_smbus_mutex, portMAX_DELAY);
    ret = SMBus_ReadWord(s_conf->hsmbus, reg, data);
    xSemaphoreGive(s_smbus_mutex);
    return ret;
}

static int BQ24725_WriteReg(uint8_t reg, uint16_t data)
{
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

    int ret;
    xSemaphoreTake(s_smbus_mutex, portMAX_DELAY);
    ret = SMBus_WriteWord(s_conf->hsmbus, reg, data);
    xSemaphoreGive(s_smbus_mutex);
    return ret;
}

/* ================================================================
 *  根据 GPIO_Pin 计算对应的 EXTI IRQn
 *
 *  PC13 → EXTI13 → EXTI15_10_IRQn
 *  PA0  → EXTI0  → EXTI0_IRQn
 *  ...以此类推
 * ================================================================ */

static IRQn_Type EXTI_GetIRQn(uint16_t GPIO_Pin)
{
    uint8_t pin_pos = 0;
    uint16_t tmp = GPIO_Pin;
    while (tmp >>= 1) { pin_pos++; }

    if (pin_pos <= 4) {
        /* EXTI0 ~ EXTI4 各自独立 IRQn */
        return (IRQn_Type)(EXTI0_IRQn + pin_pos);
    } else if (pin_pos <= 9) {
        /* EXTI5 ~ EXTI9 共享 EXTI9_5_IRQn */
        return EXTI9_5_IRQn;
    } else {
        /* EXTI10 ~ EXTI15 共享 EXTI15_10_IRQn */
        return EXTI15_10_IRQn;
    }
}

/* ================================================================
 *  ACOK 外部中断回调入口
 *
 *  用户需在 CubeMX 生成的中断回调中调用：
 *
 *    void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 *    {
 *        if (GPIO_Pin == BQ24725_ACOK_PIN) {
 *            BQ24725_EXTI_Callback();
 *        }
 *    }
 * ================================================================ */

void BQ24725_EXTI_Callback(void)
{
    if (s_conf && s_conf->ACOK_cb) {
        s_conf->ACOK_cb();
    }
}

/* ================================================================
 *  ADC DMA 连续采集完成回调
 * ================================================================ */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (s_conf && hadc->Instance == s_conf->hadc->Instance) {
        s_imonbuf = (uint16_t)HAL_ADC_GetValue(s_conf->hadc);
    }
}

/* ================================================================
 *  BQ24725_Init —— 初始化
 *
 *  参数 conf 中包含：
 *    - pins.ACOK  → ACOK 引脚（PC13）
 *    - pins.IOUT  → IOUT/IMON 引脚（PA6）
 *    - ACOK_cb    → ACOK 边沿中断回调
 *    - hsmbus     → CubeMX 生成的 SMBUS_HandleTypeDef
 *    - hadc       → CubeMX 生成的 ADC_HandleTypeDef
 * ================================================================ */

void BQ24725_Init(BQ24725_Config *conf)
{
    configASSERT(conf);
    configASSERT(conf->hsmbus);
    configASSERT(conf->hadc);
    configASSERT(!s_initialized);

    s_conf = conf;

    /* 创建 SMBus 互斥锁 */
    s_smbus_mutex = xSemaphoreCreateMutex();
    configASSERT(s_smbus_mutex);

    /* ---- ACOK GPIO (PC13) 配置为双边沿中断 ---- */
    GPIO_InitTypeDef gpio_acok = {0};
    gpio_acok.Pin   = s_conf->pins.ACOK.pin;
    gpio_acok.Mode  = GPIO_MODE_IT_RISING_FALLING;
    gpio_acok.Pull  = GPIO_PULLDOWN;
    gpio_acok.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(s_conf->pins.ACOK.port, &gpio_acok);

    /* 使能 EXTI 中断
     * PC13 → EXTI13 → EXTI15_10_IRQn
     * NVIC 优先级 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (通常为5)
     */
    IRQn_Type acok_irqn = EXTI_GetIRQn(s_conf->pins.ACOK.pin);
    HAL_NVIC_SetPriority(acok_irqn, 6, 0);
    HAL_NVIC_EnableIRQ(acok_irqn);

    /* ---- IOUT/IMON ADC (PA6 → ADC1 Channel 6) 配置 ---- */
    /*
     * 注意：PA6 的 GPIO 模式由 CubeMX 在 MX_ADC1_Init() 中配置为
     *       Analog 模式，此处无需重复配置 GPIO。
     *       ADC 通道在 CubeMX 的 ADC1 配置中已设定为 Channel 6。
     *       如果需要动态切换通道，可在此处调用 HAL_ADC_ConfigChannel。
     */

    /* 启动 ADC 连续 + DMA 采集 */
    HAL_ADC_Start_DMA(s_conf->hadc, (uint32_t *)&s_imonbuf, 1);

    s_initialized = true;
}

/* ================================================================
 *  公开 API
 * ================================================================ */

/* ---- 设备/厂商 ID ---- */

int BQ24725_GetDeviceID(uint16_t *data)
{
    return BQ24725_ReadReg(BQ24725_REG_DEVICE_ID, data);
}

int BQ24725_GetManufactureID(uint16_t *data)
{
    return BQ24725_ReadReg(BQ24725_REG_MANUFACTURE_ID, data);
}

/* ---- 充电电流 ---- */

int BQ24725_GetChargeCurrent(uint16_t *data)
{
    return BQ24725_ReadReg(BQ24725_REG_CHARGE_CURRENT, data);
}

int BQ24725_SetChargeCurrent(uint16_t mA)
{
    uint16_t data = mA & CHARGE_CURRENT_MASK;
    return BQ24725_WriteReg(BQ24725_REG_CHARGE_CURRENT, data);
}

/* ---- 充电电压 ---- */

int BQ24725_GetChargeVoltage(uint16_t *data)
{
    return BQ24725_ReadReg(BQ24725_REG_CHARGE_VOLTAGE, data);
}

int BQ24725_SetChargeVoltage(uint16_t mV)
{
    uint16_t data = mV & CHARGE_VOLTAGE_MASK;
    return BQ24725_WriteReg(BQ24725_REG_CHARGE_VOLTAGE, data);
}

/* ---- 输入电流限制 ---- */

int BQ24725_GetInputCurrent(uint16_t *data)
{
    return BQ24725_ReadReg(BQ24725_REG_INPUT_CURRENT, data);
}

int BQ24725_SetInputCurrent(uint16_t mA)
{
    uint16_t data = mA & INPUT_CURRENT_MASK;
    return BQ24725_WriteReg(BQ24725_REG_INPUT_CURRENT, data);
}

/* ---- Charge Option ---- */

int BQ24725_GetChargeOption(uint16_t *data)
{
    return BQ24725_ReadReg(BQ24725_REG_CHARGE_OPTION, data);
}

int BQ24725_SetChargeOption(BQ24725_charge_options *option)
{
    uint16_t data = BQ24725_FormOptionsData(option);
    return BQ24725_WriteReg(BQ24725_REG_CHARGE_OPTION, data);
}

/* ---- ACOK 电平读取 ---- */

int BQ24725_ACOK_Read(void)
{
    return (int)HAL_GPIO_ReadPin(s_conf->pins.ACOK.port,
                                 s_conf->pins.ACOK.pin);
}

/* ---- IMON ADC 值读取 ---- */

uint16_t BQ24725_IMON_Read(void)
{
    return s_imonbuf;
}