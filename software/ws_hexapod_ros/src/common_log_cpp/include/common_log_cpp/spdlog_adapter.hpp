//
// Created by zwh on 25-7-11.
//

#pragma once

#include <spdlog/spdlog.h>
#include <rclcpp/rclcpp.hpp>

#include "log_interface/log_interface.hpp"

namespace common_log {

    class SpdlogLoggerAdapter final : public log_interface::LogInterface {

    public:
        explicit SpdlogLoggerAdapter(const std::shared_ptr<spdlog::logger>& logger);

        void log(Level level, const std::string& file, int line,
                 const std::string& func, const std::string& message) override;

    private:
        std::shared_ptr<spdlog::logger> logger;

    public:
        /// 创建spdlog适配器
        static std::shared_ptr<LogInterface> create_spdlog_logger(const std::shared_ptr<spdlog::logger>& spdlog);
    };
}