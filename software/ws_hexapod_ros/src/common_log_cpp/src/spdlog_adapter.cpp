//
// Created by zwh on 25-7-11.
//

#include "common_log_cpp/spdlog_adapter.hpp"
#include "common_log_cpp/log_interface/log_manager.hpp"

using namespace common_log;

SpdlogLoggerAdapter::SpdlogLoggerAdapter(const std::shared_ptr<spdlog::logger>& logger) :
    logger(logger) {
}

void SpdlogLoggerAdapter::log(const Level level, const std::string& file, const int line,
                                      const std::string& func,
                                      const std::string& message) {
    if (!logger)
        return;

    const spdlog::source_loc loc{file.c_str(), line, func.c_str()};
    logger->log(loc, static_cast<spdlog::level::level_enum>(level), message);
}


std::shared_ptr<log_interface::LogInterface> SpdlogLoggerAdapter::create_spdlog_logger(const std::shared_ptr<spdlog::logger>& spdlog) {
    return std::make_shared<SpdlogLoggerAdapter>(spdlog);
}

extern "C" void set_logger(const std::shared_ptr<log_interface::LogInterface>& logger) {
    // 不能使用inline
    log_interface::LogManager::set(logger);
}