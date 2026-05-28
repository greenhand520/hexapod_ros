//
// Created by eartholnpc on 2026/5/26.
//

/**
 * @brief HUSB238A PD Sink Controller Driver for STM32 HAL + FreeRTOS
 *
 * Feature:
 * 1. PD/PPS protocol detection
 * 2. Auto-request the highest available voltage (20V-28V)
 * 3. Fault detection and reporting
 * 4. EXTI interrupt handling with latched interrupt clearing
 *
 * Hardware Requirements:
 * - STM32 MCU (I2C2 + FreeRTOS)
 * - HUSB238A in I2C mode (ADDR pin via 900k resistor to VDD or GND)
 * - HUSB238A EN_N pin pulled low to enable
 */

#include "cmsis_os.h"

#include <string.h>

#include "bsp/husb238a.h"
#include "bsp/i2c.h"
#include "log.h"

static uint8_t device_addr = HUSB238A_ADDR_GND;

/** 当前实际协商的协议 (来自 CONTRACT_STATUS 寄存器) */
static HUSB238A_ChargerProtocol contract_chg_protocol = UNKNOW;
/** 当前实际协商的电压 (mV) */
static uint16_t contract_chg_voltage_mv = 0;
/** 当前实际协商的电流 (mA) */
static float contract_chg_current_ma = 0.0f;

/** 扫描到的最佳PDO选择码 (写入SRC_PDO[7:3])，代表"我想要的PDO" */
static uint8_t best_pdo_code = 0;
/** 扫描到的最佳PDO协议类型 */
static HUSB238A_ChargerProtocol best_protocol = UNKNOW;
/** 扫描到的最佳PDO电压 (mV)，0表示PPS(由Source决定) */
static uint16_t best_voltage_mv = 0;
/** 扫描到的最佳PDO电流 (mA) */
static uint16_t best_current_ma = 0;

/** 是否检测结束、拔掉充电器会变成false */
static bool charger_detected = false;

/* 是否发生错误 */
static bool is_fault = false;

/**
 * @brief 无锁I2C操作（调用方需在外部通过I2C2_Lock/Unlock管理总线互斥）
 */
static ErrorStatus HUSB238A_ReadReg(const uint8_t reg, uint8_t *val) {
  return I2C2_ReadByte(device_addr, reg, val);
}

/**
 * @brief 写入单个寄存器
 */
static ErrorStatus HUSB238A_WriteReg(const uint8_t reg, const uint8_t val) {
  return I2C2_WriteByte(device_addr, reg, val);
}

/**
 * @brief 无锁版本的IsAttached检查（用于已持锁或ISR上下文）
 */
static bool HUSB238A_IsAttachedRaw(void) {
  uint8_t status = 0;
  if (HUSB238A_ReadReg(HUSB238A_STATUS, &status) != SUCCESS) {
    return false;
  }
  return (status & HUSB238A_STATUS_ATTACH_Msk) != 0;
}

/**
 * @brief 无锁版本的IsFault检查（用于已持锁或ISR上下文）
 */
