cmake_minimum_required(VERSION 3.22)
set(PROJECT_VERSION 1.0.0)
project(hexapod_controller VERSION ${PROJECT_VERSION})

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

option(BUILD_EXE "Build node executable file" ON)
set(PROJECT_EXE_NAME ${PROJECT_NAME}_exe)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(TBB REQUIRED)
if (DEFINED ENV{CLION_IDE})
    set(common_cpp_DIR "${CMAKE_SOURCE_DIR}/install/common_cpp/share/common_cpp/cmake")
endif ()
find_package(common_cpp REQUIRED)

add_library(${PROJECT_NAME} SHARED
        include/hexapod_controller/ros2_msg.hpp
        include/hexapod_controller/hexapod_controller.hpp
        src/hexapod_controller.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        src
)

ament_target_dependencies(${PROJECT_NAME} PUBLIC
        rclcpp
        std_msgs
        std_srvs
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        TBB::tbb
)

if (DEFINED ENV{CLION_IDE})
#    rosidl_get_typesupport_target(hexapod_test_ubtech_servo_interface_TARGET
#            hexapod_test_ubtech_servo_interface "rosidl_typesupport_cpp")
    target_link_libraries(${PROJECT_NAME} PUBLIC
            common_cpp
    )
else ()
    ament_target_dependencies(${PROJECT_NAME} PUBLIC
#            hexapod_test_ubtech_servo_interface
#            hexapod_ubtech_ros2_control
            common_cpp
    )
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -std=c++2b)
endif ()

if (BUILD_EXE)
    message(STATUS "Build node executable")
    add_executable(${PROJECT_EXE_NAME}
            src/main.cpp
    )

    target_link_libraries(${PROJECT_EXE_NAME} PRIVATE
            ${PROJECT_NAME}
    )

    if (BUILD_EXE)
        target_compile_options(${PROJECT_EXE_NAME} PRIVATE -std=c++2b)
    endif ()

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
            INSTALL_RPATH "$ORIGIN"  # 相对于安装后的可执行文件
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

    execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/install/${PROJECT_NAME}/share/${PROJECT_NAME}/config"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "${CMAKE_SOURCE_DIR}/install/${PROJECT_NAME}/share/${PROJECT_NAME}/config"
            RESULT_VARIABLE result
    )
    if (NOT result EQUAL 1)
        message(STATUS "Copying config to install directory (CLion specific)")
    else ()
        message(WARNING "Failed to copy config to install directory")
    endif ()

    execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/install/${PROJECT_NAME}/share/${PROJECT_NAME}/launch"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/launch"
            "${CMAKE_SOURCE_DIR}/install/${PROJECT_NAME}/share/${PROJECT_NAME}/launch"
            RESULT_VARIABLE result
    )
    if (NOT result EQUAL 1)
        message(STATUS "Copying config to launch directory (CLion specific)")
    else ()
        message(WARNING "Failed to copy launch to install directory")
    endif ()

endif ()

# 安装库和头文件
install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        # 确保头文件包含路径被导出
        INCLUDES DESTINATION include
)

if (BUILD_EXE)
    install(TARGETS ${PROJECT_EXE_NAME}
            DESTINATION lib/${PROJECT_NAME}
    )

    install(DIRECTORY launch config
            DESTINATION share/${PROJECT_NAME})
endif ()

install(DIRECTORY include/
        DESTINATION include
)

ament_export_include_directories(
        include
)

ament_export_libraries(
        ${PROJECT_NAME}
)

ament_export_dependencies(
        rclcpp
        std_msgs
        std_srvs
        common_cpp
)
ament_package()