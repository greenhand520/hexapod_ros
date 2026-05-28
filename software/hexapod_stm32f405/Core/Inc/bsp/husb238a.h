//
// Created by eartholnpc on 2026/5/26.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* ============================================
 * I2C地址
 * ============================================ */

/** ADDR接VDD(900k上拉) */
#define HUSB238A_ADDR_VDD (0x62 << 1)
/** ADDR接GND(900k下拉) */
#define HUSB238A_ADDR_GND (0x42 << 1)

/* ============================================
 * 寄存器地址定义 (基于 HUSB238A Register Information Rev0.1)
 * ============================================ */

/* 用户配置寄存器 */
#define HUSB238A_CONTROL       0x01
#define HUSB238A_CONTROL1      0x02
#define HUSB238A_MANUAL        0x03
#define HUSB238A_RESET         0x04
#define HUSB238A_MASK          0x05
#define HUSB238A_MASK1         0x06
#define HUSB238A_MASK2         0x07

/* 中断寄存器 (写1清除) */
#define HUSB238A_INTERRUPT     0x09
#define HUSB238A_INTERRUPT1    0x0A
#define HUSB238A_INTERRUPT2    0x0B

/* 用户配置寄存器 */
#define HUSB238A_USER_CFG0     0x0C
#define HUSB238A_USER_CFG1     0x0D
#define HUSB238A_USER_CFG2     0x0E
#define HUSB238A_USER_CFG3     0x0F

/* PDO选择和GO命令 */
#define HUSB238A_GO_COMMAND    0x18
#define HUSB238A_SRC_PDO       0x19

/* 状态寄存器 (只读) */
#define HUSB238A_STATUS        0x63
#define HUSB238A_STATUS1       0x64
#define HUSB238A_TYPE          0x65
#define HUSB238A_DPDM_STATUS   0x66
#define HUSB238A_CONTRACT_STATUS0 0x67
#define HUSB238A_CONTRACT_STATUS1 0x68

/* SourceCap_INFO */
#define HUSB238A_SOURCECAP_INFO 0x69

/* SRC PDO检测寄存器 (只读) */
#define HUSB238A_SRC_PDO_5V    0x6A
#define HUSB238A_SRC_PDO_9V    0x6B
#define HUSB238A_SRC_PDO_12V   0x6C
#define HUSB238A_SRC_PDO_15V   0x6D
#define HUSB238A_SRC_PDO_20V   0x6E
#define HUSB238A_SRC_PDO_28V   0x6F
#define HUSB238A_SRC_PDO_36V   0x70
#define HUSB238A_SRC_PDO_48V   0x71

/* SRC PPS检测寄存器 (格式同SRC_PDO: [7]=DETECT, [6:0]=CURRENT) */
#define HUSB238A_SRC_PDO_PPS1  0x72
#define HUSB238A_SRC_PDO_PPS2  0x73
#define HUSB238A_SRC_PDO_PPS3  0x74
#define HUSB238A_SRC_PPS_VOLTAGE 0x75
#define HUSB238A_SRC_PDO_AVS   0x76

/* SRC_PPS_VOLTAGE (0x75) 位定义 */
/* [7:6] PPS1_MAX_VOLTAGE */
#define HUSB238A_PPS1_MAX_VOLTAGE_Pos 6
#define HUSB238A_PPS1_MAX_VOLTAGE_Msk (0x03 << HUSB238A_PPS1_MAX_VOLTAGE_Pos)
/* [5:4] PPS2_MAX_VOLTAGE */
#define HUSB238A_PPS2_MAX_VOLTAGE_Pos 4
#define HUSB238A_PPS2_MAX_VOLTAGE_Msk (0x03 << HUSB238A_PPS2_MAX_VOLTAGE_Pos)
/* [3:2] PPS3_MAX_VOLTAGE */
#define HUSB238A_PPS3_MAX_VOLTAGE_Pos 2
#define HUSB238A_PPS3_MAX_VOLTAGE_Msk (0x03 << HUSB238A_PPS3_MAX_VOLTAGE_Pos)
/* PPS最大电压编码 */
#define HUSB238A_PPS_MAX_VOLT_5V9  0x00  /* 0V - 7V */
#define HUSB238A_PPS_MAX_VOLT_11V  0x01  /* 7.02V - 12V */
#define HUSB238A_PPS_MAX_VOLT_16V  0x02  /* 12.02V - 17V */
#define HUSB238A_PPS_MAX_VOLT_21V  0x03  /* >17.02V */

