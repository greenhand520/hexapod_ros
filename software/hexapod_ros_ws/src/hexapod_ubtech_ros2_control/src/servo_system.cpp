//
// Created by greenhand520 on 2026/4/25.
//


#include <hardware_interface/types/hardware_interface_type_values.hpp>
#include <pluginlib/class_list_macros.hpp>

#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <string>
#include <vector>

#include "hexapod_ubtech_ros2_control/servo_system.hpp"

#include "common_cpp/log/log.hpp"
#include "common_cpp/log/log_interface/log_macro.hpp"
#include "common_cpp/log/log_interface/log_manager.hpp"
#include "common_cpp/log/rclcpp_log_handler.hpp"
#include "common_cpp/log/spdlog_adapter.hpp"
#include "hexapod_ubtech_ros2_control/ubtech_servo.hpp"

namespace ubtech_servo_hardware {

    hardware_interface::CallbackReturn UbtechServoSystem::on_init(const hardware_interface::HardwareInfo& info) {
        if (SystemInterface::on_init(info) !=
            hardware_interface::CallbackReturn::SUCCESS) {
            return hardware_interface::CallbackReturn::ERROR;
        }

        // Hardware-level parameters
        const auto& params = info_.hardware_parameters;

        common_log::RclcppLogHandler::init_from_map(params);
        const auto logger = common_log::Logger::get_instance().get_logger();
        const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger);
        log_interface::LogManager::set(logger_interface);

        serial_device_ = params.contains("serial_device") ? params.at("serial_device") : "/dev/ttyS4";
        baud_rate_ = params.contains("baud_rate") ? std::stoi(params.at("baud_rate")) : 115200;
        gpio_name_ = params.contains("gpio_name") ? params.at("gpio_name") : "GPIO1_B1";

        // ── Per-joint parameters ──
        joints_.resize(info_.joints.size());

        for (size_t i = 0; i < info_.joints.size(); ++i) {
            const auto& joint_info = info_.joints[i];
            auto& [name, servo_id, angle_offset_deg, pos_state, vel_state, pos_cmd, prev_pos_state] = joints_[i];

            name = joint_info.name;

            // servo_id is mandatory
            if (!joint_info.parameters.contains("servo_id")) {
                LOG_ERROR("Joint '{}': missing required parameter 'servo_id'", name)
                return hardware_interface::CallbackReturn::ERROR;
            }
            servo_id = static_cast<uint8_t>(std::stoi(joint_info.parameters.at("servo_id")));

            pos_state = 0.0;
            vel_state = 0.0;
            pos_cmd = 0.0;
            prev_pos_state = 0.0;

            // Validate interfaces
            bool has_pos_cmd = false;
            for (const auto& ci : joint_info.command_interfaces) {
                if (ci.name == hardware_interface::HW_IF_POSITION)
                    has_pos_cmd = true;
            }
            if (!has_pos_cmd) {
                LOG_ERROR("Joint '{}': needs a 'position' command interface", name);
                return hardware_interface::CallbackReturn::ERROR;
            }
        }
        
        LOG_INFO("on_init: {} joint(s), serial={} @ {} baud, gpio={}",
                    joints_.size(), serial_device_, baud_rate_, gpio_name_);

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    /// @brief on_configure — open serial port, configure GPIO
    hardware_interface::CallbackReturn UbtechServoSystem::on_configure(
        const rclcpp_lifecycle::State& /*prev*/) {
        // Serial
        serial_ = std::make_unique<lubancat_hw::SerialPort>();
        if (!serial_->open(serial_device_, baud_rate_)) {
            LOG_ERROR("Cannot open serial port '%s' at %d baud", serial_device_, baud_rate_);
            return hardware_interface::CallbackReturn::ERROR;
        }

        // GPIO TX_EN
        tx_en_ = std::make_unique<lubancat_hw::GpioPin>();
        if (!tx_en_->open(gpio_name_, "ubtech_servo_tx_en")) {
            LOG_ERROR("Cannot open GPIO %s", gpio_name_);
            return hardware_interface::CallbackReturn::ERROR;
        }

        // Servo controller
        servo_ = std::make_unique<UbtechServo>(serial_.get(), tx_en_.get());

        LOG_INFO("on_configure: success");
        return hardware_interface::CallbackReturn::SUCCESS;
    }


    /// @brief on_activate — read initial positions
    hardware_interface::CallbackReturn UbtechServoSystem::on_activate(
        const rclcpp_lifecycle::State& /*prev*/) {
        for (auto& jd : joints_) {
            ServoAngle angle{};
            if (auto result = servo_->read_angle(jd.servo_id, angle); result.has_value() && result.value()) {
                // actual_rad already accounts for the 120° servo center offset
                const double pos = angle.actual_rad + jd.angle_offset_deg * DEG2RAD;
                jd.pos_state = pos;
                jd.prev_pos_state = pos;
                jd.pos_cmd = pos;

                LOG_INFO("Joint '{}' (ID {}): initial {:.4f} rad",
                            jd.name, jd.servo_id, pos);
            }
            else {
                const std::string err = result.has_value() ? "unknown error" : result.error();
                LOG_WARN("Joint '{}' (ID {}): failed to read initial angle: {}",
                            jd.name, jd.servo_id, err);
            }
        }

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    ///@brief on_deactivate — stop all servos
    hardware_interface::CallbackReturn UbtechServoSystem::on_deactivate(
        const rclcpp_lifecycle::State& /*prev*/) {
        for (const auto& jd : joints_) {
            if (auto result = servo_->stop(jd.servo_id); !result.has_value()) {
                LOG_WARN("Joint '{}' (ID {}): stop failed: {}",
                            jd.name, jd.servo_id, result.error());
            }
            else if (!result.value()) {
                LOG_WARN("Joint '{}' (ID {}): stop returned false",
                            jd.name, jd.servo_id);
            }
        }
        LOG_INFO("on_deactivate: servos stopped");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    /// @brief on_cleanup — release resources
    hardware_interface::CallbackReturn UbtechServoSystem::on_cleanup(
        const rclcpp_lifecycle::State& /*prev*/) {
        servo_.reset();
        tx_en_.reset();
        serial_.reset();
        LOG_INFO("on_cleanup: done");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    std::vector<hardware_interface::StateInterface> UbtechServoSystem::export_state_interfaces() {
        std::vector<hardware_interface::StateInterface> si;
        si.reserve(joints_.size() * 2);

        for (auto& jd : joints_) {
            si.emplace_back(jd.name, hardware_interface::HW_IF_POSITION, &jd.pos_state);
        }
        return si;
    }

    std::vector<hardware_interface::CommandInterface> UbtechServoSystem::export_command_interfaces() {
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

    hardware_interface::return_type UbtechServoSystem::read(const rclcpp::Time& /*time*/,
                                                            const rclcpp::Duration& period) {
        for (auto& jd : joints_) {
            ServoAngle angle{};
            if (auto result = servo_->read_angle(jd.servo_id, angle)) {
                jd.prev_pos_state = jd.pos_state;
                // actual_rad already accounts for the 120° servo center offset
                jd.pos_state = angle.actual_rad + jd.angle_offset_deg * DEG2RAD;

                if (const double dt = period.seconds(); dt > 0.0) {
                    jd.vel_state = (jd.pos_state - jd.prev_pos_state) / dt;
                }
            } else {
                LOG_ERROR("{}", result.error());
            }
            // If read fails, keep last known state
        }

        return hardware_interface::return_type::OK;
    }

    hardware_interface::return_type UbtechServoSystem::write(const rclcpp::Time& /*time*/,
                                                             const rclcpp::Duration& /*period*/) {
        std::vector<AngleCommand> commands;
        commands.reserve(joints_.size());

        for (const auto& jd : joints_) {
            // pos_cmd is in radians; subtract per-joint calibration offset
            // UbtechServo::set_angles handles the 120° center offset internally
            const double cmd_rad = jd.pos_cmd - jd.angle_offset_deg * DEG2RAD;

            AngleCommand cmd;
            cmd.id = jd.servo_id;
            cmd.angle_rad = cmd_rad;
            commands.push_back(cmd);
        }

        if (!commands.empty()) {
            if (auto result = servo_->set_angles(commands); !result) {
                LOG_ERROR("{}", result.error());
            }
        }

        return hardware_interface::return_type::OK;
    }

} // namespace ubtech_servo_hardware

PLUGINLIB_EXPORT_CLASS(
    ubtech_servo_hardware::UbtechServoSystem,
    hardware_interface::SystemInterface)
