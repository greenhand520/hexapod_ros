//
// Created by greenhand520 on 2026/5/4.
//


#include "common_cpp/lubancat/gpio_pin.hpp"

#include <expected>
#include <format>
#include <gpiod.h>
#include <stdexcept>

namespace lubancat_hw {

    GpioPin::GpioPin() : chip_(nullptr), line_(nullptr), line_num_(0) {
    }

    GpioPin::~GpioPin() {
        close();
    }

    std::expected<bool, std::string> GpioPin::open(
        const std::string& chip_path, const unsigned int line_num,
        const std::string& consumer, const GPIOValue default_val) {
        chip_path_ = chip_path;
        line_num_ = line_num;

        chip_ = gpiod_chip_open(chip_path.c_str());
        if (!chip_) {
            return std::unexpected("Failed to open gpio chip " + chip_path);
        }

        line_ = gpiod_chip_get_line(chip_, line_num);
        if (!line_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            return std::unexpected(std::format(
                "Failed to get line {} from gpio chip {}", line_num, chip_path));
        }

        if (gpiod_line_request_output(line_, consumer.c_str(), default_val) < 0) {
            gpiod_line_release(line_);
            gpiod_chip_close(chip_);
            line_ = nullptr;
            chip_ = nullptr;
            return std::unexpected(std::format(
                "Failed to request line {} to {} from gpio chip {}",
                line_num, static_cast<int>(default_val), chip_path));
        }

        return true;
    }

    std::expected<bool, std::string> GpioPin::open(const std::string& gpio_name,
                       const std::string& consumer, const GPIOValue default_val) {
        if (gpio_name.rfind("GPIO", 0) != 0) {
            return std::unexpected("Invalid format, must start with GPIO");
        }

        const size_t underscore = gpio_name.find('_');
        if (underscore == std::string::npos) {
            return std::unexpected("Invalid format, missing underscore");
        }
        // 解析控制器号
        chip_path_ = "/dev/gpiochip" + gpio_name.substr(0, underscore);
        // 解析端口号（A/B/C/D）
        const char port = gpio_name[underscore + 1];
        int port_value = 0;
        switch (std::toupper(port)) {
        case 'A':
            port_value = 0;
            break;
        case 'B':
            port_value = 1;
            break;
        case 'C':
            port_value = 2;
            break;
        case 'D':
            port_value = 3;
            break;
        default:
            return std::unexpected("Invalid port, must be A/B/C/D");
        }
        // 解析索引号（下划线后的数字部分）
        const int index = std::stoi(gpio_name.substr(underscore + 2));
        // 计算 line 号: 8 * 端口号数值 + 索引号
        const int line = 8 * port_value + index;
        return open(chip_path_, line, consumer, default_val);
    }

    void GpioPin::close() {
        if (line_) {
            gpiod_line_release(line_);
            line_ = nullptr;
        }
        if (chip_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    bool GpioPin::is_open() const {
        return chip_ != nullptr && line_ != nullptr;
    }

    bool GpioPin::set_value(const GPIOValue value) const {
        if (!line_)
            return false;
        return gpiod_line_set_value(line_, value) == 0;
    }

    int GpioPin::get_value() const {
        if (!line_)
            return -1;
        return gpiod_line_get_value(line_);
    }

} // namespace ubtech_servo_hardware
