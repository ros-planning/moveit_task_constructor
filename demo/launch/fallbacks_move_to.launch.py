from launch import LaunchDescription
from launch_ros.actions import Node
from moveit_configs_utils import MoveItConfigsBuilder


def generate_launch_description():
    moveit_config = (
        MoveItConfigsBuilder("moveit_resources_panda")
        .planning_pipelines(pipelines=["ompl", "pilz_industrial_motion_planner"])
        .to_moveit_configs()
    )

    print(moveit_config.planning_pipelines)
    fallbacks_move_to_task = Node(
        package="moveit_task_constructor_demo",
        executable="fallbacks_move_to",
        output="screen",
        parameters=[
            moveit_config.cartesian_limits,
            moveit_config.joint_limits,
            moveit_config.planning_pipelines,
            moveit_config.robot_description,
            moveit_config.robot_description_kinematics,
            moveit_config.robot_description_semantic,
        ],
    )

    return LaunchDescription([fallbacks_move_to_task])
