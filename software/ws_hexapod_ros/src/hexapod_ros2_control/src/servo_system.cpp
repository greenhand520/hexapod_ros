//
// Created by greenhand520 on 2026/4/25.
//

// src/servo_system.cpp
#include "hexapod_ros2_control/servo_system.hpp"
#include "hexapod_ros2_control/ubtech_servo.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <gpiod.h>

#include "pluginlib/class_list_macros.hpp"


namespace hexapod_ros2_control {

    static const rclcpp::Logger LOGGER = rclcpp::get_logger("ServoSystem");

    // ══════════════════════════════════════════════════════════════
    //  工厂方法 — 新增舵机类型时仅需修改此函数
    // ══════════════════════════════════════════════════════════════
    ServoBase::UniquePtr ServoSystem::create_servo_driver(const std::string& type) {
        if (type == "ubtech") {
            return std::make_unique<UbtechServo>();
        }
        // ─── 在此添加新舵机协议 ───
        // if (type == "dynamixel") return std::make_unique<DynamixelServo>();
        // if (type == "feetech")   return std::make_unique<FeetechServo>(); https://www.feetech.cn/drawing.html https://www.feetech.cn/74v18kg-serial-bus-steering-gear.html

        RCLCPP_ERROR(LOGGER, "未知舵机类型: [%s]", type.c_str());
        return nullptr;
    }

    // ══════════════════════════════════════════════════════════════
    //  角度转换
    // ══════════════════════════════════════════════════════════════
    double ServoSystem::joint_rad_to_servo_deg(double rad, const ServoJointConfig& cfg) const {
        // servo_deg = rad * direction * (180/π) + offset
        return rad * cfg.direction * (180.0 / M_PI) + cfg.offset_deg;
    }

    double ServoSystem::servo_deg_to_joint_rad(double deg, const ServoJointConfig& cfg) const {
        // rad = (servo_deg - offset) * direction * (π/180)
        return (deg - cfg.offset_deg) * cfg.direction * (M_PI / 180.0);
    }

    // ══════════════════════════════════════════════════════════════
    //  串口操作
    // ══════════════════════════════════════════════════════════════
    bool ServoSystem::open_serial() {
        serial_fd_ = ::open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (serial_fd_ < 0) {
            RCLCPP_ERROR(LOGGER, "打开串口 %s 失败: %s",
                         serial_port_.c_str(), strerror(errno));
            return false;
        }

        struct termios tty{};
        if (tcgetattr(serial_fd_, &tty) != 0) {
            RCLCPP_ERROR(LOGGER, "获取串口属性失败: %s", strerror(errno));
            close_serial();
            return false;
        }

        // 设置波特率
        speed_t speed;
        switch (baud_rate_) {
        case 921600:
            speed = B921600;
            break;
        case 115200:
            speed = B115200;
            break;
        case 460800:
            speed = B460800;
            break;
        default:
            RCLCPP_ERROR(LOGGER, "不支持的波特率: %d", baud_rate_);
            close_serial();
            return false;
        }
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);

        // 8N1, 无流控, 原始模式
        tty.c_cflag = static_cast<tcflag_t>(CS8 | CLOCAL | CREAD);
        tty.c_iflag = 0;
        tty.c_oflag = 0;
        tty.c_lflag = 0;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;

