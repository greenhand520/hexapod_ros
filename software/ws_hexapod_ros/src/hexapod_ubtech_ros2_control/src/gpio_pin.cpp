//
// Created by greenhand520 on 2026/5/4.
//


#include "hexapod_ubtech_ros2_control/gpio_pin.hpp"

#include <gpiod.h>

namespace ubtech_servo_hardware {

    GpioPin::GpioPin() : chip_(nullptr), line_(nullptr), line_num_(0) {
    }

    GpioPin::~GpioPin() {
        close();
    }

    bool GpioPin::open(
        const std::string& chip_path, const unsigned int line_num,
        const std::string& consumer, const int default_val) {
        chip_path_ = chip_path;
        line_num_ = line_num;

        chip_ = gpiod_chip_open(chip_path.c_str());
        if (!chip_) {
            return false;
        }

        line_ = gpiod_chip_get_line(chip_, line_num);
        if (!line_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
            return false;
        }

        if (gpiod_line_request_output(line_, consumer.c_str(), default_val) < 0) {
            gpiod_line_release(line_);
            gpiod_chip_close(chip_);
            line_ = nullptr;
            chip_ = nullptr;
            return false;
        }

        return true;
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
