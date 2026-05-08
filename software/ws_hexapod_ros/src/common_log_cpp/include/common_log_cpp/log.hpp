//
// Created by zwh on 2025/3/28.
//

#pragma once

#include <memory>
#include <mutex>
#include <rclcpp/node.hpp>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>

#define SPDLOG_ENABLE_SOURCE_LOC

namespace common_log {

    static const std::unordered_map<std::string, spdlog::level::level_enum> LOG_LEVEL_MAP = {
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warn", spdlog::level::warn},
        {"warning", spdlog::level::warn},
        {"error", spdlog::level::err},
        {"critical", spdlog::level::critical},
        {"off", spdlog::level::off},
        {"none", spdlog::level::off},
    };

    inline spdlog::level::level_enum str_to_log_level(const std::string& level_str);

    // 通用：用 istringstream 处理 int / double / float 等
    template <typename T>
    T str_to(const std::string& str, const T& fallback) {
        std::istringstream iss(str);
        T val{};
        if (iss >> val)
            return val;
        return fallback;
    }

    // string 不需要转换
    template <>
    inline std::string str_to<std::string>(const std::string& str, const std::string&) {
        return str;
    }

    // bool 特化
    template <>
    inline bool str_to<bool>(const std::string& str, const bool& fallback) {
        if (str == "true" || str == "1" || str == "True" || str == "TRUE")
            return true;
        if (str == "false" || str == "0" || str == "False" || str == "FALSE")
            return false;
        return fallback;
    }

    template <>
    inline spdlog::level::level_enum str_to<spdlog::level::level_enum>(
        const std::string& str, const spdlog::level::level_enum& fallback) {
        const auto it = LOG_LEVEL_MAP.find(str);
        return (it != LOG_LEVEL_MAP.end()) ? it->second : fallback;
    }

    template <typename T>
    T get_or(const std::unordered_map<std::string, std::string>& m,
             const std::string& key, const T& default_val) {
        const auto it = m.find(key);
        if (it == m.end()) {
            return default_val;
        }
        return str_to<T>(it->second, default_val);
    }

    struct LogConfig {
        spdlog::level::level_enum log_level = spdlog::level::info;
        std::string log_path = "./log";
        /// 异步日志
        bool async = true;

        std::string logger_name = "logger";

        [[nodiscard]] std::string get_logger_name() const;

        void init_from_node(const rclcpp::Node::SharedPtr& node);

        void init_from_map(const std::unordered_map<std::string, std::string>& m) {
            log_level = get_or(m, "log.log_level", log_level);
            log_path = get_or(m, "log.log_path", log_path);
            async = get_or(m, "log.async", async);
            logger_name = get_or(m, "log.logger_name", logger_name);
          }
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

        [[nodiscard]] bool is_initialized() const {
            return initialized_;
        }

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

} // namespace common_log
