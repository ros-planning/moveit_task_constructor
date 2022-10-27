/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, Bielefeld University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Bielefeld University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Authors: Michael Goerner, Robert Haschke
   Desc:    generate and validate a straight-line Cartesian path
*/

#pragma once

#include <moveit/task_constructor/solvers/planner_interface.h>

namespace moveit {
namespace task_constructor {
namespace solvers {

MOVEIT_CLASS_FORWARD(CartesianPath);

/** Use MoveIt's computeCartesianPath() to generate a straight-line path between two scenes. */
class CartesianPath : public PlannerInterface
{
public:
	CartesianPath();

	void setStepSize(double step_size) { setProperty("step_size", step_size); }
	void setJumpThreshold(double jump_threshold) { setProperty("jump_threshold", jump_threshold); }
	void setMinFraction(double min_fraction) { setProperty("min_fraction", min_fraction); }

	void setMaxVelocityScaling(double factor) { setProperty("max_velocity_scaling_factor", factor); }
	void setMaxAccelerationScaling(double factor) { setProperty("max_acceleration_scaling_factor", factor); }

	void init(const moveit::core::RobotModelConstPtr& robot_model) override;

	bool plan(const planning_scene::PlanningSceneConstPtr& from, const planning_scene::PlanningSceneConstPtr& to,
	          const moveit::core::JointModelGroup* jmg, double timeout, robot_trajectory::RobotTrajectoryPtr& result,
	          const moveit_msgs::msg::Constraints& path_constraints = moveit_msgs::msg::Constraints()) override;

	bool plan(const planning_scene::PlanningSceneConstPtr& from, const moveit::core::LinkModel& link,
	          const Eigen::Isometry3d& target, const moveit::core::JointModelGroup* jmg, double timeout,
	          robot_trajectory::RobotTrajectoryPtr& result,
	          const moveit_msgs::msg::Constraints& path_constraints = moveit_msgs::msg::Constraints()) override;
};
}  // namespace solvers
}  // namespace task_constructor
}  // namespace moveit