/* VBUS测量 */
#define HUSB238A_VBUS_MEASUREMENT 0x87

/* ============================================
 * CONTROL (0x01) 位定义
 * ============================================ */
#define HUSB238A_CONTROL_INT_MASK_Pos 0
#define HUSB238A_CONTROL_INT_MASK_Msk (1 << HUSB238A_CONTROL_INT_MASK_Pos)

/* ============================================
 * CONTROL1 (0x02) 位定义
 * ============================================ */
#define HUSB238A_CONTROL1_ENABLE_Pos 3
#define HUSB238A_CONTROL1_ENABLE_Msk (1 << HUSB238A_CONTROL1_ENABLE_Pos)
/** EN_DPM_HIZ=1: 断开D+/D-与内部电路连接，不做传统协议检测 */
#define HUSB238A_CONTROL1_EN_DPM_HIZ_Pos 5
#define HUSB238A_CONTROL1_EN_DPM_HIZ_Msk (1 << HUSB238A_CONTROL1_EN_DPM_HIZ_Pos)

/* ============================================
 * MASK (0x05) 位定义
 * ============================================ */
#define HUSB238A_MASK_M_FLGIN_Pos    7
#define HUSB238A_MASK_M_ORIENT_Pos   6
#define HUSB238A_MASK_M_FAULT_Pos    5
#define HUSB238A_MASK_M_VBUS_CHG_Pos 4
#define HUSB238A_MASK_M_VBUS_OV_Pos  3
#define HUSB238A_MASK_M_BC_LVL_Pos   2
#define HUSB238A_MASK_M_DETACH_Pos   1
#define HUSB238A_MASK_M_ATTACH_Pos   0

/* ============================================
 * MASK1 (0x06) 位定义
 * ============================================ */
#define HUSB238A_MASK1_M_TSD_Pos       7
#define HUSB238A_MASK1_M_VBUS_UV_Pos   6
#define HUSB238A_MASK1_M_DR_ROLE_Pos   5
#define HUSB238A_MASK1_M_SRC_ALERT_Pos 3
#define HUSB238A_MASK1_M_FRC_FAIL_Pos  2
#define HUSB238A_MASK1_M_FRC_SUCC_Pos  1
#define HUSB238A_MASK1_M_VDM_MSG_Pos   0

/* ============================================
 * MASK2 (0x07) 位定义
 * ============================================ */
#define HUSB238A_MASK2_M_Exit_EPR_Pos  3
#define HUSB238A_MASK2_M_Go_Fail_Pos   2
#define HUSB238A_MASK2_M_EPR_MODE_Pos  1
#define HUSB238A_MASK2_M_PD_HV_Pos     0

/* ============================================
 * INTERRUPT (0x09) 位定义
 * ============================================ */
#define HUSB238A_INT_I_Exit_EPR_Pos 3
#define HUSB238A_INT_I_Go_Fail_Pos  2
#define HUSB238A_INT_I_EPR_MODE_Pos 1
#define HUSB238A_INT_I_PD_HV_Pos    0

/* ============================================
 * INTERRUPT1 (0x0A) 位定义
 * ============================================ */
#define HUSB238A_INT1_I_FLGIN_Pos    7
#define HUSB238A_INT1_I_ORIENT_Pos   6
#define HUSB238A_INT1_I_FAULT_Pos    5
#define HUSB238A_INT1_I_VBUS_CHG_Pos 4
#define HUSB238A_INT1_I_VBUS_OV_Pos  3
#define HUSB238A_INT1_I_BC_LVL_Pos   2
#define HUSB238A_INT1_I_DETACH_Pos   1
#define HUSB238A_INT1_I_ATTACH_Pos   0

/* ============================================
 * INTERRUPT2 (0x0B) 位定义
 * ============================================ */
#define HUSB238A_INT2_I_TSD_Pos       7
#define HUSB238A_INT2_I_VBUS_UV_Pos   6
#define HUSB238A_INT2_I_DR_ROLE_Pos   5
#define HUSB238A_INT2_I_SRC_ALERT_Pos 3
#define HUSB238A_INT2_I_FRC_FAIL_Pos  2
#define HUSB238A_INT2_I_FRC_SUCC_Pos  1
#define HUSB238A_INT2_I_VDM_MSG_Pos   0

