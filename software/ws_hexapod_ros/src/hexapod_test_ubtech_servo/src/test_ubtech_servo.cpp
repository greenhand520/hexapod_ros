//
// Created by greenhand520 on 2026/5/4.
//

#include <rclcpp/rclcpp.hpp>

#include <memory>
#include <string>

#include "hexapod_test_ubtech_servo/test_ubtech_servo.hpp"

using namespace ubtech_servo_hardware;

namespace ubtech_servo_hardware {

    TestUbtechServo::TestUbtechServo(const rclcpp::Node::SharedPtr& node)
        : node_(node),
          logger_(node->get_logger()) {

        node_->declare_parameter<std::string>("serial_device", "/dev/ttyUSB0");
        node_->declare_parameter<int>("baud_rate", 115200);
        node_->declare_parameter<std::string>("gpio_chip", "/dev/gpiochip0");
        node_->declare_parameter<int>("tx_en_line", 25);

        // ── 初始化硬件 ───────────────────────────────────────────────────
        if (!init_hardware()) {
            RCLCPP_ERROR(logger_, "Hardware initialization failed, services will not be available");
            return;
        }

        // ── 注册服务 ─────────────────────────────────────────────────────
        using namespace std::placeholders;

        srv_modify_id_ = node_->create_service<ModifyId>(
            SRV_MODIFY_ID,
            [this](const std::shared_ptr<ModifyId::Request>& req,
                   const std::shared_ptr<ModifyId::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                const bool ok = servo_->modify_id(req->old_id, req->new_id);
                res->success = ok;
                res->message = ok ? "OK" : "modify_id failed";
                RCLCPP_INFO(logger_, "modify_id(%u -> %u): %s",
                            req->old_id, req->new_id, res->message.c_str());
            });

        srv_read_angle_ = node_->create_service<ReadAngle>(
            SRV_READ_ANGLE,
            [this](const std::shared_ptr<ReadAngle::Request>& req,
                   const std::shared_ptr<ReadAngle::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                ServoAngle angle{};
                const bool ok = servo_->read_angle(req->servo_id, angle);
                res->success = ok;
                if (ok) {
                    res->target_deg = angle.target_deg;
                    res->actual_deg = angle.actual_deg;
                    res->message = "OK";
                } else {
                    res->target_deg = 0.0;
                    res->actual_deg = 0.0;
                    res->message = "read_angle failed";
                }
                RCLCPP_INFO(logger_, "read_angle(ID %u): target=%.1f actual=%.1f %s",
                            req->servo_id, res->target_deg, res->actual_deg,
                            res->message.c_str());
            });

        srv_read_firmware_ = node_->create_service<ReadFirmware>(
            SRV_READ_FIRMWARE,
            [this](const std::shared_ptr<ReadFirmware::Request>& req,
                   const std::shared_ptr<ReadFirmware::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                std::string version;
                const bool ok = servo_->read_firmware_version(req->servo_id, version);
                res->success = ok;
                res->version = ok ? version : "";
                res->message = ok ? "OK" : "read_firmware_version failed";
                RCLCPP_INFO(logger_, "read_firmware(ID %u): %s %s",
                            req->servo_id, res->version.c_str(), res->message.c_str());
            });

        srv_read_offset_ = node_->create_service<ReadOffset>(
            SRV_READ_OFFSET,
            [this](const std::shared_ptr<ReadOffset::Request>& req,
                   const std::shared_ptr<ReadOffset::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                double offset = 0.0;
                const bool ok = servo_->read_offset(req->servo_id, offset);
                res->success = ok;
                res->offset_deg = ok ? offset : 0.0;
                res->message = ok ? "OK" : "read_offset failed";
                RCLCPP_INFO(logger_, "read_offset(ID %u): %.2f° %s",
                            req->servo_id, res->offset_deg, res->message.c_str());
            });

        srv_set_angle_ = node_->create_service<SetAngle>(
            SRV_SET_ANGLE,
            [this](const std::shared_ptr<SetAngle::Request>& req,
                   const std::shared_ptr<SetAngle::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                const bool ok = servo_->set_angle(req->servo_id, req->angle_deg,
                                            req->motion_time, req->lock_time_ms);
                res->success = ok;
                res->message = ok ? "OK" : "set_angle failed";
                RCLCPP_INFO(logger_, "set_angle(ID %u, %u°, t=%u, lock=%u ms): %s",
                            req->servo_id, req->angle_deg, req->motion_time,
                            req->lock_time_ms, res->message.c_str());
            });

        srv_set_offset_ = node_->create_service<SetOffset>(
            SRV_SET_OFFSET,
            [this](const std::shared_ptr<SetOffset::Request>& req,
                   const std::shared_ptr<SetOffset::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                const bool ok = servo_->set_offset(req->servo_id, req->offset_deg);
                res->success = ok;
                res->message = ok ? "OK" : "set_offset failed";
                RCLCPP_INFO(logger_, "set_offset(ID %u, %.2f°): %s",
                            req->servo_id, req->offset_deg, res->message.c_str());
            });

        srv_stop_ = node_->create_service<StopServo>(
            SRV_STOP_SERVO,
            [this](const std::shared_ptr<StopServo::Request>& req,
                   const std::shared_ptr<StopServo::Response>& res) {
                if (!servo_) {
                    res->success = false;
                    res->message = "Hardware not initialized";
                    return;
                }
                // 广播停止时 (ID=0)
                const bool ok = servo_->stop(req->servo_id);
                res->success = ok;
                res->message = ok ? "OK (broadcast stop sent)" : "stop failed";
                RCLCPP_INFO(logger_, "stop(broadcast): %s", res->message.c_str());
            });

        RCLCPP_INFO(logger_, "TestUbtechServo ready — all services registered");
    }


    bool TestUbtechServo::init_hardware() {
        const std::string serial_device = node_->get_parameter("serial_device").as_string();
        const int baud_rate = static_cast<int>(node_->get_parameter("baud_rate").as_int());
        const std::string gpio_chip = node_->get_parameter("gpio_chip").as_string();
        const int tx_en_line = static_cast<int>(node_->get_parameter("tx_en_line").as_int());

        RCLCPP_INFO(logger_, "init_hardware: serial=%s @ %d baud, gpio=%s line=%d",
                    serial_device.c_str(), baud_rate, gpio_chip.c_str(), tx_en_line);

        serial_ = std::make_unique<SerialPort>();
        if (!serial_->open(serial_device, baud_rate)) {
            RCLCPP_ERROR(logger_, "Cannot open serial port '%s' at %d baud",
                         serial_device.c_str(), baud_rate);
            return false;
        }

        tx_en_ = std::make_unique<GpioPin>();
        if (!tx_en_->open(gpio_chip, static_cast<unsigned int>(tx_en_line),
                          "test_ubtech_servo_tx_en", 0)) {
            RCLCPP_ERROR(logger_, "Cannot open GPIO %s line %d",
                         gpio_chip.c_str(), tx_en_line);
            return false;
        }

        servo_ = std::make_unique<UbtechServo>(serial_.get(), tx_en_.get());

        RCLCPP_INFO(logger_, "init_hardware: success");
        return true;
    }

} // namespace ubtech_servo_hardware