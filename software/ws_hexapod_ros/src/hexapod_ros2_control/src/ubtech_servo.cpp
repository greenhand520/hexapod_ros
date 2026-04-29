//
// Created by eartholnpc on 2026/4/25.
//

#pragma once
// src/ubtech_servo.cpp
#include "hexapod_ros2_control/ubtech_servo.hpp"

#include <algorithm>
#include <cstring>

namespace hexapod_ros2_control {

// ── 校验和 ────────────────────────────────────────────────────
uint8_t UbtechServo::calc_checksum(uint8_t id, uint8_t cmd,
                                    uint8_t p1, uint8_t p2,
                                    uint8_t p3, uint8_t p4) {
  return static_cast<uint8_t>(
      static_cast<uint16_t>(id) + cmd + p1 + p2 + p3 + p4);
}

// ── 通用帧构建 ────────────────────────────────────────────────
std::vector<uint8_t> UbtechServo::build_frame(uint8_t id, uint8_t cmd,
                                                uint8_t p1, uint8_t p2,
                                                uint8_t p3, uint8_t p4) {
  std::vector<uint8_t> frame(FRAME_LENGTH);
  frame[0] = FRAME_HEADER_0;
  frame[1] = FRAME_HEADER_1;
  frame[2] = id;
  frame[3] = cmd;
  frame[4] = p1;
  frame[5] = p2;
  frame[6] = p3;
  frame[7] = p4;
  frame[8] = calc_checksum(id, cmd, p1, p2, p3, p4);
  frame[9] = FRAME_TAIL;
  return frame;
}

// ── 转动到指定角度 ────────────────────────────────────────────
// Byte3=0x01, Byte4=角度(0-240), Byte5=运动时间,
// Byte6=锁定时间高字节, Byte7=锁定时间低字节
std::vector<uint8_t> UbtechServo::build_set_angle_cmd(
    uint8_t id, double angle_deg, uint16_t move_time, uint16_t lock_time) {
  // 钳位到有效范围
  angle_deg = std::clamp(angle_deg, MIN_ANGLE_DEG, MAX_ANGLE_DEG);
  auto angle = static_cast<uint8_t>(angle_deg);
  auto time_val = static_cast<uint8_t>(std::min<uint16_t>(move_time, 255));
  auto lock_hi = static_cast<uint8_t>((lock_time >> 8) & 0xFF);
  auto lock_lo = static_cast<uint8_t>(lock_time & 0xFF);
  return build_frame(id, CMD_SET_ANGLE, angle, time_val, lock_hi, lock_lo);
}

// ── 角度回读 ──────────────────────────────────────────────────
// Byte3=0x02, Byte4-Byte7=0x00
std::vector<uint8_t> UbtechServo::build_read_angle_cmd(uint8_t id) {
  return build_frame(id, CMD_READ_ANGLE, 0x00, 0x00, 0x00, 0x00);
}

// ── 强制中止转动 ──────────────────────────────────────────────
// Byte3=0x01, Byte4=0xFF, Byte5-Byte7=0x00
// 中止后舵机失电，仅靠齿轮阻尼维持位置
std::vector<uint8_t> UbtechServo::build_stop_cmd(uint8_t id) {
  return build_frame(id, CMD_SET_ANGLE, STOP_ANGLE_FLAG, 0x00, 0x00, 0x00);
}

// ── 帧验证 ────────────────────────────────────────────────────
bool UbtechServo::verify_frame(const uint8_t* data, size_t length) {
  if (length < FRAME_LENGTH) return false;
  if (data[0] != FRAME_HEADER_0 || data[1] != FRAME_HEADER_1) return false;
  if (data[9] != FRAME_TAIL) return false;
  uint8_t expected = calc_checksum(data[2], data[3], data[4],
                                    data[5], data[6], data[7]);
  return data[8] == expected;
}

// ── 解析角度回读响应 ──────────────────────────────────────────
// 响应帧: [FA][AF][ID][??][目标角度Hi][目标角度Lo]
//         [实际角度Hi][实际角度Lo][CHK][ED]
// 两种角度均为正整数(度), 不同表示未到位或存在误差
std::optional<ServoAngleData> UbtechServo::parse_angle_response(
    const uint8_t* data, size_t length) {
  if (!verify_frame(data, length)) return std::nullopt;

  ServoAngleData result;
  result.target_angle_deg =
      static_cast<double>((static_cast<uint16_t>(data[4]) << 8) | data[5]);
  result.actual_angle_deg =
      static_cast<double>((static_cast<uint16_t>(data[6]) << 8) | data[7]);
  return result;
}

// ── 解析 ACK 响应 ─────────────────────────────────────────────
// 成功: 0xAA + 舵机ID (2字节)
// 失败: 不返回任何数据
bool UbtechServo::parse_ack_response(
    const uint8_t* data, size_t length, uint8_t& servo_id) {
  if (length < 2) return false;
  if (data[0] != ACK_BYTE) return false;
  servo_id = data[1];
  return true;
}

// ── 响应帧长度 ────────────────────────────────────────────────
size_t UbtechServo::get_response_length(uint8_t cmd) const {
  switch (cmd) {
    case CMD_READ_ANGLE: return FRAME_LENGTH;  // 10字节完整帧
    case CMD_SET_ANGLE:  return 2;             // ACK: 0xAA + ID
    default:             return 0;
  }
}

}  // namespace hexapod_servo_control