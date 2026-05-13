cmake_minimum_required(VERSION 3.22)
set(PROJECT_VERSION 1.0.0)
project(hexapod_bringup VERSION ${PROJECT_VERSION})

if (DEFINED ENV{CLION_IDE})
    message(STATUS "CLion env detected")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

find_package(ament_cmake REQUIRED)


if (DEFINED ENV{CLION_IDE})

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

install(DIRECTORY
        launch
        config
        DESTINATION share/${PROJECT_NAME}
)

ament_package()