//
// Created by greenhand520 on 2026/5/3.
//

#include "hexapod_ubtech_ros2_control/ubtech_servo.hpp"

#include <chrono>
#include <thread>

#include "common_cpp/log/log_interface/log_macro.hpp"

namespace ubtech_servo_hardware {

    UbtechServo::UbtechServo(lubancat_hw::SerialPort* serial, lubancat_hw::GpioPin* tx_en) : serial_(serial), tx_en_(tx_en) {
    }

    bool UbtechServo::enable_tx() const {
        if (tx_en_ && tx_en_->is_open()) {
            return tx_en_->set_value(lubancat_hw::GPIOValue::HIGH) == 0;
        }
        return false;
    }

    bool UbtechServo::disable_tx() const {
        if (tx_en_ && tx_en_->is_open()) {
            return tx_en_->set_value(lubancat_hw::GPIOValue::LOW) == 0;
        }
        return false;
    }

    uint8_t UbtechServo::checksum(const uint8_t* data, const size_t count) {
        uint8_t sum = 0;
        for (size_t i = 0; i < count; ++i) {
            sum = static_cast<uint8_t>(sum + data[i]);
        }
        return sum;
    }

    std::expected<bool, std::string> UbtechServo::transmit_frame(const uint8_t* frame, const size_t len) const {
        if (!serial_ || !serial_->is_open())
            return std::unexpected("Serial is not open");

        if (!enable_tx()) {
            return std::unexpected("Cannot set STX_EN pin to HIGH level");
        };
        serial_->flush();

        const int written = serial_->write_data(frame, len);

        // write_data already calls tcdrain(), add a small margin for the
        // transceiver to finish switching.
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        if (disable_tx()) {
            return std::unexpected("Cannot set STX_EN pin to LOW level");
        };

        if (written != static_cast<int>(len)) {
            return std::unexpected("Incomplete frame write: wrote " +
                std::to_string(written) + " of " + std::to_string(len) + " bytes");
        }

        return true;
    }


    std::expected<bool, std::string> UbtechServo::receive_frame(uint8_t* out, const int timeout_ms) const {
        // Read byte-by-byte, synchronise on header FA AF, then collect
        // remaining bytes to form a complete 10-byte frame.
        constexpr int per_byte_timeout = 10;
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms);

        auto read_one = [&, fn = __FUNCTION__](uint8_t& byte) -> bool {
            while (std::chrono::steady_clock::now() < deadline) {
                const int n = serial_->read_data(&byte, 1, per_byte_timeout);
                if (n == 1)
                    return true;
                if (n < 0)
                    return false;
            }
            LOG_ERROR_WITH_FUNC(fn, "Serial receive frame is timeout");
            return false;
        };

        // Sync on header
        uint8_t b;
        while (read_one(b)) {
            if (b != FRAME_HEADER_0)
                continue;
            if (!read_one(b) || b != FRAME_HEADER_1)
                continue;

            // Header found — collect remaining 8 bytes
            out[0] = FRAME_HEADER_0;
            out[1] = FRAME_HEADER_1;
            for (size_t i = 2; i < FRAME_LEN; ++i) {
                if (!read_one(out[i]))
                    return std::unexpected("Failed to read frame body from serial port");
            }

            // Validate tail
            if (out[9] != FRAME_TAIL)
                return std::unexpected("Frame tail mismatch");

            // Validate checksum (Byte2..Byte7 → Byte8)
            if (const uint8_t expected = checksum(&out[2], 6); out[8] != expected)
                return std::unexpected("Checksum failed");

            return true;
        }

