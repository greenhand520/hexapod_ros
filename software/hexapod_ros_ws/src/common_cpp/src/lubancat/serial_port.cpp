//
// Created by greenhand520 on 2026/5/4.
//
#include "common_cpp/lubancat/serial_port.hpp"

#include <fcntl.h>
#include <format>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

namespace lubancat_hw {

    SerialPort::SerialPort() : fd_(-1) {
    }

    SerialPort::~SerialPort() {
        close();
    }

    std::expected<bool, std::string> SerialPort::open(const std::string& device, const int baud_rate) {
        device_ = device;

        fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY);
        if (fd_ < 0) {
            return std::unexpected("Failed to open serial port" + device);
        }

        if (!configure(baud_rate)) {
            close();
            return std::unexpected(std::format(
                "Failed to configure baud rate {} for serial port {}", baud_rate, device));
        }

        return true;
    }

    void SerialPort::close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool SerialPort::is_open() const {
        return fd_ >= 0;
    }

    bool SerialPort::configure(const int baud_rate, const int data_bits,
        const int stop_bits, const char parity) const {
        if (fd_ < 0)
            return false;

        struct termios tty{};
        if (tcgetattr(fd_, &tty) != 0) {
            return false;
        }

        // Baud rate
        const speed_t spd = baud_to_speed(baud_rate);
        cfsetispeed(&tty, spd);
        cfsetospeed(&tty, spd);

        // Control flags: 8N1 by default
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= static_cast<tcflag_t>(CS8);
        tty.c_cflag |= CREAD | CLOCAL;

        if (parity == 'E') {
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
        }
        else if (parity == 'O') {
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
        }

        if (stop_bits == 2) {
            tty.c_cflag |= CSTOPB;
        }

        if (data_bits == 7) {
            tty.c_cflag &= ~CSIZE;
            tty.c_cflag |= CS7;
        }

        // Input: raw
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

        // Output: raw
        tty.c_oflag &= ~OPOST;
        tty.c_oflag &= ~ONLCR;

        // Local: raw
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

        // Read behavior: return immediately with whatever is available
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            return false;
        }

        tcflush(fd_, TCIOFLUSH);
        return true;
    }

    int SerialPort::write_data(const uint8_t* data, const size_t length) const {
        if (fd_ < 0)
            return -1;
        const ssize_t n = ::write(fd_, data, length);
        if (n > 0)
            tcdrain(fd_); // block until output has been transmitted
        return static_cast<int>(n);
    }

    int SerialPort::read_data(uint8_t* buffer, const size_t length, const int timeout_ms) const {
        if (fd_ < 0)
            return -1;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);

        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        if (const int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv); ret > 0 && FD_ISSET(fd_, &fds)) {
            const ssize_t n = ::read(fd_, buffer, length);
            return static_cast<int>(n);
        }

        return 0; // timeout
    }

    void SerialPort::flush() const {
        if (fd_ >= 0)
            tcflush(fd_, TCIOFLUSH);
    }

    speed_t SerialPort::baud_to_speed(const int baud_rate) {
        switch (baud_rate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        case 460800:
            return B460800;
        case 921600:
            return B921600;
        default:
            return B115200;
        }
    }

} // namespace ubtech_servo_hardware
