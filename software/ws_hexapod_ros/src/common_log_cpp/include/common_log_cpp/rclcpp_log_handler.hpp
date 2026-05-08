//
// Created by zwh on 25-7-25.
//

#pragma once

#include <chrono>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include <rcutils/logging.h>

#include "log.hpp"

namespace common_log {
    class RclcppLogHandler {

    public:
        static void init_from_node(const rclcpp::Node::SharedPtr& node) {
            LogConfig log_config;
            log_config.init_from_node(node);
            Logger::init_from_config(&log_config);
            const rcutils_ret_t set_level_res = spdlog_level_to_rcutils(log_config.log_level);
            RCLCPP_INFO(node->get_logger(), "Logger %s set level res: %d", node->get_logger().get_name(), set_level_res);
            rcutils_logging_set_output_handler(&RclcppLogHandler::log_output_handler);
        }

    private:
        /// 将rcutils日志等级转换为spdlog日志等级
        static spdlog::level::level_enum rcutils_severity_to_spdlog(const int severity) {
            switch (severity) {
            case RCUTILS_LOG_SEVERITY_DEBUG:
                return spdlog::level::debug;
            case RCUTILS_LOG_SEVERITY_INFO:
                return spdlog::level::info;
            case RCUTILS_LOG_SEVERITY_WARN:
                return spdlog::level::warn;
            case RCUTILS_LOG_SEVERITY_ERROR:
                return spdlog::level::err;
            case RCUTILS_LOG_SEVERITY_FATAL:
                return spdlog::level::critical;
            default:
                return spdlog::level::info;
            }
        }

        static rcutils_ret_t spdlog_level_to_rcutils(const spdlog::level::level_enum level) {
            switch (level) {
            case spdlog::level::debug:    return RCUTILS_LOG_SEVERITY_DEBUG;
            case spdlog::level::info:     return RCUTILS_LOG_SEVERITY_INFO;
            case spdlog::level::warn:     return RCUTILS_LOG_SEVERITY_WARN;
            case spdlog::level::err:      return RCUTILS_LOG_SEVERITY_ERROR;
            case spdlog::level::critical: return RCUTILS_LOG_SEVERITY_FATAL;
            default:                      return RCUTILS_LOG_SEVERITY_INFO;
            }
        }

        static void log_output_handler(const rcutils_log_location_t* location, const int severity, const char* name,
                                       rcutils_time_point_value_t timestamp, const char* format, va_list* args) {

            const spdlog::level::level_enum level = rcutils_severity_to_spdlog(severity);

            // 使用vsnprintf的动态缓冲区避免固定大小缓冲区溢出
            va_list args_copy;
            va_copy(args_copy, *args);
            const int needed = vsnprintf(nullptr, 0, format, args_copy);
            va_end(args_copy);

            if (needed < 0) return;

            std::vector<char> buffer(static_cast<size_t>(needed) + 1);
            vsnprintf(buffer.data(), buffer.size(), format, *args);

            const spdlog::source_loc loc{location->file_name, static_cast<int>(location->line_number),
                                         location->function_name};

            // 将rclcpp的时间戳(nanoseconds since epoch)转换为spdlog::log_clock::time_point
            const auto duration = std::chrono::duration_cast<spdlog::log_clock::duration>(
                std::chrono::nanoseconds(timestamp));
            const spdlog::log_clock::time_point log_time{duration};

            // spdlog带时间戳的log重载只接受string_view_t，不支持格式化参数，需预先格式化
            const auto msg = fmt::format("[{}] {}", name, buffer.data());
            Logger::get_instance().get_logger()->log(log_time, loc, level, spdlog::string_view_t{msg.data(), msg.size()});
        }
    };

}