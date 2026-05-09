//
// Created by greenhand520 on 2025/5/08.
//

#pragma once

#include <rcl_interfaces/msg/log.hpp>
#include <rclcpp/rclcpp.hpp>
#include <spdlog/spdlog.h>

#include "log_interface/log_interface.hpp"

namespace common_log {

    class SpdlogLoggerAdapter final : public log_interface::LogInterface {

    public:
        explicit SpdlogLoggerAdapter(const std::shared_ptr<spdlog::logger>& logger,
                                     const std::shared_ptr<rclcpp::Node>& node = nullptr);

        void log(Level level, const std::string& file, int line,
                 const std::string& func, const std::string& message) override;

    private:
        std::shared_ptr<spdlog::logger> logger_;
        std::shared_ptr<rclcpp::Node> node_;
        rclcpp::Publisher<rcl_interfaces::msg::Log>::SharedPtr log_pub_;

    public:
        /// 创建spdlog适配器
        /// @param spdlog spdlog对象指针
        /// @param node rclcpp的node对象指针，如果传的话，就会将日志发布到/rosout话题
        static std::shared_ptr<LogInterface> create_spdlog_logger(
        const std::shared_ptr<spdlog::logger>& spdlog, const std::shared_ptr<rclcpp::Node>& node = nullptr);
    };
} // namespace common_log
