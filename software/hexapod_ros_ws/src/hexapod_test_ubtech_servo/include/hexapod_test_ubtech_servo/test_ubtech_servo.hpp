//
// Created by greenhand520 on 2026/5/4.
//

#pragma once

#include <rclcpp/rclcpp.hpp>

#include <memory>

#include "common_cpp/lubancat/gpio_pin.hpp"
#include "common_cpp/lubancat/serial_port.hpp"
#include "hexapod_ubtech_ros2_control/ubtech_servo.hpp"

#include "ros2_msg.hpp"

namespace ubtech_servo_hardware {

    class TestUbtechServo {
    public:
        explicit TestUbtechServo(const rclcpp::Node::SharedPtr& node);

    private:
        rclcpp::Node::SharedPtr node_;
        rclcpp::Logger logger_;

        std::unique_ptr<lubancat_hw::SerialPort> serial_;
        std::unique_ptr<lubancat_hw::GpioPin> tx_en_;
        std::unique_ptr<UbtechServo> servo_;

        rclcpp::Service<ModifyIdSrv>::SharedPtr srv_modify_id_;
        rclcpp::Service<ReadAngleSrv>::SharedPtr srv_read_angle_;
        rclcpp::Service<ReadFirmwareSrv>::SharedPtr srv_read_firmware_;
        rclcpp::Service<ReadOffsetSrv>::SharedPtr srv_read_offset_;
        rclcpp::Service<SetAngleSrv>::SharedPtr srv_set_angle_;
        rclcpp::Service<SetOffsetSrv>::SharedPtr srv_set_offset_;
        rclcpp::Service<StopServoSrv>::SharedPtr srv_stop_servo_;
        rclcpp::Service<SetAnglesSrv>::SharedPtr srv_set_angles_;

        bool init_hardware();

        void execute_modify_id(const std::shared_ptr<ModifyIdSrv::Request>& req,
                                const std::shared_ptr<ModifyIdSrv::Response>& res) const;

        void execute_read_angle(const std::shared_ptr<ReadAngleSrv::Request>& req,
                                const std::shared_ptr<ReadAngleSrv::Response>& res) const;

        void execute_read_firmware(const std::shared_ptr<ReadFirmwareSrv::Request>& req,
                                const std::shared_ptr<ReadFirmwareSrv::Response>& res) const;

        void execute_read_offset(const std::shared_ptr<ReadOffsetSrv::Request>& req,
                                const std::shared_ptr<ReadOffsetSrv::Response>& res) const;

        void execute_set_angle(const std::shared_ptr<SetAngleSrv::Request>& req,
                                const std::shared_ptr<SetAngleSrv::Response>& res) const;

        void execute_set_offset(const std::shared_ptr<SetOffsetSrv::Request>& req,
                                const std::shared_ptr<SetOffsetSrv::Response>& res) const;

        void execute_stop_servo(const std::shared_ptr<StopServoSrv::Request>& req,
                                const std::shared_ptr<StopServoSrv::Response>& res) const;

        void execute_set_angles(const std::shared_ptr<SetAnglesSrv::Request>& req,
                                const std::shared_ptr<SetAnglesSrv::Response>& res) const;
    };

} // namespace ubtech_servo_hardware