        return std::unexpected("Failed to read one byte from serial port");
    }

    // ── Public API ──
    std::expected<bool, std::string> UbtechServo::set_angle(const uint8_t id, uint8_t angle_deg,
                                const uint8_t motion_time, const uint16_t lock_time_ms) {
        if (id == 0 || id > 240)
            return std::unexpected("Invalid servo ID: " + std::to_string(id) + " (must be 1-240)");
        if (angle_deg > MAX_SERVO_ANGLE)
            angle_deg = MAX_SERVO_ANGLE;

        // Build frame: FA AF [ID] [01] [angle] [time] [lock_hi] [lock_lo] [chk] [ED]
        uint8_t frame[FRAME_LEN];
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = id;
        frame[3] = CMD_ROTATE;
        frame[4] = angle_deg;
        frame[5] = motion_time;
        frame[6] = static_cast<uint8_t>((lock_time_ms >> 8) & 0xFF);
        frame[7] = static_cast<uint8_t>(lock_time_ms & 0xFF);
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);

        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return res;
        }

        // Response: 成功回复 0xAA + 舵机ID (2 bytes); 失败不返回数据
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[2] = {};

        if (const int n = serial_->read_data(resp, sizeof(resp), 50); n < 1 || resp[0] != 0xAA) {
            return std::unexpected("Servo ID " + std::to_string(id) + " did not acknowledge set_angle");
        }

        return true;
    }

    std::expected<bool, std::string> UbtechServo::set_angles(const std::vector<AngleCommand>& commands) {
        if (commands.empty())
            return std::unexpected("set_angles: empty command list");

        // Validate all commands and build concatenated frame buffer
        std::vector<uint8_t> buf;
        buf.reserve(commands.size() * FRAME_LEN);

        for (const auto& [id, angle_deg, motion_time, lock_time_ms] : commands) {
            if (id == 0 || id > 240)
                return std::unexpected("Invalid servo ID: " + std::to_string(id) + " (must be 1-240)");

            uint8_t angle_deg_ = angle_deg;
            if (angle_deg_ > MAX_SERVO_ANGLE)
                angle_deg_ = MAX_SERVO_ANGLE;

            uint8_t frame[FRAME_LEN];
            frame[0] = FRAME_HEADER_0;
            frame[1] = FRAME_HEADER_1;
            frame[2] = id;
            frame[3] = CMD_ROTATE;
            frame[4] = angle_deg_;
            frame[5] = motion_time;
            frame[6] = static_cast<uint8_t>((lock_time_ms >> 8) & 0xFF);
            frame[7] = static_cast<uint8_t>(lock_time_ms & 0xFF);
            frame[8] = checksum(&frame[2], 6);
            frame[9] = FRAME_TAIL;

            buf.insert(buf.end(), frame, frame + FRAME_LEN);
        }

        std::lock_guard<std::mutex> lock(bus_mutex_);

        if (auto res = transmit_frame(buf.data(), buf.size()); !res) {
            return res;
        }

        // 等待所有舵机响应 (每个舵机回复 2 字节: 0xAA + ID)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        const size_t resp_len = commands.size() * 2;
        std::vector<uint8_t> resp(resp_len, 0);
        const int n = serial_->read_data(resp.data(), resp_len, 50);

        if (n < 1) {
            return std::unexpected("set_angles: no response from any servo");
        }

        // Check that all servos acknowledged (resp[i] == 0xAA)
        // Some servos may not respond if bus is congested, check what we got
        for (size_t i = 0; i < static_cast<size_t>(n); i += 2) {
            if (resp[i] != 0xAA) {
                return std::unexpected("set_angles: servo at index " +
                    std::to_string(i / 2) + " did not acknowledge (got 0x" +
                    std::to_string(resp[i]) + ")");
            }
        }

        return true;
    }

    std::expected<bool, std::string> UbtechServo::read_angle(const uint8_t id, ServoAngle& angle) {
        if (id == 0 || id > 240)
            return std::unexpected("Invalid servo ID: " + std::to_string(id) + " (must be 1-240)");

        // Build frame: FA AF [ID] [02] [00] [00] [00] [00] [chk] [ED]
        uint8_t frame[FRAME_LEN];
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = id;
        frame[3] = CMD_READ_ANGLE;
        frame[4] = 0x00;
        frame[5] = 0x00;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);

        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return std::unexpected("read_angle transmit failed: " + res.error());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Response: FA AF [??] [??] [target_hi] [target_lo] [actual_hi] [actual_lo] [chk] [ED]
        uint8_t resp[FRAME_LEN] = {};
        if (auto res = receive_frame(resp, 100); !res) {
            return std::unexpected("read_angle receive failed: " + res.error());
        }

        const uint16_t target_raw = (static_cast<uint16_t>(resp[4]) << 8) | resp[5];
        const uint16_t actual_raw = (static_cast<uint16_t>(resp[6]) << 8) | resp[7];

        angle.target_deg = static_cast<double>(target_raw);
        angle.actual_deg = static_cast<double>(actual_raw);

        return true;
    }

    std::expected<bool, std::string> UbtechServo::stop(const uint8_t id) {
        // Build frame: FA AF [ID] [01] [FF] [00] [00] [00] [chk] [ED]
        uint8_t frame[FRAME_LEN];
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = id;
        frame[3] = CMD_ROTATE;
        frame[4] = CMD_STOP_MARKER;
        frame[5] = 0x00;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);
        // 中止命令无回应, 只管发送
        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return std::unexpected("stop transmit failed: " + res.error());
        }
        return true;
    }

    std::expected<bool, std::string> UbtechServo::modify_id(const uint8_t old_id, const uint8_t new_id) {
        // FA AF [old_id] [CD] [00] [new_id] [00] [00] [chk] [ED]
        uint8_t frame[FRAME_LEN] = {};
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = old_id;
        frame[3] = 0xCD;
        frame[4] = 0x00;
        frame[5] = new_id;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);
        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return std::unexpected("modify_id transmit failed: " + res.error());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Response: FA AF [??] [??] [AA] [??] [??] [??] [chk] [ED]
        // Byte4 = AA 表示成功, Byte3 = 新的实际 ID
        uint8_t resp[FRAME_LEN] = {};
        if (auto res = receive_frame(resp, 100); !res) {
            return std::unexpected("modify_id receive failed: " + res.error());
        }

        if (resp[4] != 0xAA) {
            return std::unexpected("modify_id: servo did not acknowledge ID change");
        }

        return true;
    }

    std::expected<bool, std::string> UbtechServo::set_offset(const uint8_t id, const double offset_deg) {
        // 偏移量折算: 角度 → 整数编码, 范围 -30 ~ +30
        // 具体编码方式参考协议文档, 这里假设直接对应
        const auto raw = static_cast<int16_t>(offset_deg);

        // FA AF [id] [D2] [00] [00] [offset_hi] [offset_lo] [chk] [ED]
        uint8_t frame[FRAME_LEN] = {};
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = id;
        frame[3] = 0xD2;
        frame[4] = 0x00;
        frame[5] = 0x00;
        frame[6] = static_cast<uint8_t>((raw >> 8) & 0xFF);
        frame[7] = static_cast<uint8_t>(raw & 0xFF);
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);
        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return std::unexpected("set_offset transmit failed: " + res.error());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        if (auto res = receive_frame(resp, 100); !res) {
            return std::unexpected("set_offset receive failed: " + res.error());
        }

        if (resp[4] != 0xAA) {
            return std::unexpected("set_offset: servo did not acknowledge offset change");
        }

        return true;
    }

    std::expected<bool, std::string> UbtechServo::read_offset(const uint8_t id, double& offset_deg) {
        // FA AF [id] [D4] [00] [00] [00] [00] [chk] [ED]
        uint8_t frame[FRAME_LEN] = {};
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = id;
        frame[3] = 0xD4;
        frame[4] = 0x00;
        frame[5] = 0x00;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);
        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return std::unexpected("read_offset transmit failed: " + res.error());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        if (auto res = receive_frame(resp, 100); !res) {
            return std::unexpected("read_offset receive failed: " + res.error());
        }

        const auto raw = static_cast<int16_t>((resp[6] << 8) | resp[7]);
        offset_deg = static_cast<double>(raw);
        return true;
    }

    std::expected<bool, std::string> UbtechServo::read_firmware_version(const uint8_t id, std::string& version) {
        // FA AF [id] [01] [00] [00] [00] [00] [chk] [ED]
        uint8_t frame[FRAME_LEN] = {};
        frame[0] = FRAME_HEADER_0;
        frame[1] = FRAME_HEADER_1;
        frame[2] = id;
        frame[3] = 0x01;
        frame[4] = 0x00;
        frame[5] = 0x00;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);
        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return std::unexpected("read_firmware_version transmit failed: " + res.error());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        if (auto res = receive_frame(resp, 100); !res) {
            return std::unexpected("read_firmware_version receive failed: " + res.error());
        }

        // Byte4~Byte7 = 版本号 1~4
        version = std::to_string(resp[4]) + "." +
            std::to_string(resp[5]) + "." +
            std::to_string(resp[6]) + "." +
            std::to_string(resp[7]);
        return true;
    }

    std::expected<bool, std::string> UbtechServo::enter_bootloader(const uint8_t id) {
        // FC CF [id] [02] [00] [00] [00] [00] [chk] [ED]
        // 注意: 这里的帧头不是 FA AF, 而是 FC CF
        LOG_WARN("Enter servo {} bootloader", id);
        uint8_t frame[FRAME_LEN] = {};
        frame[0] = 0xFC;
        frame[1] = 0xCF;
        frame[2] = id;
        frame[3] = 0x02;
        frame[4] = 0x00;
        frame[5] = 0x00;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = checksum(&frame[2], 6);
        frame[9] = FRAME_TAIL;

        std::lock_guard<std::mutex> lock(bus_mutex_);
        if (auto res = transmit_frame(frame, FRAME_LEN); !res) {
            return res;
        }

        // 舵机回应后立刻跳转到 bootloader
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        return receive_frame(resp, 100); // 即使读不到也不算失败
    }


} // namespace ubtech_servo_hardware