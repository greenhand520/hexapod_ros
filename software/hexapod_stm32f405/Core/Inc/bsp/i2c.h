//
// Created by greenhand520 on 2026/5/14.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"

extern SemaphoreHandle_t g_i2c1_mutex;
extern I2C_HandleTypeDef hi2c1;
extern SemaphoreHandle_t g_i2c2_mutex;
extern I2C_HandleTypeDef hi2c2;

#define I2C_MUTEX_TIMEOUT_MS pdMS_TO_TICKS(100)

ErrorStatus I2C1_Lock(void);

void I2C1_Unlock(void);

ErrorStatus I2C1_Mutex_ReadWord(uint16_t addr, uint8_t reg, uint16_t *data);

ErrorStatus I2C1_Mutex_ReadByte(uint16_t addr, uint8_t reg, uint8_t *data);

ErrorStatus I2C1_Mutex_WriteByte(uint16_t addr, uint8_t reg, uint8_t data);

ErrorStatus I2C1_Mutex_WriteWord(uint16_t addr, uint8_t reg, uint16_t data);

ErrorStatus I2C1_ReadWord(uint16_t addr, uint8_t reg, uint16_t *data);

ErrorStatus I2C1_ReadByte(uint16_t addr, uint8_t reg, uint8_t *data);

ErrorStatus I2C1_WriteByte(uint16_t addr, uint8_t reg, uint8_t data);

ErrorStatus I2C1_WriteWord(uint16_t addr, uint8_t reg, uint16_t data);

ErrorStatus I2C1_Mutex_ReadMultiple(uint16_t addr, uint8_t reg,
                                    uint8_t *data, uint16_t len);

ErrorStatus I2C1_ReadMultiple(uint16_t addr, uint8_t reg,
                              uint8_t *data, uint16_t len);


ErrorStatus I2C2_Lock(void);

void I2C2_Unlock(void);

ErrorStatus I2C2_Mutex_ReadWord(uint16_t addr, uint8_t reg, uint16_t *data);

ErrorStatus I2C2_Mutex_ReadByte(uint16_t addr, uint8_t reg, uint8_t *data);

ErrorStatus I2C2_Mutex_WriteWord(uint16_t addr, uint8_t reg, uint16_t data);

ErrorStatus I2C2_ReadWord(uint16_t addr, uint8_t reg, uint16_t *data);

ErrorStatus I2C2_ReadByte(uint16_t addr, uint8_t reg, uint8_t *data);

ErrorStatus I2C2_WriteWord(uint16_t addr, uint8_t reg, uint16_t data);

ErrorStatus I2C2_WriteByte(uint16_t addr, uint8_t reg, uint8_t data);

ErrorStatus I2C2_Mutex_WriteByte(uint16_t addr, uint8_t reg, uint8_t data);

ErrorStatus I2C2_Mutex_ReadMultiple(uint16_t addr, uint8_t reg,
                                    uint8_t *data, uint16_t len);

ErrorStatus I2C2_ReadMultiple(uint16_t addr, uint8_t reg,
                              uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif