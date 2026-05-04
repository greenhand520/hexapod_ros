//
// Created by greenhand520 on 2026/4/25.
//


#include <hardware_interface/types/hardware_interface_type_values.hpp>
#include <pluginlib/class_list_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <vector>

#include "hexapod_ubtech_ros2_control/servo_system.hpp"
#include "hexapod_ubtech_ros2_control/ubtech_servo.hpp"

namespace ubtech_servo_hardware {

    // ═══════════════════════════════════════════════════════════════════════
    //  on_init — parse URDF hardware parameters & joint definitions
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::CallbackReturn UbtechServoSystem::on_init(
        const hardware_interface::HardwareInfo& info) {
        if (hardware_interface::SystemInterface::on_init(info) !=
            hardware_interface::CallbackReturn::SUCCESS) {
            return hardware_interface::CallbackReturn::ERROR;
        }

        // ── Hardware-level parameters ──────────────────────────────────────
        const auto& params = info_.hardware_parameters;

        serial_device_ = params.count("serial_device") ? params.at("serial_device") : "/dev/ttyUSB0";
        baud_rate_ = params.count("baud_rate") ? std::stoi(params.at("baud_rate")) : 115200;
        gpio_chip_ = params.count("gpio_chip") ? params.at("gpio_chip") : "/dev/gpiochip0";
        tx_en_line_ = params.count("tx_en_line") ? static_cast<unsigned int>(std::stoi(params.at("tx_en_line"))) : 25;
        angle_min_deg_ = params.count("angle_min_deg") ? std::stod(params.at("angle_min_deg")) : 0.0;

        // ── Per-joint parameters ───────────────────────────────────────────
        joints_.resize(info_.joints.size());

        for (size_t i = 0; i < info_.joints.size(); ++i) {
            const auto& joint_info = info_.joints[i];
            auto& jd = joints_[i];

            jd.name = joint_info.name;

            // servo_id is mandatory
            if (!joint_info.parameters.count("servo_id")) {
                RCLCPP_ERROR(rclcpp::get_logger("UbtechServoSystem"),
                             "Joint '%s': missing required parameter 'servo_id'", jd.name.c_str());
                return hardware_interface::CallbackReturn::ERROR;
            }
            jd.servo_id = static_cast<uint8_t>(std::stoi(joint_info.parameters.at("servo_id")));

            // angle_offset_deg is optional
            jd.angle_offset_deg = joint_info.parameters.count("angle_offset_deg")
                ? std::stod(joint_info.parameters.at("angle_offset_deg"))
                : 0.0;

            jd.pos_state = 0.0;
            jd.vel_state = 0.0;
            jd.pos_cmd = 0.0;
            jd.prev_pos_state = 0.0;

            // Validate interfaces
            bool has_pos_cmd = false;
            for (const auto& ci : joint_info.command_interfaces) {
                if (ci.name == hardware_interface::HW_IF_POSITION)
                    has_pos_cmd = true;
            }
            if (!has_pos_cmd) {
                RCLCPP_ERROR(rclcpp::get_logger("UbtechServoSystem"),
                             "Joint '%s': needs a 'position' command interface", jd.name.c_str());
                return hardware_interface::CallbackReturn::ERROR;
            }
        }

        RCLCPP_INFO(rclcpp::get_logger("UbtechServoSystem"),
                    "on_init: %zu joint(s), serial=%s @ %d baud, gpio_chip=%s tx_en_line=%u",
                    joints_.size(), serial_device_.c_str(), baud_rate_,
                    gpio_chip_.c_str(), tx_en_line_);

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  on_configure — open serial port, configure GPIO
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::CallbackReturn UbtechServoSystem::on_configure(
        const rclcpp_lifecycle::State& /*prev*/) {
        // Serial
        serial_ = std::make_unique<SerialPort>();
        if (!serial_->open(serial_device_, baud_rate_)) {
            RCLCPP_ERROR(rclcpp::get_logger("UbtechServoSystem"),
                         "Cannot open serial port '%s' at %d baud", serial_device_.c_str(), baud_rate_);
            return hardware_interface::CallbackReturn::ERROR;
        }

        // GPIO TX_EN
        tx_en_ = std::make_unique<GpioPin>();
        if (!tx_en_->open(gpio_chip_, tx_en_line_, "ubtech_servo_tx_en", 0)) {
            RCLCPP_ERROR(rclcpp::get_logger("UbtechServoSystem"),
                         "Cannot open GPIO %s line %u", gpio_chip_.c_str(), tx_en_line_);
            return hardware_interface::CallbackReturn::ERROR;
        }

        // Servo controller
        servo_ = std::make_unique<UbtechServo>(serial_.get(), tx_en_.get());

        RCLCPP_INFO(rclcpp::get_logger("UbtechServoSystem"), "on_configure: success");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  on_activate — read initial positions
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::CallbackReturn UbtechServoSystem::on_activate(
        const rclcpp_lifecycle::State& /*prev*/) {
        for (auto& jd : joints_) {
            ServoAngle sa;
            if (servo_->read_angle(jd.servo_id, sa)) {
                double pos = (sa.actual_deg - angle_min_deg_ + jd.angle_offset_deg) * DEG2RAD;
                jd.pos_state = pos;
                jd.prev_pos_state = pos;
                jd.pos_cmd = pos;

                RCLCPP_INFO(rclcpp::get_logger("UbtechServoSystem"),
                            "Joint '%s' (ID %u): initial %.1f° → %.4f rad",
                            jd.name.c_str(), jd.servo_id, sa.actual_deg, pos);
            }
            else {
                RCLCPP_WARN(rclcpp::get_logger("UbtechServoSystem"),
                            "Joint '%s' (ID %u): failed to read initial angle",
                            jd.name.c_str(), jd.servo_id);
            }
        }

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  on_deactivate — stop all servos
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::CallbackReturn UbtechServoSystem::on_deactivate(
        const rclcpp_lifecycle::State& /*prev*/) {
        for (const auto& jd : joints_) {
            servo_->stop(jd.servo_id);
        }
        RCLCPP_INFO(rclcpp::get_logger("UbtechServoSystem"), "on_deactivate: servos stopped");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  on_cleanup — release resources
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::CallbackReturn UbtechServoSystem::on_cleanup(
        const rclcpp_lifecycle::State& /*prev*/) {
        servo_.reset();
        tx_en_.reset();
        serial_.reset();
        RCLCPP_INFO(rclcpp::get_logger("UbtechServoSystem"), "on_cleanup: done");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  export interfaces
    // ═══════════════════════════════════════════════════════════════════════

    std::vector<hardware_interface::StateInterface>
    UbtechServoSystem::export_state_interfaces() {
        std::vector<hardware_interface::StateInterface> si;
        si.reserve(joints_.size() * 2);

        for (auto& jd : joints_) {
            si.emplace_back(jd.name, hardware_interface::HW_IF_POSITION, &jd.pos_state);
            si.emplace_back(jd.name, hardware_interface::HW_IF_VELOCITY, &jd.vel_state);
        }
        return si;
    }

    std::vector<hardware_interface::CommandInterface>
    UbtechServoSystem::export_command_interfaces() {
        std::vector<hardware_interface::CommandInterface> ci;
        ci.reserve(joints_.size());

        for (auto& jd : joints_) {
            ci.emplace_back(jd.name, hardware_interface::HW_IF_POSITION, &jd.pos_cmd);
        }
        return ci;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  read — query all servo angles
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::return_type UbtechServoSystem::read(
        const rclcpp::Time& /*time*/, const rclcpp::Duration& period) {
        for (auto& jd : joints_) {
            ServoAngle sa;
            if (servo_->read_angle(jd.servo_id, sa)) {
                jd.prev_pos_state = jd.pos_state;
                jd.pos_state = (sa.actual_deg - angle_min_deg_ + jd.angle_offset_deg) * DEG2RAD;

                double dt = period.seconds();
                if (dt > 0.0) {
                    jd.vel_state = (jd.pos_state - jd.prev_pos_state) / dt;
                }
            }
            // If read fails, keep last known state
        }

        return hardware_interface::return_type::OK;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  write — send target angles to all servos
    // ═══════════════════════════════════════════════════════════════════════

    hardware_interface::return_type UbtechServoSystem::write(
        const rclcpp::Time& /*time*/, const rclcpp::Duration& /*period*/) {
        for (const auto& jd : joints_) {
            // rad → deg, apply calibration
            double deg = jd.pos_cmd / DEG2RAD + angle_min_deg_ - jd.angle_offset_deg;

            // Clamp to [0, MAX_SERVO_ANGLE]
            deg = std::clamp(deg, 0.0, static_cast<double>(MAX_SERVO_ANGLE));

            auto angle_u8 = static_cast<uint8_t>(std::round(deg));
            servo_->set_angle(jd.servo_id, angle_u8);
        }

        return hardware_interface::return_type::OK;
    }

} // namespace ubtech_servo_hardware

PLUGINLIB_EXPORT_CLASS(
    ubtech_servo_hardware::UbtechServoSystem,
    hardware_interface::SystemInterface)
