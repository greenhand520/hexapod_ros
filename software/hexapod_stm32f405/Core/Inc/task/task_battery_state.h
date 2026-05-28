//
// Created by greenhand520 on 2026/5/14.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"

#include "bsp/bq40z50.h"

#define BQ40Z50_READ_INTERVAL_MS 256

/** BQ40Z50 共享数据结构，由 BQ40Z50Task 写入，BoardSensors 任务读取 */
typedef struct {
  uint16_t serial;
  /** 综合的一个电池温度 (°C) */
  float temp_c;           
  /** 电池总电压 (mV) */
  uint16_t voltage_mv;    
  /** 瞬时电流 (mA, 负=充电) */
  int16_t current_ma;     
  /** 相对电量 (%) */
  uint8_t soc;            
  /** 剩余容量 (mAh) */
  uint16_t remaining_mah; 
  /** 满充容量 (mAh) */
  uint16_t full_cap_mah;  
  /** 循环次数 */
  uint16_t cycle_count;   
  /** 单体1 (mV) */
  uint16_t cell1_mv;      
  /** 单体2 (mV) */
  uint16_t cell2_mv;      
  /** 单体3 (mV) */
  uint16_t cell3_mv;      
  /** 单体4 (mV) */
  uint16_t cell4_mv;      
  /** 温度 */
  BQ40Z50_TempDetail temp_detail;
  /** 是否存在 */
  uint8_t present;
} BQ40Z50_SharedData;

extern osThreadId_t taskBatteryStateHandle;

/**
 * @brief BQ40Z50 电池数据采集任务，并发布/sensor/battery_state话题
 * @param argument
 */
void Task_BatteryState(void *argument);

void Task_CreateBatteryStateTask(void);




#ifdef __cplusplus
}
#endif
