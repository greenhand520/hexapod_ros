//
// Created by greenhand520 on 2026/5/4.
//

#include <rclcpp/rclcpp.hpp>

#include "common_cpp/log/log_interface/log_manager.hpp"
#include "common_cpp/log/rclcpp_log_handler.hpp"
#include "common_cpp/log/spdlog_adapter.hpp"

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    const auto node = std::make_shared<rclcpp::Node>("hexapod_controller_node");

    common_log::RclcppLogHandler::init_from_node(node);
    const auto logger = common_log::Logger::get_instance().get_logger();
    const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger, node);
    log_interface::LogManager::set(logger_interface);

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
