//
// Created by greenhand520 on 2026/5/4.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <termios.h>

namespace ubtech_servo_hardware {

    /// @brief POSIX serial port wrapper with configurable baud rate and framing.
    class SerialPort {
    public:
        SerialPort();
        ~SerialPort();

        SerialPort(const SerialPort&) = delete;
        SerialPort& operator=(const SerialPort&) = delete;

        /// @brief Open and configure the serial port.
        /// @param device   Device path, e.g. "/dev/ttyUSB0", "/dev/ttyAMA0"
        /// @param baud_rate  Baud rate (9600, 19200, 38400, 57600, 115200, etc.)
        /// @return true on success
        std::expected<bool, std::string> open(const std::string& device, int baud_rate);

        void close();
        [[nodiscard]] bool is_open() const;

        /// @brief Reconfigure the port parameters.
        [[nodiscard]] bool configure(int baud_rate, int data_bits = 8, int stop_bits = 1, char parity = 'N') const;

        /// @brief Write raw bytes. Returns number of bytes written, or -1 on error.
        int write_data(const uint8_t* data, size_t length) const;

        /// @brief Read up to @p length bytes with a timeout.
        /// @param buffer     Destination buffer
        /// @param length     Maximum bytes to read
        /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
        /// @return Number of bytes read, 0 on timeout, -1 on error
        int read_data(uint8_t* buffer, size_t length, int timeout_ms = 100) const;

        /// @brief Drain both input and output buffers.
        void flush() const;

    private:
        int fd_;
        std::string device_;

        static speed_t baud_to_speed(int baud_rate);
    };

} // namespace ubtech_servo_hardware
