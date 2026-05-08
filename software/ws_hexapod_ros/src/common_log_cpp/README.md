cpp_common_log

公共的日志记录

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

在main函数中按照下面顺序初始化全局日志

```cpp
#include "common_log_cpp/rclcpp_log_handler.hpp"
#include "common_log_cpp/spdlog_adapter.hpp"
#include "common_log_cpp/log_interface/log_manager.hpp"

const auto node = std::make_shared<rclcpp::Node>("log_test_node");
// 1、先使用node初始化日志logger
common_log::RclcppLogHandler::init_from_node(node);
// 2、获取刚才创建的logger实例中得到spdlogger对象
const auto logger = common_log::Logger::get_instance().get_logger();
// 3、创建一个适配spdlog的log_interface对象
const auto logger_interface = common_log::SpdlogLoggerAdapter::create_spdlog_logger(logger);
// 4、设置全局日志 这样就统一log_macro.hpp和RCLCPP_xxx()的日志记录结果了
log_interface::LogManager::set(logger_interface);
```

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
    # 使用none表示关闭发布日志，其他等级时，每产生一条日志会发布一个app_log的话题
    topic_log_level: info
    # 日志路径
    log_path: ./log
    # 是否是异步日志
    async: false
```
