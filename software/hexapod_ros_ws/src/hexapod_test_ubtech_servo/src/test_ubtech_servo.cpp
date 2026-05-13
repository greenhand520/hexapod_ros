//
// Created by greenhand520 on 2026/5/4.
//

#include <rclcpp/rclcpp.hpp>

#include <memory>
#include <string>
#include <ranges>

#include <common_log_cpp/log_interface/log_macro.hpp>
#include "hexapod_test_ubtech_servo/test_ubtech_servo.hpp"


using namespace ubtech_servo_hardware;

namespace ubtech_servo_hardware {

    TestUbtechServo::TestUbtechServo(const rclcpp::Node::SharedPtr& node) : node_(node),
                                                                            logger_(node->get_logger()) {

        node_->declare_parameter<std::string>("serial_device", "/dev/ttyS4");
        node_->declare_parameter<int>("baud_rate", 115200);
        node_->declare_parameter<std::string>("gpio_name", "GPIO1_B1");

        if (!init_hardware()) {
            RCLCPP_ERROR(logger_, "Hardware initialization failed, services will not be available");
            return;
        }

        using namespace std::placeholders;

        srv_modify_id_ = node_->create_service<ModifyIdSrv>(
            SRV_MODIFY_ID,
            [this](const std::shared_ptr<ModifyIdSrv::Request>& req,
                   const std::shared_ptr<ModifyIdSrv::Response>& res) {
                execute_modify_id(req, res);
            });

        srv_read_angle_ = node_->create_service<ReadAngleSrv>(
            SRV_READ_ANGLE,
            [this](const std::shared_ptr<ReadAngleSrv::Request>& req,
                   const std::shared_ptr<ReadAngleSrv::Response>& res) {
                execute_read_angle(req, res);
            });

        srv_read_firmware_ = node_->create_service<ReadFirmwareSrv>(
            SRV_READ_FIRMWARE,
            [this](const std::shared_ptr<ReadFirmwareSrv::Request>& req,
                   const std::shared_ptr<ReadFirmwareSrv::Response>& res) {
                execute_read_firmware(req, res);
            });

        srv_read_offset_ = node_->create_service<ReadOffsetSrv>(
            SRV_READ_OFFSET,
            [this](const std::shared_ptr<ReadOffsetSrv::Request>& req,
                   const std::shared_ptr<ReadOffsetSrv::Response>& res) {
                execute_read_offset(req, res);
            });

        srv_set_angle_ = node_->create_service<SetAngleSrv>(
            SRV_SET_ANGLE,
            [this](const std::shared_ptr<SetAngleSrv::Request>& req,
                   const std::shared_ptr<SetAngleSrv::Response>& res) {
                execute_set_angle(req, res);
            });

        srv_set_offset_ = node_->create_service<SetOffsetSrv>(
            SRV_SET_OFFSET,
            [this](const std::shared_ptr<SetOffsetSrv::Request>& req,
                   const std::shared_ptr<SetOffsetSrv::Response>& res) {
                execute_set_offset(req, res);
            });

        srv_stop_servo_ = node_->create_service<StopServoSrv>(
            SRV_STOP_SERVO,
            [this](const std::shared_ptr<StopServoSrv::Request>& req,
                   const std::shared_ptr<StopServoSrv::Response>& res) {
                execute_stop_servo(req, res);
            });

        srv_set_angles_ = node_->create_service<SetAnglesSrv>(
            SRV_SET_ANGLES,
            [this](const std::shared_ptr<SetAnglesSrv::Request>& req,
                   const std::shared_ptr<SetAnglesSrv::Response>& res) {
                execute_set_angles(req, res);
            });

        RCLCPP_INFO(logger_, "TestUbtechServo ready — all services registered");
    }

    bool TestUbtechServo::init_hardware() {
        const std::string serial_device = node_->get_parameter("serial_device").as_string();
        const int baud_rate = static_cast<int>(node_->get_parameter("baud_rate").as_int());
        const std::string gpio_name = node_->get_parameter("gpio_name").as_string();

        RCLCPP_INFO(logger_, "init_hardware: serial=%s @ %d baud, gpio_name=%s",
                    serial_device.c_str(), baud_rate, gpio_name.c_str());

        serial_ = std::make_unique<SerialPort>();
        if (!serial_->open(serial_device, baud_rate)) {
            RCLCPP_ERROR(logger_, "Cannot open serial port '%s' at %d baud",
                         serial_device.c_str(), baud_rate);
            return false;
        }

        tx_en_ = std::make_unique<GpioPin>();
        if (!tx_en_->open(gpio_name, "test_ubtech_servo_tx_en")) {
            RCLCPP_ERROR(logger_, "Cannot open GPIO %s", gpio_name.c_str());
            return false;
        }

        servo_ = std::make_unique<UbtechServo>(serial_.get(), tx_en_.get());

        RCLCPP_INFO(logger_, "init_hardware: success");
        return true;
    }
    void TestUbtechServo::execute_modify_id(const std::shared_ptr<ModifyIdSrv::Request>& req,
                                            const std::shared_ptr<ModifyIdSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        auto result = servo_->modify_id(req->old_id, req->new_id);
        res->success = result.has_value() && result.value();
        res->message = res->success ? "OK" : result.error();
        RCLCPP_INFO(logger_, "modify_id(%u -> %u): %s",
                    req->old_id, req->new_id, res->message.c_str());
    }

