//
// Created by greenhand520 on 2026/5/4.
//

#include <rclcpp/rclcpp.hpp>
#include "hexapod_test_ubtech_servo/test_ubtech_servo.hpp"

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<rclcpp::Node>("hexapod_test_ubtech_servo_node");
    // common_log::RclcppLogHandler::init_from_node(node);
    // const auto logger = common_log::SpdlogLoggerAdapter::create_spdlog_logger(
    //     common_log::Logger::get_instance(nullptr).get_logger(), node.get());
    // log_interface::LogManager::set(logger);

    ubtech_servo_hardware::TestUbtechServo test_servo(node);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
