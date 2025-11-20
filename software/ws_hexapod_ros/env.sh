#!/bin/bash
#export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export RCUTILS_LOGGING_SEVERITY=DEBUG
source /opt/ros/$ROS_DISTRO/setup.sh
export ROS_DOMAIN_ID=1
source "$(dirname "$0")/install/setup.sh"