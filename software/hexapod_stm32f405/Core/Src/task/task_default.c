//
// Created by eartholnpc on 2026/5/28.
//
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "semphr.h"

#include <stddef.h>

#include <hexapod_sensor_interface/msg/board_state.h>
#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/init.h>
#include <rclc/node.h>
#include <rclc/publisher.h>
#include <rclc/rclc.h>
#include <rmw_microros/custom_transport.h>
#include <rmw_microros/rmw_microros.h>
#include <sensor_msgs/msg/battery_state.h>
#include <sensor_msgs/msg/imu.h>

#include "bsp/adc.h"
#include "bsp/bq24725.h"
#include "bsp/bq40z50.h"
#include "bsp/buzz.h"
#include "bsp/i2c.h"
#include "main.h"
#include "task/task_board_state.h"
#include "task/task_default.h"

SemaphoreHandle_t g_i2c1_mutex = NULL;
SemaphoreHandle_t g_i2c2_mutex = NULL;
extern UART_HandleTypeDef huart5;

/* ================================================================
 *  micro-ROS 对象
 * ================================================================ */
static rclc_support_t s_support;
static rcl_node_t s_node;
static rcl_allocator_t s_allocator;

rcl_publisher_t board_state_pub;
rcl_publisher_t battery_state_pub;
/* 全局，供 IMU 任务使用 */
rcl_publisher_t imu_pub;

/* 消息实例（静态分配，避免堆碎片化） */
hexapod_sensor_interface__msg__BoardState s_msg_board;
sensor_msgs__msg__BatteryState s_msg_batt;
sensor_msgs__msg__Imu s_msg_imu;

/* micro-ROS 已初始化标志 */
static volatile uint8_t s_uros_initialized = 0;

/* micro-ROS 连接状态标志（0 = 未连接, 1 = 已连接 由 app.c 设置，WS2812 读取）
 */
volatile uint8_t g_uros_connected = 0;

/* ================================================================
 *  micro-ROS 传输回调（定义在 dma_transport.c 中）
 * ================================================================ */
extern bool cubemx_transport_open(struct uxrCustomTransport *transport);
extern bool cubemx_transport_close(struct uxrCustomTransport *transport);
extern size_t cubemx_transport_write(struct uxrCustomTransport *transport,
                                     const uint8_t *buf, size_t len,
                                     uint8_t *err);
extern size_t cubemx_transport_read(struct uxrCustomTransport *transport,
                                    uint8_t *buf, size_t len, int timeout,
                                    uint8_t *err);

static int microros_init(void) {
  /* ---- 1. 设置自定义传输（UART5 DMA） ---- */
  rmw_uros_set_custom_transport(
      MICROROS_TRANSPORTS_FRAMING_MODE, (void *)&huart5, cubemx_transport_open,
      cubemx_transport_close, cubemx_transport_write, cubemx_transport_read);

  /* ---- 2. rclc_support 初始化 ---- */
  s_allocator = rcl_get_default_allocator();
  rcl_ret_t rc = rclc_support_init(&s_support, 0, NULL, &s_allocator);
  if (rc != RCL_RET_OK) {
    return -1;
  }

  /* ---- 3. 创建 node ---- */
  rc = rclc_node_init_default(&s_node, "hexapod_stm32", "", &s_support);
  if (rc != RCL_RET_OK) {
    return -2;
  }

  /* ---- 4. 创建 publisher: /sensor/board_state ---- */
  rc = rclc_publisher_init_default(
      &board_state_pub, &s_node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(hexapod_sensor_interface, msg, BoardState),
      "/sensor/board_state");
  if (rc != RCL_RET_OK) {
    return -3;
  }

  /* ---- 5. 创建 publisher: /sensor/battery_state ---- */
  rc = rclc_publisher_init_default(
      &battery_state_pub, &s_node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, BatteryState),
      "/sensor/battery_state");
  if (rc != RCL_RET_OK) {
    return -4;
  }

  /* ---- 6. 创建 publisher: /sensor/imu ---- */
  rc = rclc_publisher_init_default(
      &imu_pub, &s_node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
      "/sensor/imu");
  if (rc != RCL_RET_OK) {
    return -5;
  }

  return 0;
}

void Task_default(void *argument) {

  g_i2c1_mutex = xSemaphoreCreateMutex();
  configASSERT(g_i2c1_mutex);
  g_i2c2_mutex = xSemaphoreCreateMutex();
  configASSERT(g_i2c2_mutex);

  static BQ24725Config bq24725_cfg = {0};
  bq24725_cfg.hi2c = &hi2c2;
  bq24725_cfg.bus_mutex = g_i2c2_mutex;
  bq24725_cfg.ACOK_GPIO_Port = AC_OK_GPIO_Port;
  bq24725_cfg.ACOK_GPIO_Pin = AC_OK_Pin;
  bq24725_cfg.ACOK_cb = NULL;
  BQ24725_Init(&bq24725_cfg);
  BQ24725_SetChargeCurrent(CHARGE_CURRENT_MIN_MA);
  BQ24725_SetChargeVoltage(CHARGE_VOLTAGE_DEFAULT);

  static BQ40Z50_Config bq40z50_cfg = {0};
  bq40z50_cfg.hi2c = &hi2c2;
  bq40z50_cfg.bus_mutex = g_i2c2_mutex;
  BQ40Z50_Init(&bq40z50_cfg);

  const int uros_ret = microros_init();
  if (uros_ret != 0) {
    for (;;) {
      osDelay(1000);
    }
  }
  s_uros_initialized = 1;
  g_uros_connected = 1;

  // 开机音效 + 打开舵机供电
  BUZZ_StartupSound();
  HAL_GPIO_WritePin(VM_EN_GPIO_Port, VM_EN_Pin, GPIO_PIN_SET);

  // 初始化消息实例
  memset(&s_msg_board, 0, sizeof(s_msg_board));
  memset(&s_msg_batt, 0, sizeof(s_msg_batt));
  memset(&s_msg_imu, 0, sizeof(s_msg_imu));
  s_msg_batt.present = true;
  s_msg_batt.power_supply_technology =
      sensor_msgs__msg__BatteryState__POWER_SUPPLY_TECHNOLOGY_LIPO;
  s_msg_imu.orientation_covariance[0] = -1.0;

  // 启动 ADC DMA
  ADC_Start();

  // 空闲循环
  for (;;) {
    osDelay(1000);
  }
}
void Task_CreateDefaultTask(void) {
  const osThreadAttr_t task_attr = {
      .name = "DefaultTask",
      .stack_size = 1024 * 5, // 5KB - micro-ROS 初始化需要较大栈空间
      .priority = (osPriority_t)osPriorityNormal,
  };
  taskDefaultHandle = osThreadNew(Task_default, NULL, &task_attr);
}