static ErrorStatus HUSB238A_IsFaultRaw(bool *fault) {
  if (fault == NULL) {
    return ERROR;
  }
  *fault = false;
  uint8_t status1 = 0;

  if (HUSB238A_ReadReg(HUSB238A_STATUS1, &status1) != SUCCESS) {
    return ERROR;
  }
  if (!(status1 & HUSB238A_STATUS1_FAULT_Msk)) {
    return SUCCESS;
  }

  *fault = true;
  uint8_t int_reg = 0, int1_reg = 0, int2_reg = 0;
  HUSB238A_ReadReg(HUSB238A_INTERRUPT, &int_reg);
  HUSB238A_ReadReg(HUSB238A_INTERRUPT1, &int1_reg);
  HUSB238A_ReadReg(HUSB238A_INTERRUPT2, &int2_reg);

  if (int1_reg & (1 << HUSB238A_INT1_I_VBUS_OV_Pos))  LOG_ERR("HUSB238A: VBUS OV!");
  if (int2_reg & (1 << HUSB238A_INT2_I_VBUS_UV_Pos))   LOG_ERR("HUSB238A: VBUS UV!");
  if (int1_reg & (1 << HUSB238A_INT1_I_FAULT_Pos))      LOG_ERR("HUSB238A: FAULT!");
  if (int2_reg & (1 << HUSB238A_INT2_I_TSD_Pos))        LOG_ERR("HUSB238A: TSD!");
  if (int_reg  & (1 << HUSB238A_INT_I_Go_Fail_Pos))     LOG_ERR("HUSB238A: Go Fail!");
  if (int2_reg & (1 << HUSB238A_INT2_I_FRC_FAIL_Pos))   LOG_ERR("HUSB238A: FRC Fail!");

  if (int_reg)  HUSB238A_WriteReg(HUSB238A_INTERRUPT, int_reg);
  if (int1_reg) HUSB238A_WriteReg(HUSB238A_INTERRUPT1, int1_reg);
  if (int2_reg) HUSB238A_WriteReg(HUSB238A_INTERRUPT2, int2_reg);

  return SUCCESS;
}

/**
 * @brief 将PD_CONTRACT编码转换为电压(mV)
 * @return 电压值(mV)，0表示未知
 */
static uint16_t HUSB238A_PDContractToVoltageMv(const uint8_t pd_contract) {
  switch (pd_contract) {
  case HUSB238A_PD_CONTRACT_TYPEC_5V:
  case HUSB238A_PD_CONTRACT_5V:
    return 5000;
  case HUSB238A_PD_CONTRACT_9V:
    return 9000;
  case HUSB238A_PD_CONTRACT_12V:
    return 12000;
  case HUSB238A_PD_CONTRACT_15V:
    return 15000;
  case HUSB238A_PD_CONTRACT_20V:
    return 20000;
  case HUSB238A_PD_CONTRACT_PPS1:
  case HUSB238A_PD_CONTRACT_PPS2:
  case HUSB238A_PD_CONTRACT_PPS3:
    return 0; // PPS电压需要另外读取
  case HUSB238A_PD_CONTRACT_AVS:
    return 0; // AVS电压需要另外读取
  case HUSB238A_PD_CONTRACT_28V:
    return 28000;
  case HUSB238A_PD_CONTRACT_36V:
    return 36000;
  case HUSB238A_PD_CONTRACT_48V:
    return 48000;
  case HUSB238A_PD_CONTRACT_EPR_AVS:
    return 0;
  default:
    return 0;
  }
}

/**
 * @brief 将CONTRACT_STATUS1的电流编码转换为mA
 *
 * 编码方式:
 *   0x00-0x7D: 20mA/LSB, offset 500mA
 *   0x7E-0xFF: 40mA/LSB, continuation from 3000mA
 */
static uint32_t HUSB238A_ContractCurrentToMa(const uint8_t raw) {
  if (raw <= 0x7D) {
    return 500 + (uint32_t)raw * 20;
  }
  return 3000 + ((uint32_t)raw - 0x7D) * 40;
}

/**
 * @brief 将SRC_PDO电流编码转换为mA (100mA/LSB)
 */
static uint16_t HUSB238A_SrcPdoCurrentToMa(const uint8_t raw) {
  return (uint16_t)(raw & 0x7F) * 100;
}

/**
 * @brief 清除所有中断标志 (写1清除)
 */
static void HUSB238A_ClearAllInterrupts(void) {
  HUSB238A_WriteReg(HUSB238A_INTERRUPT, 0xFF);
  HUSB238A_WriteReg(HUSB238A_INTERRUPT1, 0xFF);
  HUSB238A_WriteReg(HUSB238A_INTERRUPT2, 0xFF);
}

/**
 * @brief 扫描SourceCap PDO，找出19V~30V范围内最佳PDO并存储
 *        同时更新当前合约信息
 *
 * 扫描优先级: 28V FPDO > 20V FPDO > PPS(16V<=最大电压<=28V)
 * 最佳PDO信息存储在 best_pdo_code / best_protocol / best_voltage_mv / best_current_ma
 * 当前合约信息存储在 contract_chg_protocol / contract_chg_voltage_mv / contract_chg_current_ma
 */
