//
// Created by greenhand520 on 2026/5/3.
//

// Protocol reference: 某宝白菜价总线舵机研究记
// Frame format: FA AF [ID] [CMD] [P1] [P2] [P3] [P4] [CHECKSUM] [ED]
// Checksum: sum of Byte2..Byte7, low byte.

#pragma once

#include <expected>
#include <mutex>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "gpio_pin.hpp"
#include "serial_port.hpp"

namespace ubtech_servo_hardware {

    // ── Protocol constants ──
    constexpr uint8_t FRAME_HEADER_0 = 0xFA;
    constexpr uint8_t FRAME_HEADER_1 = 0xAF;
    constexpr uint8_t FRAME_TAIL = 0xED;
    constexpr size_t FRAME_LEN = 10;

    constexpr uint8_t CMD_ROTATE = 0x01; // 转动到指定角度
    constexpr uint8_t CMD_READ_ANGLE = 0x02; // 角度回读
    constexpr uint8_t CMD_STOP_MARKER = 0xFF; // 强制中止标记 (P1 字段)

    constexpr uint8_t MAX_SERVO_ANGLE = 240; // 最大有效角度 (度)

    // ── Result of an angle-read operation ──
    struct ServoAngle {
        double target_deg; // 目标角度 (度)
        double actual_deg; // 实际角度 (度)
    };

    // ── 批量控制命令参数封装 ──
    struct AngleCommand {
        uint8_t id = 255; // 舵机 ID (1–255)
        uint8_t angle_deg = 0; // 目标角度 (0–240 度)
        uint8_t motion_time = 0; // 运动时间 (0 = 全速转动)
        uint16_t lock_time_ms = 0; // 锁定时间 (ms)
    };

    // ── UbtechServo ──────────────────────────────────────────────────────
    class UbtechServo {
    public:
        /// @param serial  Pointer to an opened SerialPort (caller retains ownership)
        /// @param tx_en   Pointer to a configured GpioPin for TX_EN (caller retains ownership)
        UbtechServo(SerialPort* serial, GpioPin* tx_en);
        ~UbtechServo() = default;

        UbtechServo(const UbtechServo&) = delete;
        UbtechServo& operator=(const UbtechServo&) = delete;

        /// @brief 转动到指定角度
        /// @param id            舵机 ID (1–240)
        /// @param angle_deg     目标角度 (0–240 度, 超过 240 自动截断)
        /// @param motion_time   运动时间 (0 = 全速转动)
        /// @param lock_time_ms  锁定时间 (ms), 锁定期间不再响应新的转动命令
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> set_angle(uint8_t id, uint8_t angle_deg,
                                                   uint8_t motion_time = 0, uint16_t lock_time_ms = 0);

        /// @brief 批量设置多个舵机角度 (所有帧拼接后一次性通过串口发送)
        /// @param commands  多个舵机的控制参数
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> set_angles(const std::vector<AngleCommand>& commands);

        /// @brief 角度回读
        /// @param id     舵机 ID
        /// @param angle  输出: 目标角度和实际角度
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> read_angle(uint8_t id, ServoAngle& angle);

        /// @brief 强制中止转动 (舵机立即停止并失电, 仅靠齿轮阻尼维持位置)
        /// @param id 舵机 ID (0 = 广播, 对所有舵机有效)
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> stop(uint8_t id);

        /// @brief 修改舵机 ID (协议: Byte3 = 0xCD, Byte5 = 新ID)
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> modify_id(uint8_t old_id, uint8_t new_id);

        /// @brief 设置角度偏移量 (协议: Byte3 = 0xD2)
        /// @param id servo id
        /// @param offset_deg 偏移角度, -30 ~ +30, 正值顺时针
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> set_offset(uint8_t id, double offset_deg);

        /// @brief 读取角度偏移量设置 (协议: Byte3 = 0xD4)
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> read_offset(uint8_t id, double& offset_deg);

        /// @brief 读取固件版本号 (协议: Byte3 = 0x01)
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> read_firmware_version(uint8_t id, std::string& version);

        /// @brief 进入 bootloader (协议: Byte0=FC, Byte1=CF, Byte3=0x02)
        /// @return 成功返回 true, 失败返回 std::unexpected("错误原因")
        std::expected<bool, std::string> enter_bootloader(uint8_t id);


    private:
        SerialPort* serial_;
        GpioPin* tx_en_;
        std::mutex bus_mutex_; // 保护总线访问

        // TX_EN 控制
        [[nodiscard]] bool enable_tx() const;
        [[nodiscard]] bool disable_tx() const;

        // 校验和: Byte2~Byte7 累加和, 取低字节
        static uint8_t checksum(const uint8_t* data, size_t count);

        // 组装完整帧并写入串口, 返回实际写入字节数
        std::expected<bool, std::string> transmit_frame(const uint8_t* frame, size_t len) const;

        // 读取一帧响应 (自动寻找 FA AF 头, 验证校验和与帧尾)
        // @param out      输出缓冲区 (至少 FRAME_LEN 字节)
        // @param timeout  超时 (ms)
        // @return true 表示收到有效帧
        std::expected<bool, std::string> receive_frame(uint8_t* out, int timeout_ms = 100) const;
    };

} // namespace ubtech_servo_hardware

// 为 AngleCommand 特化 fmt::formatter，支持容器打印
template <>
struct fmt::formatter<ubtech_servo_hardware::AngleCommand> : fmt::formatter<std::string> {
    template <typename FormatContext>
    auto format(const ubtech_servo_hardware::AngleCommand& cmd, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(),
                              "AngleCommand(id={}, angle={}, time={}, lock={}ms)",
                              cmd.id, cmd.angle_deg, cmd.motion_time, cmd.lock_time_ms);
    }
};