/* ============================================
 * USER_CFG2 (0x0E) 位定义
 * ============================================ */
#define HUSB238A_CFG2_PD_PRIOR_Pos 2
#define HUSB238A_CFG2_PD_PRIOR_Msk (1 << HUSB238A_CFG2_PD_PRIOR_Pos)

/* ============================================
 * USER_CFG1 (0x0D) 位定义
 * ============================================ */
#define HUSB238A_CFG1_EN_HVDCP_Pos 6
#define HUSB238A_CFG1_EN_HVDCP_Msk (1 << HUSB238A_CFG1_EN_HVDCP_Pos)
#define HUSB238A_CFG1_EN_VB_UV_Pos 3
#define HUSB238A_CFG1_OUT2_SEL_Pos 0
#define HUSB238A_CFG1_OUT2_SEL_Msk (0x03 << HUSB238A_CFG1_OUT2_SEL_Pos)
/** 00b: Fault Indication */
#define HUSB238A_OUT2_FAULT_INDICATION 0x00

/* ============================================
 * GO_COMMAND (0x18) 寄存器位定义
 * ============================================ */
#define HUSB238A_GO_COMMAND_Pos 0
#define HUSB238A_GO_COMMAND_Msk (0x1F << HUSB238A_GO_COMMAND_Pos)
/** 选择PDO并请求 */
#define HUSB238A_GO_SELECT_PDO  0x01
/** 软复位 (11101b) */
#define HUSB238A_GO_SOFT_RESET  0x1D
/** 硬复位 (11110b) */
#define HUSB238A_GO_HARD_RESET  0x1E

/* ============================================
 * SRC_PDO (0x19) 寄存器位定义
 * ============================================ */
/* [7:3] PDO_SELECT: 选择的PDO电压编码 */
#define HUSB238A_SRC_PDO_SELECT_Pos 3
#define HUSB238A_SRC_PDO_SELECT_Msk (0x1F << HUSB238A_SRC_PDO_SELECT_Pos)

/* PDO选择编码值 (写入[7:3])，基于PDF Table 24 */
#define HUSB238A_SELECT_PDO_NONE 0x00
#define HUSB238A_SELECT_PDO_5V   0x01
#define HUSB238A_SELECT_PDO_9V   0x02
#define HUSB238A_SELECT_PDO_12V  0x03
#define HUSB238A_SELECT_PDO_15V  0x04
#define HUSB238A_SELECT_PDO_20V  0x05
#define HUSB238A_SELECT_PDO_PPS1 0x06
#define HUSB238A_SELECT_PDO_PPS2 0x07
#define HUSB238A_SELECT_PDO_PPS3 0x08
#define HUSB238A_SELECT_PDO_AVS  0x09
#define HUSB238A_SELECT_PDO_28V  0x18
#define HUSB238A_SELECT_PDO_36V  0x1A
#define HUSB238A_SELECT_PDO_48V  0x1C
#define HUSB238A_SELECT_EPR_AVS  0x1E

/* [2:0] SNK_PPS_VOL_M / other control bits */

/* ============================================
 * STATUS (0x63) 位定义 (只读)
 * ============================================ */
#define HUSB238A_STATUS_AMS_PROCESS_Pos 7
#define HUSB238A_STATUS_PD_EPR_SNK_Pos  6
#define HUSB238A_STATUS_TSD_Pos         3
#define HUSB238A_STATUS_TSD_Msk         (1 << HUSB238A_STATUS_TSD_Pos)
#define HUSB238A_STATUS_BC_LVL_Pos      2
#define HUSB238A_STATUS_BC_LVL_Msk      (0x03 << HUSB238A_STATUS_BC_LVL_Pos)
#define HUSB238A_STATUS_ATTACH_Pos      0
#define HUSB238A_STATUS_ATTACH_Msk      (1 << HUSB238A_STATUS_ATTACH_Pos)

/* ============================================
 * STATUS1 (0x64) 位定义 (只读)
 * ============================================ */
