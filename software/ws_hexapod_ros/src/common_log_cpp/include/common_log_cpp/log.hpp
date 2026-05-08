//
// Created by zwh on 2025/3/28.
//

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <memory>
#include <mutex>
#include <rclcpp/node.hpp>

#define SPDLOG_ENABLE_SOURCE_LOC

inline spdlog::level::level_enum str_to_log_level(const std::string& level_str);

namespace common_log {

    struct LogConfig {
        spdlog::level::level_enum log_level = spdlog::level::info;
        std::string log_path = "./log";
        /// 异步日志
        bool async = true;

        std::string logger_name = "logger";

        [[nodiscard]] std::string get_logger_name() const;

        void init_from_node(const rclcpp::Node::SharedPtr& node);
    };

    class Logger {

        std::string logger_name_;
        std::vector<spdlog::sink_ptr> sinks_;
        std::shared_ptr<spdlog::logger> logger_;
        /// 初始化线程池（队列大小=1000，线程数=1）
        std::shared_ptr<spdlog::details::thread_pool> thread_pool_;
        /// 用于标记logger是否已被正确初始化
        bool initialized_ = false;

        void init_file_log_sink(const LogConfig* log_config);

        void init_console_log_sink(const LogConfig* log_config);

        explicit Logger(const LogConfig* log_config);

        /// 使用默认配置构造未初始化的Logger（避免get_instance未初始化时crash）
        Logger();

    public:
        [[nodiscard]] std::shared_ptr<spdlog::logger> get_logger() const;

        [[nodiscard]] bool is_initialized() const { return initialized_; }

        ~Logger();

        /**
         * 获取Logger单例。
         * 首次调用时如果未通过init_from_config初始化，将使用默认配置创建。
         * 线程安全：使用std::call_once保证。
         */
        static Logger& get_instance();

        /**
         * 使用LogConfig初始化Logger单例。
         * 必须在get_instance之前调用，以确保正确的配置。
         */
        static void init_from_config(const LogConfig* config);

    private:
        static std::unique_ptr<Logger> instance_;
        static std::once_flag init_flag_;
    };

}