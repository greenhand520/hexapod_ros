//
// Created by greenhand520 on 2026/5/3.
//

#pragma once

#include <hardware_interface/handle.hpp>
#include <hardware_interface/hardware_info.hpp>
#include <hardware_interface/system_interface.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/state.hpp>

#include <memory>
#include <string>
#include <vector>

#include "common_cpp/lubancat/gpio_pin.hpp"
#include "common_cpp/lubancat/serial_port.hpp"
#include "ubtech_servo.hpp"

namespace ubtech_servo_hardware {

    class UbtechServoSystem : public hardware_interface::SystemInterface {
    public:
        RCLCPP_SHARED_PTR_DEFINITIONS(UbtechServoSystem)

        hardware_interface::CallbackReturn on_init(
            const hardware_interface::HardwareInfo& info) override;

        hardware_interface::CallbackReturn on_configure(
            const rclcpp_lifecycle::State& prev_state) override;

        hardware_interface::CallbackReturn on_activate(
            const rclcpp_lifecycle::State& prev_state) override;

        hardware_interface::CallbackReturn on_deactivate(
            const rclcpp_lifecycle::State& prev_state) override;

        hardware_interface::CallbackReturn on_cleanup(
            const rclcpp_lifecycle::State& prev_state) override;

        std::vector<hardware_interface::StateInterface> export_state_interfaces() override;
        std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

        hardware_interface::return_type read(
            const rclcpp::Time& time, const rclcpp::Duration& period) override;
        hardware_interface::return_type write(
            const rclcpp::Time& time, const rclcpp::Duration& period) override;

    private:
        std::unique_ptr<lubancat_hw::SerialPort> serial_;
        std::unique_ptr<lubancat_hw::GpioPin> tx_en_;
        std::unique_ptr<UbtechServo> servo_;

        std::string serial_device_;
        int baud_rate_ = 115200;
        std::string gpio_name_;

        /// Per-joint bookkeeping
        struct JointData {
            std::string name;
            uint8_t servo_id;
            /// 校准偏移 (度)
            double angle_offset_deg;
            /// 实际位置 (rad)
            double pos_state;
            /// 实际速度 (rad/s)
            double vel_state;
            /// 目标位置 (rad)
            double pos_cmd;
            /// 上一周期实际位置
            double prev_pos_state;
        };
        std::vector<JointData> joints_;

        static constexpr double DEG2RAD = M_PI / 180.0;
    };

} // namespace ubtech_servo_hardware
