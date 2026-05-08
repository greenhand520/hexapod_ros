//
// Created by zwh on 2025/4/29.
//

#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>

#include "common_log_cpp/log.hpp"

spdlog::level::level_enum str_to_log_level(const std::string& level_str) {
    static const std::unordered_map<std::string, spdlog::level::level_enum> levelMap = {
        {"trace", spdlog::level::level_enum::trace},
        {"debug", spdlog::level::level_enum::debug},
        {"info", spdlog::level::level_enum::info},
        {"warn", spdlog::level::level_enum::warn},
        {"warning", spdlog::level::level_enum::warn},
        {"error", spdlog::level::level_enum::err},
        // 关键/致命，高级别的日志等级，用于记录 严重错误或不可恢复的系统故障，通常表示程序即将崩溃或必须立即终止运行
        {"critical", spdlog::level::level_enum::critical},
        {"off", spdlog::level::level_enum::off},
        {"none", spdlog::level::level_enum::off}};

    if (const auto it = levelMap.find(level_str); it != levelMap.end()) {
        return it->second;
    }
    return spdlog::level::level_enum::info;
}

using namespace common_log;

std::string LogConfig::get_log_name() const {
    if (log_node) {
        return log_node->get_name();
    }
    return "logger";
}

void LogConfig::init_from_node(const rclcpp::Node::SharedPtr& node) {
    node->declare_parameter("log.log_level", "info");
    node->declare_parameter("log.topic_log_level", "off");
    node->declare_parameter("log.log_path", "./log");
    node->declare_parameter("log.async", false);

    log_level = str_to_log_level(node->get_parameter("log.log_level").as_string());
    topic_log_level = str_to_log_level(node->get_parameter("log.topic_log_level").as_string());
    log_path = node->get_parameter("log.log_path").as_string();
    async = node->get_parameter("log.async").as_bool();
    log_node = node;
}

void Logger::init_file_log_sink(const LogConfig* log_config) {
    spdlog::sink_ptr file_sink;
    const std::string log_file = std::format("{}/log_{}.log", log_config->log_path, log_name_);
    if (log_config->async) {
        file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            log_file, 0, 0);
    }
    else {
        file_sink = std::make_shared<spdlog::sinks::daily_file_sink_st>(
            log_file, 0, 0);
    }
    file_sink->set_level(log_config->log_level);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%# %!] %v");
    sinks_.push_back(file_sink);
}

void Logger::init_console_log_sink(const LogConfig* log_config) {
    // debug日志时添加控制台输出的sink
    spdlog::sink_ptr console_sink;
    if (log_config->async) {
        console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    }
    else {
        console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    }
    console_sink->set_level(log_config->log_level);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] <thread %t> [%s:%# %!] %v");
    sinks_.push_back(console_sink);
}

// 静态成员定义
std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::once_flag Logger::init_flag_;

Logger::Logger() {
    // 默认构造，创建一个最小化的logger，避免未初始化时crash
    log_name_ = "default";
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    console_sink->set_level(spdlog::level::info);
    sinks_.push_back(console_sink);
    logger_ = std::make_shared<spdlog::logger>(log_name_, sinks_.begin(), sinks_.end());
    logger_->set_level(spdlog::level::info);
    initialized_ = false; // 默认构造不算真正初始化
}

Logger::Logger(const LogConfig* log_config) {
    if (!log_config) {
        std::cout << "Log config is null" << std::endl;
        throw std::runtime_error("Log config is null");
    }
    log_name_ = log_config->get_log_name();

    // [%^%l%$] 显示带颜色日志登记
    // <thread %t> 显示线程id
    // [%s:%# %!] 显示源文件 行号 函数
    // %v 显示日志本体
    init_file_log_sink(log_config);
    init_console_log_sink(log_config);
    // 直接订阅/rosout即可获得日志，无需再次实现
    // if (log_config->topic_log_level < spdlog::level::off) {
    //     init_topic_log_sink(log_config);
    // }

    if (log_config->async) {
        thread_pool_ = std::make_shared<spdlog::details::thread_pool>(1000, 1);
        logger_ = std::make_shared<spdlog::async_logger>(log_name_, sinks_.begin(), sinks_.end(), thread_pool_);
    }
    else {
        logger_ = std::make_shared<spdlog::logger>(log_name_, sinks_.begin(), sinks_.end());
    }

    logger_->set_level(log_config->log_level);
    logger_->flush_on(log_config->log_level);

    register_logger(logger_);
    initialized_ = true;
}

std::shared_ptr<spdlog::logger> Logger::get_logger() const {
    return logger_;
}

Logger::~Logger() {
    // 先刷新确保所有日志写入完成，再清理资源
    if (logger_) {
        logger_->flush();
    }
    // 只清理自己创建的 logger，不使用spdlog::shutdown()
    // spdlog::shutdown()会关闭所有已注册的 spdlog logger，导致其他logger意外被销毁
    spdlog::drop(log_name_);
}

Logger& Logger::get_instance() {
    // 如果尚未通过init_from_config初始化，则使用默认配置创建
    // std::call_once保证线程安全
    std::call_once(init_flag_, []() {
        if (!instance_) {
            instance_ = std::unique_ptr<Logger>(new Logger());
        }
    });
    return *instance_;
}

void Logger::init_from_config(const LogConfig* config) {
    if (!config) {
        throw std::runtime_error("Log config is null");
    }
    // 使用call_once确保只初始化一次
    std::call_once(init_flag_, [config]() {
        instance_ = std::unique_ptr<Logger>(new Logger(config));
    });
}
