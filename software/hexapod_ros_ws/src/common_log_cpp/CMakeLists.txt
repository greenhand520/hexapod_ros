cmake_minimum_required(VERSION 3.8)
set(PROJECT_VERSION 1.0.0)
project(common_log_cpp VERSION ${PROJECT_VERSION})

if(DEFINED ENV{CLION_IDE})
    message(STATUS "CLion env detected")
endif ()

find_program(GCC13_PATH "gcc-13")
if (GCC13_PATH)
    set(CMAKE_C_COMPILER "/usr/bin/gcc-13")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++-13")
    message(STATUS "Using GCC-13: ${GCC13_PATH}")
else ()
    message(FATAL_ERROR "gcc-13 not found, falling back to default")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

add_library(${PROJECT_NAME} SHARED
        include/common_log_cpp/log_interface/log_interface.hpp
        include/common_log_cpp/log_interface/log_manager.hpp
        include/common_log_cpp/log_interface/log_macro.hpp
        include/common_log_cpp/spdlog_adapter.hpp
        include/common_log_cpp/log.hpp
        include/common_log_cpp/spdlog_adapter.hpp
        src/log.cpp
        src/spdlog_adapter.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        fmt::fmt
        spdlog::spdlog
        Threads::Threads
)

ament_target_dependencies(${PROJECT_NAME} PUBLIC
        rclcpp
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 显式传递 C++23 标志（GCC 13 使用 -std=c++23 或 -std=c++2b）
    target_compile_options(${PROJECT_NAME} PRIVATE -std=c++2b)
endif ()

if(DEFINED ENV{CLION_IDE})

    set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN"  # 相对于安装后的可执行文件
            SKIP_BUILD_RPATH FALSE
    )
endif ()

# 安装库文件
install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        # 确保头文件包含路径被导出
        INCLUDES DESTINATION include
)

# 安装头文件
install(
        DIRECTORY include/
        DESTINATION include
)

# 导出包信息
ament_export_dependencies(
        rclcpp
)
ament_export_targets(${PROJECT_NAME})
ament_export_include_directories(include)

ament_package()