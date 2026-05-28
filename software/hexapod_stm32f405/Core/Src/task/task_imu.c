//
// Created by greenhand520 on 2026/5/14.
//

#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"

#include "rcl/publisher.h"
#include "sensor_msgs/msg/imu.h"

#include "bsp/mpu6500.h"
#include "log.h"
#include "main.h"
#include "task/task_imu.h"

static sensor_msgs__msg__Imu s_msg_imu __attribute__((section(".ccmram")));
static MPU6500_Data_t imu_data __attribute__((section(".ccmram")));

extern SPI_HandleTypeDef hspi1;
extern volatile uint8_t g_uros_connected;
extern rcl_publisher_t imu_pub;

void Task_IMU(void *argument) {
  (void)argument;

  static const MPU6500_Config_t imu_cfg = {
      .hspi = &hspi1,
      .CS_GPIO_Port = CS1_MPU_GPIO_Port,
      .CS_GPIO_Pin = CS1_MPU_Pin,
      .accel_range = MPU6500_ACCEL_RANGE_4G,
      .gyro_range = MPU6500_GYRO_RANGE_2000DPS,
      .dlpf = MPU6500_DLPF_41HZ,
      // 1kHz / 5 = 200Hz
      .sample_rate_divider = 4,
  };

  /* 初始化 MPU6500 */
  const int ret = MPU6500_Init(&imu_cfg);
  if (ret != MPU6500_OK) {
    LOG_ERR("Failed to init MPU6500");
    for (;;) {
      osDelay(osWaitForever);
      // TODO: 错误状态，第一个指示灯显示红灯
    }
  }

  MPU6500_ReadAllDMA();

  for (;;) {
    osThreadFlagsWait(0x02, osFlagsWaitAny, osWaitForever);

    // DMA 传输完成后处理数据
    MPU6500_ProcessData();

    // 填充 IMU 消息（ROS2 标准单位: m/s² 和 rad/s）
    s_msg_imu.header.stamp.sec = 0;
    s_msg_imu.header.stamp.nanosec = 0;

    // 加速度: g → m/s²  (×9.80665)
    // MPU6500_Data_t imu_data;
    MPU6500_GetData(&imu_data);
    s_msg_imu.linear_acceleration.x = (double)(imu_data.accel_x_g * 9.80665f);
    s_msg_imu.linear_acceleration.y = (double)(imu_data.accel_y_g * 9.80665f);
    s_msg_imu.linear_acceleration.z = (double)(imu_data.accel_z_g * 9.80665f);

    // 角速度: °/s → rad/s  (×π/180)
    s_msg_imu.angular_velocity.x =
        (double)(imu_data.gyro_x_dps * 0.0174532925f);
    s_msg_imu.angular_velocity.y =
        (double)(imu_data.gyro_y_dps * 0.0174532925f);
    s_msg_imu.angular_velocity.z =
        (double)(imu_data.gyro_z_dps * 0.0174532925f);

    if (g_uros_connected) {
      const rcl_ret_t rcl_ret = rcl_publish(&imu_pub, &s_msg_imu, NULL);
      if (rcl_ret != RCL_RET_OK) {
        LOG_ERR("Failed to publish imu data with error %d", rcl_ret);
      }
    }

    // 触发下一次 DMA 读取
    MPU6500_ReadAllDMA();
  }
}

void Task_CreateIMUTask(void) {
  const osThreadAttr_t task_attr = {
      .name = "IMUTask",
      .stack_size = 1024 * 3, // 3KB - SPI DMA + MPU6500数据处理 + IMU消息填充 + publish
      .priority = (osPriority_t)osPriorityNormal5,
  };
  taskIMUHandle = osThreadNew(Task_IMU, NULL, &task_attr);
}
