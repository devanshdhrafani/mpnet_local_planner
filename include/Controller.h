// for math
#include <cmath>

// for ros
#include "ros/ros.h"
#include "nav_msgs/Odometry.h"
#include <ackermann_msgs/AckermannDriveStamped.h>
#include "geometry_msgs/Pose.h"
#include <tf/transform_datatypes.h>
#include <ros/package.h>
#include "nav_msgs/Path.h"
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/Pose.h"
#include <base_local_planner/odometry_helper_ros.h>
#include <base_local_planner/trajectory.h>
#include <tf2/utils.h>

// for MPC
#include "MPC.h"
#include <cppad/cppad.hpp>

#include <vector>

// for readcsv
// #include "utils.h"
namespace mpnet_local_planner{

	class Controller
	{
	public:
		void observe(geometry_msgs::PoseStamped& robot_vel, nav_msgs::Odometry& base_odom);
		void get_path(base_local_planner::Trajectory& traj);
		// void control(geometry_msgs::Twist& cmd_vel);
		void control(ackermann_msgs::AckermannDriveStamped& _ackermann_msg);
		void control_cmd_vel(geometry_msgs::Twist& cmd_vel);
		bool verbose = true;
		// vector<double> path_x = {};
		// vector<double> path_y = {};
		std::vector<double> path_x = vector<double>(N);
		std::vector<double> path_y = vector<double>(N);
		std::vector<double> path_goal = vector<double>(2);
		Controller(){};
	private:
		MPC mpc;
		double x, y, th, vel, vth, a = 0, sta=0;
		int curr = 0;	
		// Eigen::VectorXd coeffs;

	};
}
