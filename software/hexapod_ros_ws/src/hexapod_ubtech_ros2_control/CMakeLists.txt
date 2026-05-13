cmake_minimum_required(VERSION 3.8)
set(PROJECT_VERSION 1.0.0)
project(hexapod_ubtech_ros2_control VERSION ${PROJECT_VERSION})

if (DEFINED ENV{CLION_IDE})
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

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(TBB REQUIRED)

if (DEFINED ENV{CLION_IDE})
  set(common_log_cpp_DIR "${CMAKE_SOURCE_DIR}/install/common_log_cpp/share/common_log_cpp/cmake")
  message(${common_log_cpp_DIR})
endif ()
find_package(common_log_cpp REQUIRED)

find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(GPIOD REQUIRED libgpiod)
else()
  find_library(GPIOD_LIBRARIES NAMES gpiod)
  find_path(GPIOD_INCLUDE_DIRS gpiod.h)
endif()

add_library(${PROJECT_NAME} SHARED
        include/hexapod_ubtech_ros2_control/gpio_pin.hpp
        include/hexapod_ubtech_ros2_control/ubtech_servo.hpp
        include/hexapod_ubtech_ros2_control/servo_system.hpp
        src/gpio_pin.cpp
        src/serial_port.cpp
        src/ubtech_servo.cpp
        src/servo_system.cpp

)
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        src
        ${GPIOD_INCLUDE_DIRS}
)

ament_target_dependencies(${PROJECT_NAME} PUBLIC
        rclcpp
        hardware_interface
        pluginlib
        rclcpp_lifecycle
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        ${GPIOD_LIBRARIES}
        TBB::tbb
)

if (DEFINED ENV{CLION_IDE})
  target_link_libraries(${PROJECT_NAME} PUBLIC
          common_log_cpp
  )
else ()
  ament_target_dependencies(${PROJECT_NAME} PUBLIC
          common_log_cpp
  )
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -std=c++2b)
endif ()

if (BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif ()

if (DEFINED ENV{CLION_IDE})
  set_target_properties(${PROJECT_NAME} PROPERTIES
          BUILD_WITH_INSTALL_RPATH TRUE
          INSTALL_RPATH "$ORIGIN"
          SKIP_BUILD_RPATH FALSE
  )

  # 为动态库目标添加后构建步骤
  add_custom_command(TARGET ${PROJECT_NAME}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/install/${PROJECT_NAME}/lib"
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${PROJECT_NAME}>
          "${CMAKE_SOURCE_DIR}/install/${PROJECT_NAME}/lib"
          COMMENT "Copying ${PROJECT_NAME} library to install directory (CLion specific)"
  )

endif ()

pluginlib_export_plugin_description_file(
        hardware_interface hexapod_ubtech_ros2_control_plugin.xml)

## 安装生成的库文件
install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)

# 安装头文件（自定义的头文件）
install(
        DIRECTORY include/
        DESTINATION include
)
ament_export_include_directories(include)
ament_export_dependencies(
        hardware_interface
        pluginlib
        rclcpp
        rclcpp_lifecycle
)
ament_export_libraries(${PROJECT_NAME})

ament_package()