//
// Created by zwh on 25-7-15.
//

#pragma once

#include "log_manager.hpp"

#define LOG(level, ...) \
if (auto _logger_ = log_interface::LogManager::get()) { \
_logger_->log_format(log_interface::LogInterface::Level::level, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
}

#define LOG_TRACE(...)   LOG(TRACE, __VA_ARGS__)
#define LOG_DEBUG(...)   LOG(DEBUG, __VA_ARGS__)
#define LOG_INFO(...)    LOG(INFO, __VA_ARGS__)
#define LOG_WARN(...)    LOG(WARN, __VA_ARGS__)
#define LOG_ERROR(...)   LOG(ERROR, __VA_ARGS__)
#define LOG_FATAL(...) LOG(CRITICAL, __VA_ARGS__)

#define PUB_FATAL(topic, fmt, ...) \
if (auto _logger_ = log_interface::LogManager::get()) { \
_logger_->log_fatal(topic, __FILE__, __LINE__, __FUNCTION__, std::format(fmt, ##__VA_ARGS__)); \
}
//