#define HUSB238A_STATUS1_FLGIN_Pos      7
#define HUSB238A_STATUS1_PD_HV_Pos      5
#define HUSB238A_STATUS1_PD_COMM_Pos    4
#define HUSB238A_STATUS1_SRC_ALERT_Pos  3
#define HUSB238A_STATUS1_AMS_SUCC_Pos   2
#define HUSB238A_STATUS1_AMS_SUCC_Msk   (1 << HUSB238A_STATUS1_AMS_SUCC_Pos)
#define HUSB238A_STATUS1_FAULT_Pos      1
#define HUSB238A_STATUS1_FAULT_Msk      (1 << HUSB238A_STATUS1_FAULT_Pos)
#define HUSB238A_STATUS1_DATA_ROLE_Pos  0

/* ============================================
 * TYPE (0x65) 位定义 (只读)
 * ============================================ */
#define HUSB238A_TYPE_CC_RX_ACTIVE_Pos 7
#define HUSB238A_TYPE_DEBUGSNK_Pos     5
#define HUSB238A_TYPE_SINK_Pos         4
#define HUSB238A_TYPE_SINK_Msk         (1 << HUSB238A_TYPE_SINK_Pos)

/* ============================================
 * DPDM_STATUS (0x66) 位定义 (只读)
 * [7:3] DPDM_STATUS: 当前legacy协议状态
 * [2]   CDP_FLAG
 * [1]   SDP_FLAG
 * [0]   DIVIDER3_FLAG
 * ============================================ */
#define HUSB238A_DPDM_STATUS_Pos      3
#define HUSB238A_DPDM_STATUS_Msk      (0x1F << HUSB238A_DPDM_STATUS_Pos)
#define HUSB238A_DPDM_CDP_FLAG_Pos    2
#define HUSB238A_DPDM_SDP_FLAG_Pos    1
#define HUSB238A_DPDM_DIVIDER3_FLAG_Pos 0

/* DPDM_STATUS协议编码 */
#define HUSB238A_DPDM_UNATTACHED     0x00
#define HUSB238A_DPDM_DIVIDER3_DET   0x02
#define HUSB238A_DPDM_BC12_DET       0x03
#define HUSB238A_DPDM_QC2_DET        0x05
#define HUSB238A_DPDM_HIZ            0x06

/* ============================================
 * CONTRACT_STATUS0 (0x67) 位定义 (只读)
 * ============================================ */
/* [7:4] PD_CONTRACT: 当前PD合约 */
#define HUSB238A_CONTRACT_PD_Pos     4
#define HUSB238A_CONTRACT_PD_Msk     (0x0F << HUSB238A_CONTRACT_PD_Pos)
/* PD合约编码 */
#define HUSB238A_PD_CONTRACT_TYPEC_5V 0x00
#define HUSB238A_PD_CONTRACT_5V       0x01
#define HUSB238A_PD_CONTRACT_9V       0x02
#define HUSB238A_PD_CONTRACT_12V      0x03
#define HUSB238A_PD_CONTRACT_15V      0x04
#define HUSB238A_PD_CONTRACT_20V      0x05
#define HUSB238A_PD_CONTRACT_PPS1     0x06
#define HUSB238A_PD_CONTRACT_PPS2     0x07
#define HUSB238A_PD_CONTRACT_PPS3     0x08
#define HUSB238A_PD_CONTRACT_AVS      0x09
#define HUSB238A_PD_CONTRACT_28V      0x0A
#define HUSB238A_PD_CONTRACT_36V      0x0B
#define HUSB238A_PD_CONTRACT_48V      0x0C
#define HUSB238A_PD_CONTRACT_EPR_AVS  0x0D

/* [3:0] DPM_CONTRACT: 当前DPDM合约 */
#define HUSB238A_CONTRACT_DPM_Pos    0
#define HUSB238A_CONTRACT_DPM_Msk    (0x0F << HUSB238A_CONTRACT_DPM_Pos)
#define HUSB238A_DPM_CONTRACT_5V_DEFAULT   0x00
#define HUSB238A_DPM_CONTRACT_5V_DIVIDER3  0x01
#define HUSB238A_DPM_CONTRACT_5V_SDP       0x02
#define HUSB238A_DPM_CONTRACT_5V_CDP       0x03
#define HUSB238A_DPM_CONTRACT_5V_DCP       0x04
#define HUSB238A_DPM_CONTRACT_5V_HVDCP     0x05
#define HUSB238A_DPM_CONTRACT_QC2_9V       0x06
#define HUSB238A_DPM_CONTRACT_QC2_12V      0x07

