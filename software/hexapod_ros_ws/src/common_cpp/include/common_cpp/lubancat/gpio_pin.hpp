//
// Created by greenhand520 on 2026/5/4.
//

#pragma once

#include <expected>
#include <string>

// Forward declarations for libgpiod types
struct gpiod_chip;
struct gpiod_line;

namespace lubancat_hw {

    enum GPIOValue : int {
        LOW,
        HIGH
    };

    /// @brief libgpiod-based GPIO output pin controller.
    ///        Used to drive the TX_EN pin of an RS-485 / half-duplex transceiver.
    class GpioPin {
    public:
        GpioPin();
        ~GpioPin();

        GpioPin(const GpioPin&) = delete;
        GpioPin& operator=(const GpioPin&) = delete;

        /// @brief Open a GPIO chip and request a line as output.
        /// @param chip_path  e.g. "/dev/gpiochip0"
        /// @param line_num   GPIO line offset number
        /// @param consumer   Consumer name shown in debugfs
        /// @param default_val Initial output value
        /// @return true on success
        std::expected<bool, std::string> open(const std::string& chip_path, unsigned int line_num,
                  const std::string& consumer = "ubtech_servo", GPIOValue default_val = LOW);

        /// Open a GPIO chip and request a line as output by gpio name
        /// @param gpio_name GPIO1_C4、GPIO3_B2
        /// @param consumer Consumer name shown in debugfs
        /// @param default_val Initial output value
        /// @return
        std::expected<bool, std::string> open(const std::string& gpio_name,
            const std::string& consumer = "ubtech_servo", GPIOValue default_val = LOW);

        void close();
        [[nodiscard]] bool is_open() const;

        /// @brief Set the output value (0 = low, 1 = high).
        [[nodiscard]] bool set_value(GPIOValue value) const;

        /// @brief Read the current value. Returns -1 on error.
        [[nodiscard]] int get_value() const;

    private:
        gpiod_chip* chip_;
        gpiod_line* line_;
        std::string chip_path_;
        unsigned int line_num_;
    };

} // namespace ubtech_servo_hardware
