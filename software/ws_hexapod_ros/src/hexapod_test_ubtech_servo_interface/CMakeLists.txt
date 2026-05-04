cmake_minimum_required(VERSION 3.8)
set(PROJECT_VERSION 1.0.0)
project(hexapod_test_ubtech_servo_interface VERSION ${PROJECT_VERSION})

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(std_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
        "srv/ModifyId.srv"
        "srv/ReadAngle.srv"
        "srv/ReadFirmware.srv"
        "srv/ReadOffset.srv"
        "srv/SetAngle.srv"
        "srv/SetOffset.srv"
        "srv/StopServo.srv"
        DEPENDENCIES builtin_interfaces std_msgs
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

if (DEFINED ENV{CLION_IDE})
    set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN"
            SKIP_BUILD_RPATH FALSE
    )

endif ()

ament_package()