/* ============================================
 * CONTRACT_STATUS1 (0x68) 位定义 (只读)
 * ============================================ */
/* [7:0] CONTRACT_CURRENT: 协商后的电流
 * 0x00-0x7D: 20mA/LSB, offset 0.5A
 * 0x7E-0xFF: 40mA/LSB, offset 0.5A
 */

/* ============================================
 * SRC_PDO_XXV 寄存器位定义 (0x6A-0x71)
 * ============================================ */
/* 每个SRC_PDO寄存器格式：
 * [7]    SRC_XXV_DETECT: 0=未检测到 1=检测到该电压PDO
 * [6:0]  SRC_XXV_CURRENT: 电流能力，100mA/LSB
 */
#define HUSB238A_SRC_PDO_DETECT_Pos 7
#define HUSB238A_SRC_PDO_DETECT_Msk (1 << HUSB238A_SRC_PDO_DETECT_Pos)
#define HUSB238A_SRC_PDO_CURRENT_Pos 0
#define HUSB238A_SRC_PDO_CURRENT_Msk (0x7F << HUSB238A_SRC_PDO_CURRENT_Pos)

/* ============================================
 * VBUS_MEASUREMENT (0x87)
 * ============================================ */
/** 125mV per LSB */
#define HUSB238A_VBUS_MEAS_LSB_MV 125

/* ============================================
 * GO_COMMAND超时和延时
 * ============================================ */
#define HUSB238A_GO_COMMAND_TIMEOUT_MS  500
#define HUSB238A_SETTLE_TIME_MS         50

typedef enum {
  UNKNOW = 0,
  PD = 1,
  PPS = 2,
  DCP = 3,
  HVDCP = 4,
  BC = 5
} HUSB238A_ChargerProtocol;

/**
 * @brief 初始化husb238a
 *        - 清除中断标志（写1清除）
 *        - 解除全局中断掩码
 *        - 配置MASK/MASK1/MASK2只使能关键中断
 *        - PD_PRIORITY=1
 *        - USER_CFG1 OUT2_SEL=故障指示模式
 *        - CONTROL1 EN_DPM_HIZ=1 禁用传统协议
 * @return SUCCESS on success, ERROR on failure
 */
ErrorStatus HUSB238A_Init(void);

/** @brief 获取协商的充电协议 */
HUSB238A_ChargerProtocol HUSB238A_GetContractChgProtocol(void);
/** @brief 获取协商的充电电压 */
uint16_t HUSB238A_GetContractChgVoltage(void);
/** @brief 获取协商的充电电流 */
float HUSB238A_GetContractChgCurrent(void);

/** @brief Check if charger is attached */
bool HUSB238A_IsAttached(void);

/**
 * @brief 是否支持充电 (基于UpdateContractInfo的扫描结果)
 *        必须在EXTI回调触发(UpdateContractInfo自动调用)后或Init后调用。
 *        直接检查内部存储的best_pdo_code是否非零。
 * @param[out] support true=支持>=19V充电 false=不支持
 * @return SUCCESS on success, ERROR on failure
 */
ErrorStatus HUSB238A_IsSupportCharge(bool *support);

/**
 * @brief 请求充电 (使用UpdateContractInfo扫描到的最佳PDO)
 *        优先级: 28V FPDO > 20V FPDO > PPS(16V<=最大电压<=28V)
 *        会自动根据best_protocol选择PDO类型(FPDO或PPS)
 *        调用前需确保UpdateContractInfo已执行(Init或EXTI回调中自动完成)
 * @return SUCCESS on success, ERROR on failure
 */
ErrorStatus HUSB238A_RequestCharge(void);

/**
 * @brief 获取当前VBUS的电压，使用HUSB238A内部ADC读取VBUS电压，读取寄存器来获取
 * @return true on success
 */
// ErrorStatus HUSB238A_GetCurrentVoltage(uint16_t *voltage_v);

/**
 * @brief 是否发生错误，读取HUSB238A的寄存器来判断是否发生故障
 * @return true on success
 */
bool HUSB238A_IsFault(void);

/**
 * @brief EXTI 中断处理（在 HAL_GPIO_EXTI_Callback 中调用）
 *        INT_N 引脚拉低触发 STM32 外部中断
 */
void HUSB238A_EXTI_Callback(void);

#ifdef __cplusplus
}
#endif