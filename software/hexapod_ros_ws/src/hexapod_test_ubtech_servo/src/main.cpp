//
// Created by greenhand520 on 2026/5/4.
//

#include <rclcpp/rclcpp.hpp>
#include "hexapod_test_ubtech_servo/test_ubtech_servo.hpp"

#include "common_log_cpp/rclcpp_log_handler.hpp"
#include "common_log_cpp/spdlog_adapter.hpp"
#include "common_log_cpp/log_interface/log_manager.hpp"

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<rclcpp::Node>("hexapod_test_ubtech_servo_node");

    common_log::RclcppLogHandler::init_from_node(node);
    const auto logger = common_log::Logger::get_instance().get_logger();
    const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger, node);
    log_interface::LogManager::set(logger_interface);

    ubtech_servo_hardware::TestUbtechServo test_servo(node);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