    void TestUbtechServo::execute_read_angle(const std::shared_ptr<ReadAngleSrv::Request>& req,
                                             const std::shared_ptr<ReadAngleSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        ServoAngle angle{};
        auto result = servo_->read_angle(req->servo_id, angle);
        res->success = result.has_value() && result.value();
        if (res->success) {
            res->target_deg = angle.target_deg;
            res->actual_deg = angle.actual_deg;
            res->message = "OK";
        }
        else {
            res->target_deg = 0.0;
            res->actual_deg = 0.0;
            res->message = result.has_value() ? "read_angle failed" : result.error();
        }
        RCLCPP_INFO(logger_, "read_angle(ID %u): target=%.1f actual=%.1f %s",
                    req->servo_id, res->target_deg, res->actual_deg,
                    res->message.c_str());
    }

    void TestUbtechServo::execute_read_firmware(const std::shared_ptr<ReadFirmwareSrv::Request>& req,
                                                const std::shared_ptr<ReadFirmwareSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        std::string version;
        auto result = servo_->read_firmware_version(req->servo_id, version);
        res->success = result.has_value() && result.value();
        res->version = res->success ? version : "";
        res->message = res->success ? "OK" : result.error();
        RCLCPP_INFO(logger_, "read_firmware(ID %u): %s %s",
                    req->servo_id, res->version.c_str(), res->message.c_str());
    }

    void TestUbtechServo::execute_read_offset(const std::shared_ptr<ReadOffsetSrv::Request>& req,
                                              const std::shared_ptr<ReadOffsetSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        double offset_deg = 0.0;
        auto result = servo_->read_offset(req->servo_id, offset_deg);
        res->success = result.has_value() && result.value();
        res->offset_deg = res->success ? offset_deg : 0.0;
        res->message = res->success ? "OK" : result.error();
        RCLCPP_INFO(logger_, "read_offset(ID %u): %.2f° %s",
                    req->servo_id, res->offset_deg, res->message.c_str());
    }
    void TestUbtechServo::execute_set_angle(const std::shared_ptr<SetAngleSrv::Request>& req,
                                            const std::shared_ptr<SetAngleSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        const auto& angle = req->angle;
        auto result = servo_->set_angle(angle.servo_id, angle.angle_deg,
                                        angle.motion_time, angle.lock_time_ms);
        res->success = result.has_value() && result.value();
        res->message = res->success ? "OK" : result.error();
        RCLCPP_INFO(logger_, "set_angle(ID %u, %u°, t=%u, lock=%u ms): %s",
                    angle.servo_id, angle.angle_deg, angle.motion_time,
                    angle.lock_time_ms, res->message.c_str());
    }

    void TestUbtechServo::execute_set_offset(const std::shared_ptr<SetOffsetSrv::Request>& req,
                                             const std::shared_ptr<SetOffsetSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        auto result = servo_->set_offset(req->servo_id, req->offset_deg);
        res->success = result.has_value() && result.value();
        res->message = res->success ? "OK" : result.error();
        RCLCPP_INFO(logger_, "set_offset(ID %u, %.2f°): %s",
                    req->servo_id, req->offset_deg, res->message.c_str());
    }

    void TestUbtechServo::execute_stop_servo(const std::shared_ptr<StopServoSrv::Request>& req,
                                             const std::shared_ptr<StopServoSrv::Response>& res) const {
        if (!servo_) {
            res->success = false;
            res->message = "Hardware not initialized";
            return;
        }
        // 广播停止时 (ID=0)
        auto result = servo_->stop(req->servo_id);
        res->success = result.has_value() && result.value();
        res->message = res->success ? "OK" : result.error();
        RCLCPP_INFO(logger_, "stop(ID %u): %s", req->servo_id, res->message.c_str());
    }

    void TestUbtechServo::execute_set_angles(const std::shared_ptr<SetAnglesSrv::Request>& req,
                                             const std::shared_ptr<SetAnglesSrv::Response>& res) const {
        auto set_angle_to_angle_command = [&](const SetAngle& angle) -> AngleCommand {
            AngleCommand command;
            command.id = angle.servo_id;
            command.angle_deg = angle.angle_deg;
            command.motion_time = angle.motion_time;
            command.lock_time_ms = angle.lock_time_ms;
            return command;
        };
        auto view = req->angles | std::views::transform(set_angle_to_angle_command);
        const std::vector<AngleCommand> commands(view.begin(), view.end());
        const auto& exp = servo_->set_angles(commands);
        res->success = exp.has_value() && exp.value();
        res->message = res->success ? "OK" : exp.error();
        LOG_INFO("set angles: {}", fmt::join(commands, " | "));
    }

} // namespace ubtech_servo_hardware