static void HUSB238A_UpdateContractInfo(void) {
  uint8_t contract0 = 0, contract1 = 0;

  // ---- Part 1: 更新当前合约 ----
  if (HUSB238A_ReadReg(HUSB238A_CONTRACT_STATUS0, &contract0) == SUCCESS &&
      HUSB238A_ReadReg(HUSB238A_CONTRACT_STATUS1, &contract1) == SUCCESS) {

    const uint8_t pd_contract = (contract0 & HUSB238A_CONTRACT_PD_Msk) >> HUSB238A_CONTRACT_PD_Pos;

    if (pd_contract != HUSB238A_PD_CONTRACT_TYPEC_5V) {
      if (pd_contract >= HUSB238A_PD_CONTRACT_PPS1 &&
          pd_contract <= HUSB238A_PD_CONTRACT_PPS3) {
        contract_chg_protocol = PPS;
        contract_chg_current_ma = (float)contract1 * 0.05f;
      } else {
        contract_chg_protocol = PD;
        contract_chg_current_ma = (float)HUSB238A_ContractCurrentToMa(contract1);
      }
      contract_chg_voltage_mv = HUSB238A_PDContractToVoltageMv(pd_contract);
    } else {
      contract_chg_protocol = UNKNOW;
      contract_chg_voltage_mv = 0;
      contract_chg_current_ma = 0.0f;
    }
  }

  // ---- Part 2: 扫描SourceCap，找最佳PDO ----
  uint8_t reg_val = 0;
  best_pdo_code = 0;
  best_protocol = UNKNOW;
  best_voltage_mv = 0;
  best_current_ma = 0;

  // 优先级1: 28V FPDO (EPR)
  if (HUSB238A_ReadReg(HUSB238A_SRC_PDO_28V, &reg_val) == SUCCESS) {
    if (reg_val & HUSB238A_SRC_PDO_DETECT_Msk) {
      best_pdo_code = HUSB238A_SELECT_PDO_28V;
      best_protocol = PD;
      best_voltage_mv = 28000;
      best_current_ma = HUSB238A_SrcPdoCurrentToMa(reg_val);
      LOG_INFO("HUSB238A: Best PDO = 28V FPDO, %dmA", best_current_ma);
      goto scan_done;
    }
  }

  // 优先级2: 20V FPDO
  if (HUSB238A_ReadReg(HUSB238A_SRC_PDO_20V, &reg_val) == SUCCESS) {
    if (reg_val & HUSB238A_SRC_PDO_DETECT_Msk) {
      best_pdo_code = HUSB238A_SELECT_PDO_20V;
      best_protocol = PD;
      best_voltage_mv = 20000;
      best_current_ma = HUSB238A_SrcPdoCurrentToMa(reg_val);
      LOG_INFO("HUSB238A: Best PDO = 20V FPDO, %dmA", best_current_ma);
      goto scan_done;
    }
  }

  // 优先级3: PPS PDO (16V <= 最大电压 <= 28V)
  {
    uint8_t pps_voltage = 0;
    if (HUSB238A_ReadReg(HUSB238A_SRC_PPS_VOLTAGE, &pps_voltage) == SUCCESS) {
      static const struct {
        uint8_t pps_reg;
        uint8_t pdo_code;
        uint8_t max_volt_mask;
        uint8_t max_volt_shift;
      } pps_table[] = {
          {HUSB238A_SRC_PDO_PPS1, HUSB238A_SELECT_PDO_PPS1,
           HUSB238A_PPS1_MAX_VOLTAGE_Msk, HUSB238A_PPS1_MAX_VOLTAGE_Pos},
          {HUSB238A_SRC_PDO_PPS2, HUSB238A_SELECT_PDO_PPS2,
           HUSB238A_PPS2_MAX_VOLTAGE_Msk, HUSB238A_PPS2_MAX_VOLTAGE_Pos},
          {HUSB238A_SRC_PDO_PPS3, HUSB238A_SELECT_PDO_PPS3,
           HUSB238A_PPS3_MAX_VOLTAGE_Msk, HUSB238A_PPS3_MAX_VOLTAGE_Pos},
      };

      for (size_t i = 0; i < sizeof(pps_table) / sizeof(pps_table[0]); i++) {
        if (HUSB238A_ReadReg(pps_table[i].pps_reg, &reg_val) != SUCCESS) {
          continue;
        }
        if (!(reg_val & HUSB238A_SRC_PDO_DETECT_Msk)) {
          continue;
        }
        const uint8_t max_volt = (pps_voltage & pps_table[i].max_volt_mask) >> pps_table[i].max_volt_shift;
        // 16V <= PPS最大电压 <= 28V: 16V编码=0x02, 21V编码=0x03
        if (max_volt >= HUSB238A_PPS_MAX_VOLT_16V && max_volt <= HUSB238A_PPS_MAX_VOLT_21V) {
          best_pdo_code = pps_table[i].pdo_code;
          best_protocol = PPS;
          best_voltage_mv = 0; // PPS电压由Source决定，请求时会协商
          best_current_ma = HUSB238A_SrcPdoCurrentToMa(reg_val);
          LOG_INFO("HUSB238A: Best PDO = PPS%d (max_volt=%d), %dmA",
                  (int)i + 1, max_volt, best_current_ma);
          goto scan_done;
        }
      }
    }
  }

  // (已检查完所有候选PDO)

scan_done:
  if (best_pdo_code == 0) {
    LOG_WARN("HUSB238A: No suitable PDO (>=19V) found in SourceCap");
  }

  LOG_INFO("HUSB238A Contract: proto=%d, %umV, %.2fmA",
          contract_chg_protocol, contract_chg_voltage_mv, contract_chg_current_ma);
}

