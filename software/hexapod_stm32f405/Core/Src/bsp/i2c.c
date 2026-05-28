//
// Created by greenhand520 on 2026/5/14.
//

#include "FreeRTOS.h"
#include "portmacro.h"
#include "semphr.h"

#include "bsp/i2c.h"

/** I2C 传输超时 (ms) */
#define I2C_TIMEOUT (100u)

ErrorStatus I2C_ReadWord(I2C_HandleTypeDef *hi2c, const uint16_t addr,
                         const uint8_t reg, uint16_t *data) {
  uint8_t buf[2] = {0};

  const HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(
      hi2c, addr, reg, I2C_MEMADD_SIZE_8BIT, buf, 2, I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return ERROR;
  }

  *data = (uint16_t)((buf[1] << 8) | buf[0]);
  return SUCCESS;
}

ErrorStatus I2C_ReadByte(I2C_HandleTypeDef *hi2c, const uint16_t addr,
                         const uint8_t reg, uint8_t *data) {
  uint8_t buf[1] = {0};

  const HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(
      hi2c, addr, reg, I2C_MEMADD_SIZE_8BIT, buf, 1, I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return ERROR;
  }

  *data = buf[0];
  return SUCCESS;
}

ErrorStatus I2C_WriteWord(I2C_HandleTypeDef *hi2c, const uint16_t addr,
                          const uint8_t reg, const uint16_t data) {
  uint8_t buf[2];

  buf[0] = (uint8_t)(data & 0xFF);
  buf[1] = (uint8_t)((data >> 8) & 0xFF);

  const HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(
      hi2c, addr, reg, I2C_MEMADD_SIZE_8BIT, buf, 2, I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return ERROR;
  }
  return SUCCESS;
}

ErrorStatus I2C_WriteByte(I2C_HandleTypeDef *hi2c, const uint16_t addr,
                          const uint8_t reg, const uint8_t data) {
  uint8_t buf[1];

  buf[0] = data;

  const HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(
      hi2c, addr, reg, I2C_MEMADD_SIZE_8BIT, buf, 1, I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return ERROR;
  }
  return SUCCESS;
}

/**
 * @brief I2C Read Word（2 字节，小端序），自带互斥锁
 *
 * @param hi2c i2c句柄
 * @param bus_mutex 共享锁
 * @param addr 设备地址
 * @param reg 寄存器
 * @param data 数据
 * @return 操作结果状态
 */
ErrorStatus I2C_Mutex_ReadWord(I2C_HandleTypeDef *hi2c,
                               const SemaphoreHandle_t bus_mutex,
                               const uint16_t addr, const uint8_t reg,
                               uint16_t *data) {
  xSemaphoreTake(bus_mutex, portMAX_DELAY);
  const ErrorStatus et = I2C_ReadWord(hi2c, addr, reg, data);
  xSemaphoreGive(bus_mutex);
  return et;
}

ErrorStatus I2C_Mutex_ReadByte(I2C_HandleTypeDef *hi2c,
                               const SemaphoreHandle_t bus_mutex,
                               const uint16_t addr, const uint8_t reg,
                               uint8_t *data) {
  xSemaphoreTake(bus_mutex, portMAX_DELAY);
  const ErrorStatus et = I2C_ReadByte(hi2c, addr, reg, data);
  xSemaphoreGive(bus_mutex);
  return et;
}

ErrorStatus I2C_Mutex_WriteWord(I2C_HandleTypeDef *hi2c,
                                const SemaphoreHandle_t bus_mutex,
                                const uint16_t addr, const uint8_t reg,
                                const uint16_t data) {
  xSemaphoreTake(bus_mutex, portMAX_DELAY);
  const ErrorStatus et = I2C_WriteWord(hi2c, addr, reg, data);
  xSemaphoreGive(bus_mutex);
  return et;
}

ErrorStatus I2C_Mutex_WriteByte(I2C_HandleTypeDef *hi2c,
                                const SemaphoreHandle_t bus_mutex,
                                const uint16_t addr, const uint8_t reg,
                                const uint8_t data) {
  xSemaphoreTake(bus_mutex, portMAX_DELAY);
  const ErrorStatus et = I2C_WriteByte(hi2c, addr, reg, data);
  xSemaphoreGive(bus_mutex);
  return et;
}

ErrorStatus I2C1_Lock(void) {
  if (g_i2c1_mutex == NULL) {
    return SUCCESS;
  }
  return (xSemaphoreTake(g_i2c1_mutex, I2C_MUTEX_TIMEOUT_MS) == pdTRUE) ? SUCCESS
                                                                      : ERROR;
}

void I2C1_Unlock(void) {
  if (g_i2c1_mutex != NULL) {
    xSemaphoreGive(g_i2c1_mutex);
  }
}

ErrorStatus I2C1_Mutex_ReadWord(const uint16_t addr, const uint8_t reg,
                                uint16_t *data) {
  return I2C_Mutex_ReadWord(&hi2c1, g_i2c1_mutex, addr, reg, data);
}

ErrorStatus I2C1_Mutex_ReadByte(const uint16_t addr, const uint8_t reg,
                                uint8_t *data) {
  return I2C_Mutex_ReadByte(&hi2c1, g_i2c1_mutex, addr, reg, data);
}

