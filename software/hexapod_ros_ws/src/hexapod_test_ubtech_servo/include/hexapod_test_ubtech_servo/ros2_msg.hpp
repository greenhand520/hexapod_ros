//
// Created by greenhand520 on 2026/5/4.
//

#pragma once

#include <hexapod_test_ubtech_servo_interface/msg/set_angle.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/modify_id.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/read_angle.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/read_firmware.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/read_offset.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/set_angle.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/set_offset.hpp>
#include <hexapod_test_ubtech_servo_interface/srv/stop_servo.hpp>

#include "hexapod_test_ubtech_servo_interface/srv/detail/set_angles__struct.hpp"

namespace ubtech_servo_hardware {

    using SetAngle = hexapod_test_ubtech_servo_interface::msg::SetAngle;
    using ModifyIdSrv = hexapod_test_ubtech_servo_interface::srv::ModifyId;
    using ReadAngleSrv = hexapod_test_ubtech_servo_interface::srv::ReadAngle;
    using ReadFirmwareSrv = hexapod_test_ubtech_servo_interface::srv::ReadFirmware;
    using ReadOffsetSrv = hexapod_test_ubtech_servo_interface::srv::ReadOffset;
    using SetAngleSrv = hexapod_test_ubtech_servo_interface::srv::SetAngle;
    using SetAnglesSrv = hexapod_test_ubtech_servo_interface::srv::SetAngles;
    using SetOffsetSrv = hexapod_test_ubtech_servo_interface::srv::SetOffset;
    using StopServoSrv = hexapod_test_ubtech_servo_interface::srv::StopServo;

    const std::string SRV_MODIFY_ID = "/hexapod_ros/ubtech/test/modify_id";
    const std::string SRV_READ_ANGLE = "/hexapod_ros/ubtech/test/read_angle";
    const std::string SRV_READ_FIRMWARE = "/hexapod_ros/ubtech/test/read_firmware";
    const std::string SRV_READ_OFFSET = "/hexapod_ros/ubtech/test/read_offset";
    const std::string SRV_SET_ANGLE = "/hexapod_ros/ubtech/test/set_angle";
    const std::string SRV_SET_ANGLES = "/hexapod_ros/ubtech/test/set_angles";
    const std::string SRV_SET_OFFSET = "/hexapod_ros/ubtech/test/set_offset";
    const std::string SRV_STOP_SERVO = "/hexapod_ros/ubtech/test/stop_servo";

}

