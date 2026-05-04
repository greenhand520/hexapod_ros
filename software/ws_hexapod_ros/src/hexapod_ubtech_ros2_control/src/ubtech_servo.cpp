//
// Created by greenhand520 on 2026/5/3.
//

#include "hexapod_ubtech_ros2_control/ubtech_servo.hpp"

#include <chrono>
#include <cstring>
#include <thread>

namespace ubtech_servo_hardware {

    UbtechServo::UbtechServo(SerialPort* serial, GpioPin* tx_en) : serial_(serial), tx_en_(tx_en) {
    }

    void UbtechServo::enable_tx() const {
        if (tx_en_ && tx_en_->is_open()) {
            tx_en_->set_value(GPIOValue::HIGH);
        }
    }

    void UbtechServo::disable_tx() const {
        if (tx_en_ && tx_en_->is_open()) {
            tx_en_->set_value(GPIOValue::LOW);
        }
    }


    uint8_t UbtechServo::checksum(const uint8_t* data, const size_t count) {
        uint8_t sum = 0;
        for (size_t i = 0; i < count; ++i) {
            sum = static_cast<uint8_t>(sum + data[i]);
        }
        return sum;
    }


    bool UbtechServo::transmit_frame(const uint8_t* frame, const size_t len) const {
        if (!serial_ || !serial_->is_open())
            return false;

        enable_tx();
        serial_->flush();

        const int written = serial_->write_data(frame, len);

        // write_data already calls tcdrain(), add a small margin for the
        // transceiver to finish switching.
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        disable_tx();

        return written == static_cast<int>(len);
    }


    bool UbtechServo::receive_frame(uint8_t* out, const int timeout_ms) const {
        // Read byte-by-byte, synchronise on header FA AF, then collect
        // remaining bytes to form a complete 10-byte frame.
        constexpr int per_byte_timeout = 10;
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms);

        auto read_one = [&](uint8_t& byte) -> bool {
            while (std::chrono::steady_clock::now() < deadline) {
                const int n = serial_->read_data(&byte, 1, per_byte_timeout);
                if (n == 1)
                    return true;
                if (n < 0)
                    return false;
            }
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
                    return false;
            }

            // Validate tail
            if (out[9] != FRAME_TAIL)
                return false;

            // Validate checksum (Byte2..Byte7 → Byte8)
            if (const uint8_t expected = checksum(&out[2], 6); out[8] != expected)
                return false;

            return true;
        }

        return false;
    }

    // ── Public API ────────────────────────────────────────────────────────

    bool UbtechServo::set_angle(const uint8_t id, uint8_t angle_deg,
                                const uint8_t motion_time, const uint16_t lock_time_ms) {
        if (id == 0 || id > 240)
            return false;
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

        if (!transmit_frame(frame, FRAME_LEN)) {
            return false;
        }

        // Response: 成功回复 0xAA + 舵机ID (2 bytes); 失败不返回数据
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[2] = {};
        const int n = serial_->read_data(resp, sizeof(resp), 50);

        return (n >= 1 && resp[0] == 0xAA);
    }

    bool UbtechServo::read_angle(const uint8_t id, ServoAngle& angle) {
        if (id == 0 || id > 240)
            return false;

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

        if (!transmit_frame(frame, FRAME_LEN)) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Response: FA AF [??] [??] [target_hi] [target_lo] [actual_hi] [actual_lo] [chk] [ED]
        uint8_t resp[FRAME_LEN] = {};
        if (!receive_frame(resp, 100)) {
            return false;
        }

        const uint16_t target_raw = (static_cast<uint16_t>(resp[4]) << 8) | resp[5];
        const uint16_t actual_raw = (static_cast<uint16_t>(resp[6]) << 8) | resp[7];

        angle.target_deg = static_cast<double>(target_raw);
        angle.actual_deg = static_cast<double>(actual_raw);

        return true;
    }

    bool UbtechServo::stop(const uint8_t id) {
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
        return transmit_frame(frame, FRAME_LEN);
    }

    // ubtech_servo.cpp 中添加:

    bool UbtechServo::modify_id(uint8_t old_id, uint8_t new_id) {
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
        if (!transmit_frame(frame, FRAME_LEN))
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Response: FA AF [??] [??] [AA] [??] [??] [??] [chk] [ED]
        // Byte4 = AA 表示成功, Byte3 = 新的实际 ID
        uint8_t resp[FRAME_LEN] = {};
        if (!receive_frame(resp, 100))
            return false;
        return resp[4] == 0xAA;
    }

    bool UbtechServo::set_offset(uint8_t id, double offset_deg) {
        // 偏移量折算: 角度 → 整数编码, 范围 -30 ~ +30
        // 具体编码方式参考协议文档, 这里假设直接对应
        int16_t raw = static_cast<int16_t>(offset_deg);

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
        if (!transmit_frame(frame, FRAME_LEN))
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        if (!receive_frame(resp, 100))
            return false;
        return resp[4] == 0xAA;
    }

    bool UbtechServo::read_offset(uint8_t id, double& offset_deg) {
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
        if (!transmit_frame(frame, FRAME_LEN))
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        if (!receive_frame(resp, 100))
            return false;

        int16_t raw = static_cast<int16_t>((resp[6] << 8) | resp[7]);
        offset_deg = static_cast<double>(raw);
        return true;
    }

    bool UbtechServo::read_firmware_version(uint8_t id, std::string& version) {
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
        if (!transmit_frame(frame, FRAME_LEN))
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        if (!receive_frame(resp, 100))
            return false;

        // Byte4~Byte7 = 版本号 1~4
        version = std::to_string(resp[4]) + "." +
            std::to_string(resp[5]) + "." +
            std::to_string(resp[6]) + "." +
            std::to_string(resp[7]);
        return true;
    }

    bool UbtechServo::enter_bootloader(uint8_t id) {
        // FC CF [id] [02] [00] [00] [00] [00] [chk] [ED]
        // 注意: 这里的帧头不是 FA AF, 而是 FC CF
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
        if (!transmit_frame(frame, FRAME_LEN))
            return false;

        // 舵机回应后立刻跳转到 bootloader
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint8_t resp[FRAME_LEN] = {};
        receive_frame(resp, 100); // 即使读不到也不算失败

        return true;
    }


} // namespace ubtech_servo_hardware
