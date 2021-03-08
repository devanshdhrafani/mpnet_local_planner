#include <Controller.h>

int main(int argc, char **argv)
{
		ros::init(argc, argv, "controller");

		ros::NodeHandle n;
        // For simulation use: 
        mpnet_local_planner::OdometryHelperRos odom_helper_("/pf/pose/odom");
        
        // For real-world use:
        // mpnet_local_planner::OdometryHelperRos odom_helper_("/robot_pose_ekf/odom_ekf_topic");
        
		mpnet_local_planner::Controller controller(false);
        geometry_msgs::PoseStamped robot_vel;
        ackermann_msgs::AckermannDriveStamped control_msg;
        nav_msgs::Odometry base_odom;
		ros::Subscriber path = n.subscribe("/move_base/MpnetLocalPlanner/local_plan", 2, &mpnet_local_planner::Controller::get_path, &controller);
		// ros::Publisher control = n.advertise<ackermann_msgs::AckermannDriveStamped>("/vesc/high_level/ackermann_cmd_mux/input/nav_0", 10);
        ros::Publisher control = n.advertise<ackermann_msgs::AckermannDriveStamped>("/vesc/ackermann_cmd_mux/input/navigation", 10);
        ros::ServiceServer resetController = n.advertiseService("reset_controller", &mpnet_local_planner::Controller::resetController, &controller);
        ros::Rate loop_rate(20);
        ros::spinOnce();
		while (n.ok())
		{       
            odom_helper_.getRobotVel(robot_vel);
            odom_helper_.getOdom(base_odom);
            controller.observe(robot_vel, base_odom);
            // controller.control_cmd_vel(cmd_vel);
            controller.control(control_msg);
            control.publish(control_msg);
            ros::spinOnce();
            loop_rate.sleep();
		}
	ros::spin();

	return 0;
}