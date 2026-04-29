//
// Created by eartholnpc on 2026/4/25.
//

#pragma once

#include "servo_base.hpp"

namespace hexapod_ros2_control {

/// @brief UBTECH 总线舵机协议实现
///
/// 帧格式 (10字节):
///   [FA] [AF] [ID] [CMD] [P1] [P2] [P3] [P4] [CHK] [ED]
///   CHK = (ID + CMD + P1 + P2 + P3 + P4) & 0xFF
///
/// 支持指令:
///   0x01 + angle(0-240) : 转动到指定角度
///   0x01 + 0xFF         : 强制中止转动
///   0x02 + 0x00         : 角度回读
class UbtechServo : public ServoBase {
public:
  // ── 协议常量 ──────────────────────────────────────────────
  static constexpr uint8_t  FRAME_HEADER_0    = 0xFA;
  static constexpr uint8_t  FRAME_HEADER_1    = 0xAF;
  static constexpr uint8_t  FRAME_TAIL        = 0xED;
  static constexpr uint8_t  CMD_SET_ANGLE     = 0x01;
  static constexpr uint8_t  CMD_READ_ANGLE    = 0x02;
  static constexpr uint8_t  STOP_ANGLE_FLAG   = 0xFF;
  static constexpr double   MAX_ANGLE_DEG     = 240.0;
  static constexpr double   MIN_ANGLE_DEG     = 0.0;
  static constexpr size_t   FRAME_LENGTH      = 10;
  static constexpr uint8_t  ACK_BYTE          = 0xAA;

  // ── ServoBase 接口实现 ────────────────────────────────────
  std::string get_name() const override { return "ubtech"; }
  double get_max_angle_deg() const override { return MAX_ANGLE_DEG; }
  double get_min_angle_deg() const override { return MIN_ANGLE_DEG; }

  std::vector<uint8_t> build_set_angle_cmd(
      uint8_t id, double angle_deg,
      uint16_t move_time = 0, uint16_t lock_time = 0) override;

  std::vector<uint8_t> build_read_angle_cmd(uint8_t id) override;

  std::vector<uint8_t> build_stop_cmd(uint8_t id) override;

  std::optional<ServoAngleData> parse_angle_response(
      const uint8_t* data, size_t length) override;

  bool parse_ack_response(
      const uint8_t* data, size_t length, uint8_t& servo_id) override;

  size_t get_response_length(uint8_t cmd) const override;

private:
  /// 通用帧构建
  std::vector<uint8_t> build_frame(uint8_t id, uint8_t cmd,
                                    uint8_t p1, uint8_t p2,
                                    uint8_t p3, uint8_t p4);

  /// 校验和计算: (ID + CMD + P1 + P2 + P3 + P4) & 0xFF
  static uint8_t calc_checksum(uint8_t id, uint8_t cmd,
                                uint8_t p1, uint8_t p2,
                                uint8_t p3, uint8_t p4);

  /// 验证帧头帧尾及校验和
  bool verify_frame(const uint8_t* data, size_t length);
};

}  // namespace hexapod_servo_control