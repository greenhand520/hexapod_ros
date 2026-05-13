# hexapod_test_ubtech_servo

## Overview

This package is used to test the implementation of the UBTECH serial servo protocol in package `hexapod_ubtech_ros2_control`.

The node name is `hexapod_test_ubtech_servo_node`, and the corresponding executable is `hexapod_test_ubtech_servo_exe`.

## Launch

### 1. Launch from Console

```bash
# Use the hexapod_test_ubtech_servo_node.yaml configuration file under config
ros2 launch hexapod_test_ubtech_servo start_node.launch.py

# Or override parameters
ros2 launch hexapod_test_ubtech_servo start_node.launch.py \
    serial_device:=/dev/ttyAMA0 \
    baud_rate:=9600 \
    tx_en_line:=17
```

### 2. Included in Another Launch File

```python
# robot_bringup/launch/robot.launch.py

debug_node = IncludeLaunchDescription(
    PythonLaunchDescriptionSource(
        PathJoinSubstitution([
            FindPackageShare('hexapod_test_ubtech_servo'),
            'launch', 'start_node.launch.py'
        ])
    ),
    launch_arguments={
        # Point to the bringup package's own configuration file
        'config_file': PathJoinSubstitution([
            FindPackageShare('robot_bringup'),
            'config', 'hexapod_test_ubtech_servo_node.yaml'
        ]),
    }.items(),
)
```