/* ===================== 公共接口函数 ===================== */

HUSB238A_ChargerProtocol HUSB238A_GetContractChgProtocol(void) {
  return contract_chg_protocol;
}

uint16_t HUSB238A_GetContractChgVoltage(void) {
  return contract_chg_voltage_mv;
}

float HUSB238A_GetContractChgCurrent(void) {
  return contract_chg_current_ma;
}

ErrorStatus HUSB238A_Init(void) {
  if (I2C2_Lock() != SUCCESS) {
    return ERROR;
  }

  uint8_t reg_val = 0;

  // Step 0: 检查芯片是否存在，读取STATUS寄存器
  if (HUSB238A_ReadReg(HUSB238A_STATUS, &reg_val) != SUCCESS) {
    LOG_ERR("HUSB238A: I2C communication failed");
    I2C2_Unlock();
    return ERROR;
  }

  // Step 1: 先清除所有中断标志 (写1清除)
  HUSB238A_ClearAllInterrupts();

  // Step 2: 解除全局中断掩码 (CONTROL[0] = 0, 让MASK/MASK1/MASK2控制)
  if (HUSB238A_ReadReg(HUSB238A_CONTROL, &reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }
  reg_val &= ~HUSB238A_CONTROL_INT_MASK_Msk; // INT_MASK = 0
  if (HUSB238A_WriteReg(HUSB238A_CONTROL, reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 3: 配置MASK寄存器 - 只使能关键中断
  // MASK (0x05): 使能 ATTACH, DETACH, FAULT, VBUS_OV
  // 掩码: FLGIN, ORIENT, VBUS_CHG, BC_LVL (这些我们不需要关注)
  const uint8_t mask = (1 << HUSB238A_MASK_M_FLGIN_Pos) |
                 (1 << HUSB238A_MASK_M_ORIENT_Pos) |
                 (1 << HUSB238A_MASK_M_VBUS_CHG_Pos) |
                 (1 << HUSB238A_MASK_M_BC_LVL_Pos);
  if (HUSB238A_WriteReg(HUSB238A_MASK, mask) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // MASK1 (0x06): 使能 TSD, VBUS_UV, FRC_FAIL
  // 掩码: DR_ROLE, SRC_ALERT, FRC_SUCC, VDM_MSG
  const uint8_t mask1 = (1 << HUSB238A_MASK1_M_DR_ROLE_Pos) |
                  (1 << HUSB238A_MASK1_M_SRC_ALERT_Pos) |
                  (1 << HUSB238A_MASK1_M_FRC_SUCC_Pos) |
                  (1 << HUSB238A_MASK1_M_VDM_MSG_Pos);
  if (HUSB238A_WriteReg(HUSB238A_MASK1, mask1) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // MASK2 (0x07): 使能 PD_HV, Go_Fail
  // 掩码: Exit_EPR, EPR_MODE
  const uint8_t mask2 = (1 << HUSB238A_MASK2_M_Exit_EPR_Pos) |
                  (1 << HUSB238A_MASK2_M_EPR_MODE_Pos);
  if (HUSB238A_WriteReg(HUSB238A_MASK2, mask2) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 4: 设置PD_PRIORITY=1 (USER_CFG2[2])，PD高优先级
  if (HUSB238A_ReadReg(HUSB238A_USER_CFG2, &reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }
  reg_val |= HUSB238A_CFG2_PD_PRIOR_Msk;
  if (HUSB238A_WriteReg(HUSB238A_USER_CFG2, reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 5: 将USER_CFG1的OUT2_SEL配置为故障指示模式 (00b)
  if (HUSB238A_ReadReg(HUSB238A_USER_CFG1, &reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }
  reg_val &= ~HUSB238A_CFG1_OUT2_SEL_Msk; // OUT2_SEL = 00b (Fault Indication)
  // 不再需要EN_HVDCP，已禁用传统协议
  reg_val &= ~HUSB238A_CFG1_EN_HVDCP_Msk;
  if (HUSB238A_WriteReg(HUSB238A_USER_CFG1, reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 6: 使能HUSB238A + 禁用传统协议检测
  // CONTROL1: ENABLE=1, EN_DPM_HIZ=1 (断开D+/D-，不做传统协议检测)
  if (HUSB238A_ReadReg(HUSB238A_CONTROL1, &reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }
  reg_val |= HUSB238A_CONTROL1_ENABLE_Msk;
  reg_val |= HUSB238A_CONTROL1_EN_DPM_HIZ_Msk; // 禁用传统协议
  if (HUSB238A_WriteReg(HUSB238A_CONTROL1, reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 7: 再次清除所有中断
  HUSB238A_ClearAllInterrupts();

  // 检查当前是否已有连接
  if (HUSB238A_ReadReg(HUSB238A_STATUS, &reg_val) == SUCCESS) {
    if (reg_val & HUSB238A_STATUS_ATTACH_Msk) {
      charger_detected = true;
      HUSB238A_UpdateContractInfo();
    }
  }

  LOG_INFO("HUSB238A: Init OK (addr=0x%02X)", device_addr);
  I2C2_Unlock();
  return SUCCESS;
}

bool HUSB238A_IsAttached(void) {
  if (I2C2_Lock() != SUCCESS) {
    return false;
  }
  uint8_t status = 0;
  const ErrorStatus ret = HUSB238A_ReadReg(HUSB238A_STATUS, &status);
  I2C2_Unlock();
  if (ret != SUCCESS) {
    return false;
  }
  return (status & HUSB238A_STATUS_ATTACH_Msk) != 0;
}

ErrorStatus HUSB238A_IsSupportCharge(bool *support) {
  if (support == NULL) {
    return ERROR;
  }

  // 直接根据UpdateContractInfo扫描结果判断
  *support = (best_pdo_code != 0);

  if (!(*support)) {
    LOG_WARN("HUSB238A: No suitable PDO (>=19V) found");
  }

  return SUCCESS;
}

ErrorStatus HUSB238A_RequestCharge(void) {
  if (I2C2_Lock() != SUCCESS) {
    return ERROR;
  }

  if (!HUSB238A_IsAttachedRaw()) {
    LOG_ERR("HUSB238A: No charger attached");
    I2C2_Unlock();
    return ERROR;
  }

  // 使用UpdateContractInfo中扫描到的最佳PDO
  if (best_pdo_code == 0) {
    LOG_ERR("HUSB238A: No suitable PDO, call IsSupportCharge first");
    I2C2_Unlock();
    return ERROR;
  }

  uint8_t reg_val = 0;

  LOG_INFO("HUSB238A: Requesting %s, %dmA (PDO code=0x%02X)",
          best_protocol == PPS ? "PPS" : "FPDO",
          best_current_ma, best_pdo_code);

  // Step 1: 写入SRC_PDO寄存器选择目标PDO
  // SRC_PDO (0x19): [7:3]=PDO_SELECT, [2:0] other
  if (HUSB238A_ReadReg(HUSB238A_SRC_PDO, &reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }
  reg_val = (reg_val & ~HUSB238A_SRC_PDO_SELECT_Msk) |
            (best_pdo_code << HUSB238A_SRC_PDO_SELECT_Pos);
  if (HUSB238A_WriteReg(HUSB238A_SRC_PDO, reg_val) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 2: 发送GO_COMMAND选择PDO并请求
  if (HUSB238A_WriteReg(HUSB238A_GO_COMMAND, HUSB238A_GO_SELECT_PDO) != SUCCESS) {
    I2C2_Unlock();
    return ERROR;
  }

  // Step 3: 等待GO命令执行完成
  bool go_success = false;
  for (uint32_t retry = 0; retry < (HUSB238A_GO_COMMAND_TIMEOUT_MS / 10); retry++) {
    osDelay(10);
    if (HUSB238A_ReadReg(HUSB238A_STATUS1, &reg_val) == SUCCESS) {
      if (reg_val & HUSB238A_STATUS1_AMS_SUCC_Msk) {
        go_success = true;
        break;
      }
    }
  }

  if (!go_success) {
    LOG_ERR("HUSB238A: GO_COMMAND timeout");
    uint8_t int_reg = 0;
    if (HUSB238A_ReadReg(HUSB238A_INTERRUPT, &int_reg) == SUCCESS) {
      if (int_reg & (1 << HUSB238A_INT_I_Go_Fail_Pos)) {
        LOG_ERR("HUSB238A: Go_Fail interrupt detected");
      }
    }
    I2C2_Unlock();
    return ERROR;
  }

  // Step 4: 等待电压稳定
  osDelay(HUSB238A_SETTLE_TIME_MS);

  // Step 5: 更新合约信息
  HUSB238A_UpdateContractInfo();

  LOG_INFO("HUSB238A: Charge request OK - proto=%d, %umV, %.2fmA",
          contract_chg_protocol, contract_chg_voltage_mv, contract_chg_current_ma);
  I2C2_Unlock();
  return SUCCESS;
}

ErrorStatus HUSB238A_GetCurrentVoltage(uint16_t *voltage_v) {
  if (voltage_v == NULL) {
    return ERROR;
  }
  if (I2C2_Lock() != SUCCESS) {
    return ERROR;
  }

  uint8_t raw = 0;
  const ErrorStatus ret = HUSB238A_ReadReg(HUSB238A_VBUS_MEASUREMENT, &raw);
  I2C2_Unlock();
  if (ret != SUCCESS) {
    return ERROR;
  }

  // VBUS_MEASUREMENT: 125mV per LSB
  *voltage_v = (uint16_t)raw * HUSB238A_VBUS_MEAS_LSB_MV;
  return SUCCESS;
}

bool HUSB238A_IsFault(void) {
  return is_fault;
}

/**
 * @brief EXTI中断回调 (在ISR上下文中调用)
 * @note  ISR优先级高于所有FreeRTOS任务，无需mutex保护。
 *        ReadReg/WriteReg使用无锁I2C函数。
 *        注意: ISR中调用了osDelay/UpdateContractInfo, 需确保
 *        EXTI中断优先级已设置为可调用FreeRTOS API的阈值
 *        (低于configMAX_SYSCALL_INTERRUPT_PRIORITY)。
 */
void HUSB238A_EXTI_Callback(void) {
  if (I2C2_Lock() != SUCCESS) {
    return;
  }

  // Step 1: 读中断标志（先不清除）
  uint8_t int_reg = 0, int1_reg = 0, int2_reg = 0;
  if (HUSB238A_ReadReg(HUSB238A_INTERRUPT, &int_reg) != SUCCESS) {
    I2C2_Unlock();
    return;
  }
  if (HUSB238A_ReadReg(HUSB238A_INTERRUPT1, &int1_reg) != SUCCESS) {
    I2C2_Unlock();
    return;
  }
  if (HUSB238A_ReadReg(HUSB238A_INTERRUPT2, &int2_reg) != SUCCESS) {
    I2C2_Unlock();
    return;
  }

  LOG_INFO("HUSB238A IRQ: INT=0x%02X, INT1=0x%02X, INT2=0x%02X",
          int_reg, int1_reg, int2_reg);

  // Step 2: 解析事件类型
  const bool is_attach = int1_reg & (1 << HUSB238A_INT1_I_ATTACH_Pos);
  const bool is_detach = int1_reg & (1 << HUSB238A_INT1_I_DETACH_Pos);
  const bool is_pd_hv  = int_reg  & (1 << HUSB238A_INT_I_PD_HV_Pos);
  const bool is_fault_  = int1_reg & (1 << HUSB238A_INT1_I_FAULT_Pos);
  const bool is_vbus_ov = int1_reg & (1 << HUSB238A_INT1_I_VBUS_OV_Pos);
  const bool is_vbus_uv = int2_reg & (1 << HUSB238A_INT2_I_VBUS_UV_Pos);
  const bool is_tsd     = int2_reg & (1 << HUSB238A_INT2_I_TSD_Pos);

  // Step 3: 读连接状态 (ISR上下文，使用无锁版本)
  const bool attached = HUSB238A_IsAttachedRaw();

  // Step 4: 根据事件类型处理
  if (is_detach) {
    // 充电器拔出
    charger_detected = false;
    contract_chg_protocol = UNKNOW;
    contract_chg_voltage_mv = 0;
    contract_chg_current_ma = 0.0f;
    best_pdo_code = 0;
    best_protocol = UNKNOW;
    best_voltage_mv = 0;
    best_current_ma = 0;
    LOG_INFO("HUSB238A: Charger detached");
  }

  if (is_fault_ || is_vbus_ov || is_vbus_uv || is_tsd) {
    // bool fault = false;
    // HUSB238A_IsFaultRaw(&fault);
    is_fault = true;
    return;
  }
  is_fault = false;

  if (is_attach || is_pd_hv) {
    // 充电器插入或电压协商完成
    if (HUSB238A_RequestCharge() != SUCCESS) {
      charger_detected = false;
      return;
    }
    charger_detected = true;
    HUSB238A_UpdateContractInfo();
    LOG_INFO("HUSB238A: Charger %s, contract=%uV/%.2fA",
            is_attach ? "attached" : "HV negotiated",
            contract_chg_voltage_mv, contract_chg_current_ma);
  }

  // 如果连接但没有事件触发，也更新合约信息
  if (attached && !is_attach && !is_detach && !is_pd_hv) {
    charger_detected = true;
    HUSB238A_UpdateContractInfo();
  }

  // Step 5: 清除所有已触发的中断标志 (写1清除)
  if (int_reg) HUSB238A_WriteReg(HUSB238A_INTERRUPT, int_reg);
  if (int1_reg) HUSB238A_WriteReg(HUSB238A_INTERRUPT1, int1_reg);
  if (int2_reg) HUSB238A_WriteReg(HUSB238A_INTERRUPT2, int2_reg);

  I2C2_Unlock();
}