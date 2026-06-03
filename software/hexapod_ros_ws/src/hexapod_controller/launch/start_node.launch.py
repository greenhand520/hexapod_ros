from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import (
    LaunchConfiguration, PathJoinSubstitution
)
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():

    # 允许外部指定配置文件路径，否则用包内默认的
    config_file = LaunchConfiguration('config_file')

    DeclareLaunchArgument(
        'config_file',
        default_value=PathJoinSubstitution([
            FindPackageShare('hexapod_controller'),
            'config', 'hexapod_controller_node.yaml'
        ]),
        description='Path to debug node config yaml'
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            'config_file',
            default_value=PathJoinSubstitution([
                FindPackageShare('hexapod_controller'),
                'config', 'hexapod_controller_node.yaml'
            ]),
        ),

        Node(
            package='hexapod_controller',
            executable='hexapod_controller_exe',
            name='hexapod_controller_node',
            output='screen',
            parameters=[config_file],
        ),
    ])

