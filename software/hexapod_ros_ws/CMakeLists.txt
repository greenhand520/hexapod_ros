cmake_minimum_required(VERSION 3.14)
project("hexapod_ros_ws")

# check the GCC version
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # get the GCC version
    execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
            OUTPUT_VARIABLE GCC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # convert to digital format（e.g. "13.2.1" -> 13.2.1）
    string(REGEX REPLACE "^([0-9]+).*" "\\1" GCC_MAJOR_VERSION "${GCC_VERSION}")

    # check if the version is 13
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND GCC_MAJOR_VERSION LESS 13)
        message("\n===========================================")
        message("  ERROR: Incompatible GCC version")
        message("  Detected: GCC ${GCC_VERSION}")
        message("  Required: GCC 13 or higher")
        message("===========================================")
        message("Solutions:")
        message("  1. Install GCC 13+:(Ubuntu)")
        message("     sudo apt update")
        message("     sudo apt install software-properties-common")
        message("     sudo add-apt-repository ppa:ubuntu-toolchain-r/test")
        message("     sudo apt update")
        message("     sudo apt install gcc-13 g++-13")
        message("  2. Set compiler explicitly:")
        message("     cmake -DCMAKE_CXX_COMPILER=g++-13 ..")
        message("===========================================\n")
        message(FATAL_ERROR "")
    endif ()
endif ()

include("/opt/ros/scripts/cmake/colcon.cmake")

# only for clion highlighting and analysis
colcon_add_subdirectories(
        BUILD_BASE "${PROJECT_SOURCE_DIR}/build"
        BASE_PATHS "${PROJECT_SOURCE_DIR}/src/"
        # --packages-select
        # --packages-select demo_nodes_cpp
)
