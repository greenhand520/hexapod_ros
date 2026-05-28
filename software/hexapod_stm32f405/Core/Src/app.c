//
// Created by greenhand520 on 2026/5/12.
//

#include "app.h"
#include "bsp/bq24725.h"
#include "bsp/husb238a.h"
#include "bsp/mpu6500.h"
#include "log.h"
#include "main.h"
#include "task/task_battery_state.h"
#include "task/task_board_state.h"
#include "task/task_default.h"
#include "task/task_imu.h"
#include "task/task_led_indication.h"
#include "task/task_microros.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == MPU_INT_Pin) {
    MPU6500_EXTI_Callback();
    return;
  }
  if (GPIO_Pin == AC_OK_Pin) {
    BQ24725_ACOK_EXTI_Callback();
    return;
  }
  if (GPIO_Pin == HUSB238A_INT_Pin) {
    HUSB238A_EXTI_Callback();
  }
}

void app_start(void) {
  // 创建日志队列（需在 osKernelInitialize 之后）
  LOG_Init();

  // Task_CreateDefaultTask();
  Task_CreateIMUTask();
  Task_CreateBoardState();
  Task_CreateBatteryStateTask();
  Task_CreateLedIndictionTask();
}