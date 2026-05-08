//
// Created by greenhand520 on 25-7-15.
//

#pragma once

#include <memory>
#include "log_interface.hpp"

namespace log_interface {
    class LogManager {

        inline static std::shared_ptr<LogInterface> logger;

    public:
        static void set(const std::shared_ptr<LogInterface>& new_logger) {
            logger = new_logger;
        }

        static LogInterface* get() {
            return logger.get();
        }
    };

    extern "C" void set_logger(const std::shared_ptr<LogInterface>& logger);
}