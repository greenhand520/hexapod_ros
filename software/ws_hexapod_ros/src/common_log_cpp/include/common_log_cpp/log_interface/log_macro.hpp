//
// Created by zwh on 25-7-15.
//

#pragma once

#include "log_manager.hpp"

#define LOG(level, ...)                                                                                                 \
    if (auto _logger_ = log_interface::LogManager::get()) {                                                             \
        _logger_->log_format(log_interface::LogInterface::Level::level, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
    }

#define LOG_TRACE(...) LOG(TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(DEBUG, __VA_ARGS__)
#define LOG_INFO(...) LOG(INFO, __VA_ARGS__)
#define LOG_WARN(...) LOG(WARN, __VA_ARGS__)
#define LOG_ERROR(...) LOG(ERROR, __VA_ARGS__)
#define LOG_FATAL(...) LOG(CRITICAL, __VA_ARGS__)


#define LOG_WITH_FUNC(level, func, ...)                                 \
    if (auto _logger_ = log_interface::LogManager::get()) {             \
        _logger_->log_format(log_interface::LogInterface::Level::level, \
                             __FILE__, __LINE__, func, __VA_ARGS__);    \
    }

#define LOG_TRACE_WITH_FUNC(func, ...) LOG_WITH_FUNC(TRACE, func, __VA_ARGS__)
#define LOG_DEBUG_WITH_FUNC(func, ...) LOG_WITH_FUNC(DEBUG, func, __VA_ARGS__)
#define LOG_INFO_WITH_FUNC(func, ...) LOG_WITH_FUNC(INFO, func, __VA_ARGS__)
#define LOG_WARN_WITH_FUNC(func, ...) LOG_WITH_FUNC(WARN, func, __VA_ARGS__)
#define LOG_ERROR_WITH_FUNC(func, ...) LOG_WITH_FUNC(ERROR, func, __VA_ARGS__)
#define LOG_FATAL_WITH_FUNC(func, ...) LOG_WITH_FUNC(CRITICAL, func, __VA_ARGS__)
