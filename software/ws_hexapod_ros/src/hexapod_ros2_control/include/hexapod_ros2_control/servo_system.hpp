//
// Created by greenhand520 on 2026/4/25.
//

#pragma once

// include/hexapod_servo_control/servo_system.hpp
#ifndef HEXAPOD_SERVO_CONTROL__SERVO_SYSTEM_HPP_
#define HEXAPOD_SERVO_CONTROL__SERVO_SYSTEM_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "hexapod_ros2_control/ubtech_servo.hpp"

// libgpiod 前向声明
struct gpiod_chip;
struct gpiod_line;

namespace hexapod_ros2_control {

/// @brief 单个舵机在总线上的配置
struct ServoJointConfig {
  uint8_t  id;             // 总线舵机ID (1-240)
  std::string joint_name;  // URDF中的关节名
  double   offset_deg;     // 零位偏移(度): 关节零位对应的舵机角度
  double   direction;      // 方向: 1.0 或 -1.0
};

/// @brief ros2_control 硬件接口：六足机器人18舵机控制系统
///
/// 架构:
///   ServoSystem (SystemInterface)
///       └── ServoBase* (抽象接口)
///              └── UbtechServo / 其他舵机实现
///
/// 更换舵机时:
///   1. 新建类继承 ServoBase, 实现协议
///   2. 在 create_servo_driver() 工厂方法中注册
///   3. 修改 URDF 中 servo_type 参数
///   ServoSystem 本身代码无需改动。
class ServoSystem : public hardware_interface::SystemInterface {
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(ServoSystem)

  // ── 生命周期回调 ──────────────────────────────────────────
  hardware_interface::CallbackReturn on_init(
      const hardware_interface::HardwareInfo& info) override;

  hardware_interface::CallbackReturn on_configure(
      const rclcpp_lifecycle::State& previous_state) override;

  hardware_interface::CallbackReturn on_cleanup(
      const rclcpp_lifecycle::State& previous_state) override;

  hardware_interface::CallbackReturn on_activate(
      const rclcpp_lifecycle::State& previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
      const rclcpp_lifecycle:: State& previous_state) override;

  // ── 接口导出 ──────────────────────────────────────────────
  std::vector<hardware_interface::StateInterface>
  export_state_interfaces() override;

  std::vector<hardware_interface::CommandInterface>
  export_command_interfaces() override;

  // ── 周期读写 ──────────────────────────────────────────────
  hardware_interface::return_type read(
      const rclcpp::Time& time, const rclcpp::Duration& period) override;

  hardware_interface::return_type write(
      const rclcpp::Time& time, const rclcpp::Duration& period) override;

private:
  // ── 串口操作 ──────────────────────────────────────────────
  bool open_serial();
  void close_serial();
  bool write_serial(const uint8_t* data, size_t length);
  int read_serial(uint8_t* buffer, size_t max_len, int timeout_ms);

  // ── GPIO (STX引脚) ───────────────────────────────────────
  bool open_gpio();
  void close_gpio();
  void set_stx(bool high);

  // ── 舵机驱动工厂 ─────────────────────────────────────────
  /// 根据名称创建对应的舵机协议实现
  /// 新增舵机类型时只需在此方法中添加分支
  ServoBase::UniquePtr create_servo_driver(const std::string& type);

  // ── 角度转换 ──────────────────────────────────────────────
  /// 关节弧度 → 舵机角度(度)
  double joint_rad_to_servo_deg(double rad, const ServoJointConfig& cfg) const;
  /// 舵机角度(度) → 关节弧度
  double servo_deg_to_joint_rad(double deg, const ServoJointConfig& cfg) const;

  // ── 成员变量 ──────────────────────────────────────────────
  // 舵机协议驱动 (通过抽象接口调用)
  ServoBase::UniquePtr servo_driver_;

  // 各关节舵机配置
  std::vector<ServoJointConfig> servo_configs_;

  // ros2_control 接口数据
  std::vector<double> hw_commands_;    // 收到的指令(弧度)
  std::vector<double> hw_positions_;   // 反馈的状态(弧度)

  // 预分配的发送/接收缓冲区(避免实时循环中分配内存)
  std::vector<uint8_t> tx_buffer_;
  std::vector<uint8_t> rx_buffer_;

  // 串口
  int serial_fd_ = -1;
  std::string serial_port_;
  int baud_rate_ = 921600;

  // GPIO (STX方向控制)
  gpiod_chip* gpio_chip_ = nullptr;
  gpiod_line* stx_line_ = nullptr;
  std::string gpio_chip_name_;
  unsigned int stx_pin_ = 0;

  // 状态标志
  bool configured_ = false;
  std::atomic<bool> activated_{false};

  // 线程安全
  std::mutex comm_mutex_;

  // 读取角度的控制
  bool enable_angle_read_ = true;   // 是否在read()中回读角度
  int read_skip_counter_ = 0;       // 跳过计数器
  int read_every_n_cycles_ = 10;    // 每N个周期读一次角度
};

}  // namespace hexapod_servo_control

#endif  // HEXAPOD_SERVO_CONTROL__SERVO_SYSTEM_HPP_