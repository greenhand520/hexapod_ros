# hexapod_test_ubtech_servo

## 简介

这个功能包是用来测试`hexapod_ubtech_ros2_control`中对 UBTECH 串口舵机协议的实现。节点名是`hexapod_test_ubtech_servo_node`，对应的可执行文件是`hexapod_test_ubtech_servo_exe`。

## 启动

1、控制台启动

```bash
# 使用config下的hexapod_test_ubtech_servo_node.yaml配置文件
ros2 launch hexapod_test_ubtech_servo start_node.launch.py

# 或者使用覆盖参数
ros2 launch hexapod_test_ubtech_servo start_node.launch.py \
    serial_device:=/dev/ttyAMA0 \
    baud_rate:=9600 \
    tx_en_line:=17

```

2、在别的 Launch 文件中被调用

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
        # 指向 bringup 包自己的配置文件
        'config_file': PathJoinSubstitution([
            FindPackageShare('robot_bringup'),
            'config', 'hexapod_test_ubtech_servo_node.yaml'
        ]),
    }.items(),
)
```

