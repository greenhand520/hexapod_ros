common_log_cpp

公共的日志记录，使用 spdlog 代替 rclcpp 的后端日志实现，并且支持自定义日志实现

## 使用方法

### CMakeLists配置

```cmake
find_package(common_log_cpp REQUIRED)

if (DEFINED ENV{CLION_IDE})
    target_link_libraries(${PROJECT_NAME} PUBLIC
            common_log_cpp
    )
else ()
    ament_target_dependencies(${PROJECT_NAME} PUBLIC
            common_log_cpp
    )
endif ()
```

### 初始化

方式1：当模块可以获取到 ros2 节点 node 实例时，可以在在 main 函数中按照下面顺序初始化全局日志

```cpp
#include "common_log_cpp/rclcpp_log_handler.hpp"
#include "common_log_cpp/spdlog_adapter.hpp"
#include "common_log_cpp/log_interface/log_manager.hpp"

const auto node = std::make_shared<rclcpp::Node>("log_test_node");
// 1、先使用RclcppLogHandler初始化日志logger
common_log::RclcppLogHandler::init_from_node(node);
// 2、获取刚才创建的logger实例中得到spdlogger对象
const auto logger = common_log::Logger::get_instance().get_logger();
// 3、创建一个适配spdlog的log_interface对象
const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger);
// 4、设置全局日志 这样就统一log_macro.hpp和RCLCPP_xxx()的日志记录结果了
log_interface::LogManager::set(logger_interface);
```

方式2：获取不到 node 实例时，可以按照下面顺序初始化日志

```cpp
// 还是先使用RclcppLogHandler初始化日志logger
// 假设params是你存储配置的map,map的key和yaml中定义的一样
std::unordered_map<std::string, std::string> params;
common_log::RclcppLogHandler::init_from_map(params);
// 或者自己实现获取LogConfig
LogConfig log_config;
common_log::RclcppLogHandler::init_from_config(log_config);
// 后续操作和前面一样
const auto logger = common_log::Logger::get_instance().get_logger();
const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger);
log_interface::LogManager::set(logger_interface);
```

方式3：如果不想代替 RCLCPP 的后端来记录日志，不调用`RclcppLogHandler`的几个`init_from_xxx`函数即可。

```cpp
// 先创建LogConfig，它的配置内容获取可以自己实现
common_log::LogConfig log_config;
// 这里提供了一个默认实现，假设params是你存储配置的map
std::unordered_map<std::string, std::string> params;
log_config.init_from_map(params);
// 初始化日志logger
common_log::Logger::init_from_config(&log_config);
// 后续操作和前面一样
const auto logger = common_log::Logger::get_instance().get_logger();
const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger);
log_interface::LogManager::set(logger_interface);
```

> 调用`RclcppLogHandler`中的几个`init_from_xxx`后 RCLCPP 记录日志的后端才会用这个 common_log_cpp 来实现，而具体的实现依靠`log_interface::LogManager::set(logger_interface);`

### 记录日志

1、ros2相关代码中正常使用

```cpp
    RCLCPP_DEBUG(node->get_logger(), "debug log test: ctcemti_log_test_node started");
    RCLCPP_INFO(node->get_logger(), "info log test: ctcemti_log_test_node started");
    RCLCPP_WARN(node->get_logger(), "warn log test: ctcemti_log_test_node started");
    RCLCPP_ERROR(node->get_logger(), "error log test: ctcemti_log_test_node started");
    RCLCPP_FATAL(node->get_logger(), "fatal log test: ctcemti_log_test_node started");
```

2、使用`log_interface/log_macro.hpp`中的宏

```cpp
LOG_DEBUG("test_log_debug {}", 123);
LOG_INFO("test_log_info {}", 123);
LOG_INFO("Joint '{}' (ID {}): initial {:.1f}° → {:.4f} rad", jd.name, jd.servo_id, sa.actual_deg, pos);
LOG_WARN("test_log_warn {}", 123);
LOG_ERROR("test_log_error {}", 123);
LOG_FATAL("test_log_fatal {}", 123);
```

### 参数配置

`common_log::RclcppLogHandler::init_from_node(node);`会从 node 的参数中获取一些日志相关的配置，如下：

```yaml
log:
    # 日志登记
    log_level: debug
    # 日志路径
    log_path: ./log
    # 是否是异步日志
    async: false
    # 日志器名字，会影响最终日志文件的名字，不设置的话默认使用节点名
    logger_nama: test
```

上面配置最终会产生日志的路径是`./log/log_test.log`
