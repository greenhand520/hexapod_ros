//
// Created by greenhand520 on 2026/4/25.
//

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hexapod_ros2_control {

    /// @brief 舵机角度读取返回数据
    struct ServoAngleData {
        double target_angle_deg; // 目标角度(度)
        double actual_angle_deg; // 实际角度(度)
    };

    /// @brief 舵机通信抽象基类
    ///
    /// 所有舵机协议实现均继承此类。ServoSystem 通过该接口与具体舵机通信，
    /// 更换舵机时只需新增子类并修改配置，无需改动 ServoSystem 代码。
    class ServoBase {
    public:
        using UniquePtr = std::unique_ptr<ServoBase>;
        using SharedPtr = std::shared_ptr<ServoBase>;

        virtual ~ServoBase() = default;

        // ── 协议元信息 ──────────────────────────────────────────────
        /// 协议名称，用于配置匹配（如 "ubtech"）
        virtual std::string get_name() const = 0;

        /// 舵机最大有效角度(度)
        virtual double get_max_angle_deg() const = 0;

        /// 舵机最小有效角度(度)
        virtual double get_min_angle_deg() const = 0;

        // ── 指令构建 ────────────────────────────────────────────────
        /// 构建"转动到指定角度"指令帧
        /// @param id        舵机ID (1-240, 0为广播)
        /// @param angle_deg 目标角度(度)，超出范围会被钳位
        /// @param move_time 运动时间, 0=全速
        /// @param lock_time 锁定时间(ms), 锁定期间不再响应转动指令
        virtual std::vector<uint8_t> build_set_angle_cmd(
            uint8_t id, double angle_deg,
            uint16_t move_time = 0, uint16_t lock_time = 0) = 0;

        /// 构建"角度回读"指令帧
        virtual std::vector<uint8_t> build_read_angle_cmd(uint8_t id) = 0;

        /// 构建"强制中止转动"指令帧
        virtual std::vector<uint8_t> build_stop_cmd(uint8_t id) = 0;

        // ── 响应解析 ────────────────────────────────────────────────
        /// 解析角度回读响应帧
        /// @return 解析成功返回角度数据，失败返回 nullopt
        virtual std::optional<ServoAngleData> parse_angle_response(
            const uint8_t* data, size_t length) = 0;

        /// 解析单字节 ACK 响应（设置角度等简单指令的应答）
        /// @param[out] servo_id 返回应答的舵机ID
        /// @return true=成功应答
        virtual bool parse_ack_response(
            const uint8_t* data, size_t length, uint8_t& servo_id) = 0;

        /// 获取指定指令对应的响应帧长度(字节)
        virtual size_t get_response_length(uint8_t cmd) const = 0;
    };

} // namespace hexapod_servo_control