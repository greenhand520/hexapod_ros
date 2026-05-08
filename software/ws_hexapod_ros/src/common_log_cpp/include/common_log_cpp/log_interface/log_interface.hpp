//
// Created by zwh on 25-7-11.
//

#pragma once

#include <string>
#include <fmt/format.h>

namespace log_interface {

    class LogInterface {

    public:
        virtual ~LogInterface() = default;

        // 日志级别
        enum class Level {
            TRACE,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            CRITICAL,
            NONE,
            N_LEVELS
        };

        virtual void log(Level level, const std::string& file, int line,
                         const std::string& func, const std::string& message) = 0;

        virtual void log_fatal(const std::string& /*topic*/, const std::string& file, const int line, const std::string& func, const std::string& message) {
            log(Level::CRITICAL, file, line, func, message);
        }


        template <typename... Args>
        void log_format(const Level level, const std::string& file, const int line,
                        const std::string& func, fmt::format_string<Args...> fmt, Args&&... args) {
            log(level, file, line, func, fmt::format(fmt, std::forward<Args>(args)...));
        }

    };

}