ErrorStatus I2C1_Mutex_WriteByte(const uint16_t addr, const uint8_t reg,
                                 const uint8_t data) {
  return I2C_Mutex_WriteByte(&hi2c1, g_i2c1_mutex, addr, reg, data);
}

ErrorStatus I2C1_Mutex_WriteWord(const uint16_t addr, const uint8_t reg,
                                 const uint16_t data) {
  return I2C_Mutex_WriteWord(&hi2c1, g_i2c1_mutex, addr, reg, data);
}

ErrorStatus I2C1_ReadWord(const uint16_t addr, const uint8_t reg,
                          uint16_t *data) {
  return I2C_ReadWord(&hi2c1, addr, reg, data);
}

ErrorStatus I2C1_ReadByte(const uint16_t addr, const uint8_t reg,
                          uint8_t *data) {
  return I2C_ReadByte(&hi2c1, addr, reg, data);
}

ErrorStatus I2C1_WriteByte(const uint16_t addr, const uint8_t reg,
                           const uint8_t data) {
  return I2C_WriteByte(&hi2c1, addr, reg, data);
}

ErrorStatus I2C1_WriteWord(const uint16_t addr, const uint8_t reg,
                           const uint16_t data) {
  return I2C_WriteWord(&hi2c1, addr, reg, data);
}


ErrorStatus I2C_ReadMultiple(I2C_HandleTypeDef *hi2c, const uint16_t addr,
                             const uint8_t reg, uint8_t *data,
                             const uint16_t len) {
  const HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(
      hi2c, addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, I2C_TIMEOUT);
  if (ret != HAL_OK) {
    return ERROR;
  }
  return SUCCESS;
}

ErrorStatus I2C_Mutex_ReadMultiple(I2C_HandleTypeDef *hi2c,
                                   const SemaphoreHandle_t bus_mutex,
                                   const uint16_t addr, const uint8_t reg,
                                   uint8_t *data, const uint16_t len) {
  xSemaphoreTake(bus_mutex, portMAX_DELAY);
  const ErrorStatus et = I2C_ReadMultiple(hi2c, addr, reg, data, len);
  xSemaphoreGive(bus_mutex);
  return et;
}

ErrorStatus I2C1_Mutex_ReadMultiple(const uint16_t addr, const uint8_t reg,
                                    uint8_t *data, const uint16_t len) {
  return I2C_Mutex_ReadMultiple(&hi2c1, g_i2c1_mutex, addr, reg, data, len);
}

ErrorStatus I2C1_ReadMultiple(const uint16_t addr, const uint8_t reg,
                              uint8_t *data, const uint16_t len) {
  return I2C_ReadMultiple(&hi2c1, addr, reg, data, len);
}

ErrorStatus I2C2_Lock(void) {
  if (g_i2c2_mutex == NULL) {
    return SUCCESS;
  }
  return (xSemaphoreTake(g_i2c2_mutex, I2C_MUTEX_TIMEOUT_MS) == pdTRUE) ? SUCCESS
                                                                      : ERROR;
}

void I2C2_Unlock(void) {
  if (g_i2c2_mutex != NULL) {
    xSemaphoreGive(g_i2c2_mutex);
  }
}

ErrorStatus I2C2_Mutex_ReadWord(const uint16_t addr, const uint8_t reg,
                                uint16_t *data) {
  return I2C_Mutex_ReadWord(&hi2c2, g_i2c2_mutex, addr, reg, data);
}

ErrorStatus I2C2_Mutex_ReadByte(const uint16_t addr, const uint8_t reg,
                                uint8_t *data) {
  return I2C_Mutex_ReadByte(&hi2c2, g_i2c2_mutex, addr, reg, data);
}

ErrorStatus I2C2_Mutex_WriteWord(const uint16_t addr, const uint8_t reg,
                                 const uint16_t data) {
  return I2C_Mutex_WriteWord(&hi2c2, g_i2c2_mutex, addr, reg, data);
}

ErrorStatus I2C2_ReadWord(const uint16_t addr, const uint8_t reg,
                          uint16_t *data) {
  return I2C_ReadWord(&hi2c2, addr, reg, data);
}

ErrorStatus I2C2_ReadByte(const uint16_t addr, const uint8_t reg,
                          uint8_t *data) {
  return I2C_ReadByte(&hi2c2, addr, reg, data);
}

ErrorStatus I2C2_WriteByte(const uint16_t addr, const uint8_t reg,
                           const uint8_t data) {
  return I2C_WriteByte(&hi2c2, addr, reg, data);
}

ErrorStatus I2C2_Mutex_WriteByte(const uint16_t addr, const uint8_t reg,
                                 const uint8_t data) {
  return I2C_Mutex_WriteByte(&hi2c2, g_i2c1_mutex, addr, reg, data);
}

ErrorStatus I2C2_WriteWord(const uint16_t addr, const uint8_t reg,
                           const uint16_t data) {
  return I2C_WriteWord(&hi2c2, addr, reg, data);
}

ErrorStatus I2C2_Mutex_ReadMultiple(const uint16_t addr, const uint8_t reg,
                                    uint8_t *data, const uint16_t len) {
  return I2C_Mutex_ReadMultiple(&hi2c2, g_i2c1_mutex, addr, reg, data, len);
}

ErrorStatus I2C2_ReadMultiple(const uint16_t addr, const uint8_t reg,
                              uint8_t *data, const uint16_t len) {
  return I2C_ReadMultiple(&hi2c2, addr, reg, data, len);
}