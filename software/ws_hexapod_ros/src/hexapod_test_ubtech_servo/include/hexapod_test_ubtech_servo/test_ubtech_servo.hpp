//
// Created by greenhand520 on 2026/5/4.
//

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <memory>

#include "hexapod_ubtech_ros2_control/gpio_pin.hpp"
#include "hexapod_ubtech_ros2_control/serial_port.hpp"
#include "hexapod_ubtech_ros2_control/ubtech_servo.hpp"

#include "ros2_msg.hpp"

namespace ubtech_servo_hardware {

    class TestUbtechServo {
    public:
        explicit TestUbtechServo(const rclcpp::Node::SharedPtr& node);

    private:
        rclcpp::Node::SharedPtr node_;
        rclcpp::Logger logger_;

        std::unique_ptr<SerialPort> serial_;
        std::unique_ptr<GpioPin> tx_en_;
        std::unique_ptr<UbtechServo> servo_;

        rclcpp::Service<ModifyId>::SharedPtr srv_modify_id_;
        rclcpp::Service<ReadAngle>::SharedPtr srv_read_angle_;
        rclcpp::Service<ReadFirmware>::SharedPtr srv_read_firmware_;
        rclcpp::Service<ReadOffset>::SharedPtr srv_read_offset_;
        rclcpp::Service<SetAngle>::SharedPtr srv_set_angle_;
        rclcpp::Service<SetOffset>::SharedPtr srv_set_offset_;
        rclcpp::Service<StopServo>::SharedPtr srv_stop_;

        bool init_hardware();

    };

} // namespace ubtech_servo_hardware
