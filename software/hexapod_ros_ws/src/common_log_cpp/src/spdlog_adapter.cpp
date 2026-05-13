//
// Created by greenhand520 on 2025/5/08.
//

#include "common_log_cpp/spdlog_adapter.hpp"
#include "common_log_cpp/log_interface/log_manager.hpp"

using namespace common_log;

SpdlogLoggerAdapter::SpdlogLoggerAdapter(const std::shared_ptr<spdlog::logger>& logger,
                                         const std::shared_ptr<rclcpp::Node>& node) :
    logger_(logger), node_(node) {
    if (node_) {
        log_pub_ = node_->create_publisher<rcl_interfaces::msg::Log>(
            "/rosout",
            rclcpp::QoS(1000));
    }
}

void SpdlogLoggerAdapter::log(const Level level, const std::string& file, const int line,
                              const std::string& func,
                              const std::string& message) {
    if (!logger_)
        return;

    if (node_) {
        rcl_interfaces::msg::Log log_msg;
        log_msg.file = file;
        log_msg.line = line;
        log_msg.function = func;
        log_msg.msg = message;
        log_msg.name = node_->get_name();
        log_pub_->publish(log_msg);
    }
    const spdlog::source_loc loc{file.c_str(), line, func.c_str()};
    logger_->log(loc, static_cast<spdlog::level::level_enum>(level), message);
}


std::shared_ptr<log_interface::LogInterface> SpdlogLoggerAdapter::create_spdlog_logger(
    const std::shared_ptr<spdlog::logger>& spdlog,
    const std::shared_ptr<rclcpp::Node>& node) {
    return std::make_shared<SpdlogLoggerAdapter>(spdlog, node);
}

extern "C" void set_logger(const std::shared_ptr<log_interface::LogInterface>& logger) {
    // 不能使用inline
    log_interface::LogManager::set(logger);
}