        tcflush(serial_fd_, TCIOFLUSH);
        if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
            RCLCPP_ERROR(LOGGER, "设置串口属性失败: %s", strerror(errno));
            close_serial();
            return false;
        }

        RCLCPP_INFO(LOGGER, "串口 %s 已打开, 波特率 %d", serial_port_.c_str(), baud_rate_);
        return true;
    }

    void ServoSystem::close_serial() {
        if (serial_fd_ >= 0) {
            ::close(serial_fd_);
            serial_fd_ = -1;
        }
    }

    bool ServoSystem::write_serial(const uint8_t* data, size_t length) {
        size_t total_written = 0;
        while (total_written < length) {
            ssize_t n = ::write(serial_fd_, data + total_written, length - total_written);
            if (n < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                RCLCPP_ERROR(LOGGER, "串口写入失败: %s", strerror(errno));
                return false;
            }
            total_written += static_cast<size_t>(n);
        }
        // 等待数据发送完毕
        tcdrain(serial_fd_);
        return true;
    }

    int ServoSystem::read_serial(uint8_t* buffer, size_t max_len, int timeout_ms) {
        struct pollfd pfd{};
        pfd.fd = serial_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            if (errno == EINTR)
                return 0;
            RCLCPP_ERROR(LOGGER, "串口poll失败: %s", strerror(errno));
            return -1;
        }
        if (ret == 0)
            return 0; // 超时

        ssize_t n = ::read(serial_fd_, buffer, max_len);
        if (n < 0) {
            if (errno == EAGAIN)
                return 0;
            RCLCPP_ERROR(LOGGER, "串口读取失败: %s", strerror(errno));
            return -1;
        }
        return static_cast<int>(n);
    }

    // ══════════════════════════════════════════════════════════════
    //  GPIO (STX引脚控制)
    // ══════════════════════════════════════════════════════════════
    bool ServoSystem::open_gpio() {
        gpio_chip_ = gpiod_chip_open_by_name(gpio_chip_name_.c_str());
        if (!gpio_chip_) {
            RCLCPP_ERROR(LOGGER, "打开GPIO芯片 %s 失败: %s",
                         gpio_chip_name_.c_str(), strerror(errno));
            return false;
        }

        stx_line_ = gpiod_chip_get_line(gpio_chip_, stx_pin_);
        if (!stx_line_) {
            RCLCPP_ERROR(LOGGER, "获取GPIO引脚 %u 失败: %s",
                         stx_pin_, strerror(errno));
            close_gpio();
            return false;
        }

        // 请求为输出模式，初始低电平(接收模式)
        int ret = gpiod_line_request_output(stx_line_, "hexapod_servo", 0);
        if (ret < 0) {
            RCLCPP_ERROR(LOGGER, "请求GPIO输出失败: %s", strerror(errno));
            close_gpio();
            return false;
        }

        RCLCPP_INFO(LOGGER, "GPIO %s pin %u 已初始化 (STX方向控制)",
                    gpio_chip_name_.c_str(), stx_pin_);
        return true;
    }

    void ServoSystem::close_gpio() {
        if (stx_line_) {
            gpiod_line_release(stx_line_);
            stx_line_ = nullptr;
        }
        if (gpio_chip_) {
            gpiod_chip_close(gpio_chip_);
            gpio_chip_ = nullptr;
        }
    }

    void ServoSystem::set_stx(bool high) {
        if (stx_line_) {
            gpiod_line_set_value(stx_line_, high ? 1 : 0);
        }
    }

    // ══════════════════════════════════════════════════════════════
    //  生命周期回调
    // ══════════════════════════════════════════════════════════════
    hardware_interface::CallbackReturn ServoSystem::on_init(const hardware_interface::HardwareInfo& info) {
        if (hardware_interface::SystemInterface::on_init(info) !=
            hardware_interface::CallbackReturn::SUCCESS) {
            return hardware_interface::CallbackReturn::ERROR;
        }

        // ── 读取硬件参数 ──
        serial_port_ = info_.hardware_parameters.value_or("serial_port");
        baud_rate_ = std::stoi(info_.hardware_parameters.value_or("baud_rate", "921600"));
        gpio_chip_name_ = info_.hardware_parameters.value_or("gpio_chip", "gpiochip0");
        stx_pin_ = static_cast<unsigned int>(
            std::stoi(info_.hardware_parameters.value_or("stx_pin", "17")));
        std::string servo_type = info_.hardware_parameters.value_or("servo_type", "ubtech");
        enable_angle_read_ = (info_.hardware_parameters.value_or("enable_angle_read", "true") == "true");
        read_every_n_cycles_ = std::stoi(info_.hardware_parameters.value_or("read_every_n_cycles", "10"));

        // ── 创建舵机协议驱动 ──
        servo_driver_ = create_servo_driver(servo_type);
        if (!servo_driver_) {
            return hardware_interface::CallbackReturn::ERROR;
        }

        // ── 解析关节配置 ──
        size_t num_joints = info_.joints.size();
        if (num_joints != 18) {
            RCLCPP_WARN(LOGGER, "期望18个关节(六足×3), 实际 %zu 个", num_joints);
        }

        servo_configs_.resize(num_joints);
        hw_commands_.resize(num_joints, 0.0);
        hw_positions_.resize(num_joints, 0.0);

        for (size_t i = 0; i < num_joints; ++i) {
            const auto& joint = info_.joints[i];
            auto& cfg = servo_configs_[i];

            cfg.joint_name = joint.name;
            cfg.id = static_cast<uint8_t>(
                std::stoi(joint.parameters.value_or("servo_id", std::to_string(i + 1))));
            cfg.offset_deg = std::stod(joint.parameters.value_or("offset_deg", "120.0"));

            std::string dir_str = joint.parameters.value_or("direction", "1.0");
            cfg.direction = (dir_str == "-1.0" || dir_str == "-1") ? -1.0 : 1.0;

            RCLCPP_INFO(LOGGER, "  关节[%zu] %-20s → 舵机ID=%3u  偏移=%.1f°  方向=%.0f",
                        i, cfg.joint_name.c_str(), cfg.id, cfg.offset_deg, cfg.direction);
        }

        // ── 预分配缓冲区 ──
        // 发送缓冲区: 18帧 × 10字节 = 180字节
        tx_buffer_.reserve(num_joints * UbtechServo::FRAME_LENGTH);
        // 接收缓冲区: 18帧 × 10字节 = 180字节 (最大情况)
        rx_buffer_.resize(num_joints * UbtechServo::FRAME_LENGTH);

        RCLCPP_INFO(LOGGER, "ServoSystem 初始化完成: %s 协议, %zu 个舵机",
                    servo_driver_->get_name().c_str(), num_joints);

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ServoSystem::on_configure(
        const rclcpp_lifecycle::State& /*previous_state*/) {
        RCLCPP_INFO(LOGGER, "配置中...");

        // 打开串口
        if (!open_serial()) {
            return hardware_interface::CallbackReturn::ERROR;
        }

        // 打开GPIO
        if (!open_gpio()) {
            close_serial();
            return hardware_interface::CallbackReturn::ERROR;
        }

        configured_ = true;
        RCLCPP_INFO(LOGGER, "配置完成");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ServoSystem::on_cleanup(
        const rclcpp_lifecycle::State& /*previous_state*/) {
        RCLCPP_INFO(LOGGER, "清理中...");
        close_gpio();
        close_serial();
        configured_ = false;
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ServoSystem::on_activate(
        const rclcpp_lifecycle::State& /*previous_state*/) {
        RCLCPP_INFO(LOGGER, "激活中...");
        if (!configured_) {
            RCLCPP_ERROR(LOGGER, "未配置，无法激活");
            return hardware_interface::CallbackReturn::ERROR;
        }

        // 初始化: 将当前位置设为指令值(防止启动时跳变)
        for (size_t i = 0; i < hw_commands_.size(); ++i) {
            hw_commands_[i] = 0.0;
            hw_positions_[i] = 0.0;
        }

        activated_.store(true);
        RCLCPP_INFO(LOGGER, "已激活");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn ServoSystem::on_deactivate(
        const rclcpp_lifecycle::State& /*previous_state*/) {
        RCLCPP_INFO(LOGGER, "停用中...");
        activated_.store(false);

        // 发送停止指令使所有舵机失电
        if (serial_fd_ >= 0) {
            tx_buffer_.clear();
            for (const auto& cfg : servo_configs_) {
                auto cmd = servo_driver_->build_stop_cmd(cfg.id);
                tx_buffer_.insert(tx_buffer_.end(), cmd.begin(), cmd.end());
            }
            set_stx(true);
            write_serial(tx_buffer_.data(), tx_buffer_.size());
            set_stx(false);
        }

        RCLCPP_INFO(LOGGER, "已停用");
        return hardware_interface::CallbackReturn::SUCCESS;
    }

    // ══════════════════════════════════════════════════════════════
    //  接口导出
    // ══════════════════════════════════════════════════════════════
    std::vector<hardware_interface::StateInterface>
    ServoSystem::export_state_interfaces() {
        std::vector<hardware_interface::StateInterface> interfaces;
        interfaces.reserve(servo_configs_.size());

        for (size_t i = 0; i < servo_configs_.size(); ++i) {
            interfaces.emplace_back(
                servo_configs_[i].joint_name,
                hardware_interface::HW_IF_POSITION,
                &hw_positions_[i]);
        }
        return interfaces;
    }

    std::vector<hardware_interface::CommandInterface>
    ServoSystem::export_command_interfaces() {
        std::vector<hardware_interface::CommandInterface> interfaces;
        interfaces.reserve(servo_configs_.size());

        for (size_t i = 0; i < servo_configs_.size(); ++i) {
            interfaces.emplace_back(
                servo_configs_[i].joint_name,
                hardware_interface::HW_IF_POSITION,
                &hw_commands_[i]);
        }
        return interfaces;
    }

    // ══════════════════════════════════════════════════════════════
    //  read() — 读取舵机实际角度
    // ══════════════════════════════════════════════════════════════
    hardware_interface::return_type ServoSystem::read(
        const rclcpp::Time& /*time*/, const rclcpp::Duration& /*period*/) {
        if (!activated_.load()) {
            return hardware_interface::return_type::OK;
        }

        // 注意: 参考文档指出角度回读后舵机会失电。
        // 因此默认每N个周期才回读一次角度，避免频繁失电。
        if (!enable_angle_read_) {
            // 开环模式: 状态直接等于指令
            hw_positions_ = hw_commands_;
            return hardware_interface::return_type::OK;
        }

        if (++read_skip_counter_ < read_every_n_cycles_) {
            // 跳过本次回读，保持上一次的状态
            return hardware_interface::return_type::OK;
        }
        read_skip_counter_ = 0;

        std::lock_guard<std::mutex> lock(comm_mutex_);

        // 构建所有舵机的角度回读指令
        tx_buffer_.clear();
        for (const auto& cfg : servo_configs_) {
            auto cmd = servo_driver_->build_read_angle_cmd(cfg.id);
            tx_buffer_.insert(tx_buffer_.end(), cmd.begin(), cmd.end());
        }

        // 清空串口接收缓冲区
        tcflush(serial_fd_, TCIFLUSH);

        // 拉高STX → 发送 → 拉低STX → 接收
        set_stx(true);
        bool ok = write_serial(tx_buffer_.data(), tx_buffer_.size());
        set_stx(false);

        if (!ok) {
            RCLCPP_WARN(LOGGER, "发送角度回读指令失败");
            return hardware_interface::return_type::OK;
        }

        // 等待舵机响应 (每个舵机10字节, 18个=180字节)
        // 921600baud下180字节约2ms, 加上舵机处理时间, 给50ms超时
        size_t expected = servo_configs_.size() * UbtechServo::FRAME_LENGTH;
        size_t total_read = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);

        while (total_read < expected) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                break;
            int remaining_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
            int n = read_serial(rx_buffer_.data() + total_read,
                                expected - total_read, remaining_ms);
            if (n > 0) {
                total_read += static_cast<size_t>(n);
            }
            else if (n < 0) {
                break;
            }
        }

        if (total_read < expected) {
            RCLCPP_WARN_THROTTLE(LOGGER, *rclcpp::Clock::make_shared(), 2000,
                                 "角度回读数据不完整: 收到 %zu/%zu 字节",
                                 total_read, expected);
        }

        // 解析响应: 逐帧扫描，按舵机ID匹配关节
        size_t offset = 0;
        std::unordered_map<uint8_t, double> angle_map;

        while (offset + UbtechServo::FRAME_LENGTH <= total_read) {
            // 寻找帧头 FA AF
            if (rx_buffer_[offset] == UbtechServo::FRAME_HEADER_0 &&
                offset + 1 < total_read &&
                rx_buffer_[offset + 1] == UbtechServo::FRAME_HEADER_1) {

                auto angle_data = servo_driver_->parse_angle_response(
                    rx_buffer_.data() + offset, total_read - offset);

                if (angle_data.has_value()) {
                    uint8_t resp_id = rx_buffer_[offset + 2];
                    angle_map[resp_id] = angle_data->actual_angle_deg;
                    offset += UbtechServo::FRAME_LENGTH;
                    continue;
                }
            }
            ++offset; // 未匹配帧头，向前滑动
        }

        // 更新关节状态
        for (size_t i = 0; i < servo_configs_.size(); ++i) {
            auto it = angle_map.find(servo_configs_[i].id);
            if (it != angle_map.end()) {
                hw_positions_[i] = servo_deg_to_joint_rad(it->second, servo_configs_[i]);
            }
            // 未收到响应的关节保持上一次的值不变
        }

        return hardware_interface::return_type::OK;
    }

    // ══════════════════════════════════════════════════════════════
    //  write() — 一次性发送所有舵机的角度指令
    // ══════════════════════════════════════════════════════════════
    hardware_interface::return_type ServoSystem::write(
        const rclcpp::Time& /*time*/, const rclcpp::Duration& /*period*/) {
        if (!activated_.load()) {
            return hardware_interface::return_type::OK;
        }

        std::lock_guard<std::mutex> lock(comm_mutex_);

        // 构建18个舵机的设置角度指令帧
        tx_buffer_.clear();
        for (size_t i = 0; i < servo_configs_.size(); ++i) {
            double servo_deg = joint_rad_to_servo_deg(hw_commands_[i], servo_configs_[i]);

            auto cmd = servo_driver_->build_set_angle_cmd(
                servo_configs_[i].id, servo_deg,
                0, // move_time=0: 全速转动
                0); // lock_time=0: 不锁定，允许连续更新

            tx_buffer_.insert(tx_buffer_.end(), cmd.begin(), cmd.end());
        }

        // 拉高STX(发送模式) → 一次性写入全部数据 → 拉低STX(接收模式)
        set_stx(true);
        bool ok = write_serial(tx_buffer_.data(), tx_buffer_.size());
        set_stx(false);

        if (!ok) {
            RCLCPP_ERROR(LOGGER, "发送舵机指令失败");
            return hardware_interface::return_type::ERROR;
        }

        return hardware_interface::return_type::OK;
    }

} // namespace hexapod_ros2_control


PLUGINLIB_EXPORT_CLASS(hexapod_ros2_control::ServoSystem, hardware_interface::SystemInterface)