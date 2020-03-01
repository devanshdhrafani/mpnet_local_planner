/**
 * Describing different components of the local planner  
*/ 

#include <mpnet_plan_ros.h>

#include <base_local_planner/goal_functions.h>
#include <chrono> 

#include <nav_msgs/Path.h>

#include <tf2/utils.h>

#include <std_srvs/Empty.h>

#include <pluginlib/class_list_macros.h>

#include <costmap_2d/footprint.h>

// Register this planner
PLUGINLIB_EXPORT_CLASS(mpnet_local_planner::MpnetLocalPlanner, nav_core::BaseLocalPlanner)

namespace mpnet_local_planner{

    MpnetLocalPlanner::MpnetLocalPlanner():
    tf_(NULL),
    initialized_(false),
    navigation_costmap_ros_(NULL),
    odom_helper_("odom"),
    tc_(NULL),
    controller(false)
    {}
    
    MpnetLocalPlanner::MpnetLocalPlanner(std::string name, tf2_ros::Buffer* tf, costmap_2d::Costmap2DROS* costmap_ros):
    tf_(NULL),
    initialized_(false),
    navigation_costmap_ros_(NULL),
    odom_helper_("odom"),
    tc_(NULL),
    controller(false)
    {
        initialize(name, tf, costmap_ros);
    }

    MpnetLocalPlanner::~MpnetLocalPlanner()
    {        
        if (navigation_costmap_ros_!=NULL)
            delete navigation_costmap_ros_;

        if (tc_!=NULL)
            delete tc_;
    }

    void MpnetLocalPlanner::initialize(std::string name, tf2_ros::Buffer* tf, costmap_2d::Costmap2DROS* costmap_ros)
    {
        if(!isInitialized())
        {
            ros::NodeHandle private_nh("~/"+name);  
            l_plan_pub_ = private_nh.advertise<nav_msgs::Path>("local_plan", 1);
            g_plan_pub_ = private_nh.advertise<nav_msgs::Path>("global_plan", 1);
            footprintPolygon = private_nh.advertise<geometry_msgs::PolygonStamped>("robot_footprint",1);
            resetController = private_nh.serviceClient<std_srvs::Empty>("/reset_controller");

            navigation_costmap_ros_ = costmap_ros;
            costmap_ = navigation_costmap_ros_->getCostmap();
            tf_ = tf;
            // ---------Set parameters of the model ----------------
            // Load network model from reading the parameter file
            std::string file_name;
            double g_tolerance, yaw_tolerance;
            if (private_nh.getParam("model_file", file_name))
            {
                prune_plan_ = true;
                global_frame_ = costmap_ros->getGlobalFrameID();
                robot_base_frame_ = costmap_ros->getBaseFrameID();

                // Goal tolerance parameter
                private_nh.param("xy_goal_tolerance", g_tolerance, 0.1);
                private_nh.param("yaw_goal_tolerance", yaw_tolerance, 0.2);
                xy_goal_tolerance = g_tolerance;
                yaw_goal_tolerance = yaw_tolerance;

                // Planning parameters
                int numSamples, numPaths;
                private_nh.param("num_samples", numSamples, 4);
                private_nh.param("num_paths", numPaths, 2);

                // LoadRobotActualFootprints
                robot_footprint = costmap_2d::makeFootprintFromParams(private_nh);

                initialized_ = true;
                ROS_INFO("Initialized xy tolerance: %f ", xy_goal_tolerance);
                tc_ = new MpnetPlanner(
                    tf, 
                    navigation_costmap_ros_, 
                    file_name,
                    xy_goal_tolerance/2,
                    yaw_goal_tolerance,
                    numSamples,
                    numPaths,
                    robot_footprint
                    );
            }
            else
                ROS_ERROR("No model file specified, Did not initialize planner");            
        }
        else
        {
            ROS_WARN("mpnet_local_planner has already been initialized, doing nothing");
        }   
    }

    bool MpnetLocalPlanner::setPlan(const std::vector<geometry_msgs::PoseStamped>& orig_global_plan)
    {
        if (!isInitialized())
        {
            ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
            return false;
        }

        global_plan_.clear();
        local_plan.clear();
        path.resetPoints();
        std_srvs::Empty callController;
        if (resetController.call(callController))
            ROS_INFO("Reset the controller");
        else
            ROS_INFO("Was not able to reset the controller");
        global_plan_ = orig_global_plan;

        reached_goal_ = false;
        valid_local_path = false;
        return true;
    }

    bool MpnetLocalPlanner::computeVelocityCommands(geometry_msgs::Twist& cmd_vel)
    {
        if (!isInitialized())
        {
            ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
            return false;
        }
        geometry_msgs::PoseStamped global_pose;
        if (!navigation_costmap_ros_->getRobotPose(global_pose)){
            return false;
        }

        geometry_msgs::PolygonStamped oriented_footprint;
        oriented_footprint.header.frame_id = global_frame_;
        double yaw = tf2::getYaw(global_pose.pose.orientation);
        costmap_2d::transformFootprint(global_pose.pose.position.x, global_pose.pose.position.y, yaw, robot_footprint, oriented_footprint);
        footprintPolygon.publish(oriented_footprint);

        std::vector<geometry_msgs::PoseStamped> transformed_plan;
        if (!base_local_planner::transformGlobalPlan(*tf_, global_plan_, global_pose, *costmap_, global_frame_, transformed_plan))
        {
            ROS_WARN("Could not transform the global plan to the frame of the controller");
            return false;
        }

        if(prune_plan_)
            base_local_planner::prunePlan(global_pose, transformed_plan, global_plan_);

        geometry_msgs::PoseStamped drive_cmds;
        drive_cmds.header.frame_id = robot_base_frame_;


        if (transformed_plan.empty())
            return false;

        geometry_msgs::PoseStamped goal_point = transformed_plan.back();
        geometry_msgs::PoseStamped goal_point_minus=transformed_plan.end()[-2];

        // Calculate the distance of the vector
        double diff_x = goal_point.pose.position.x - goal_point_minus.pose.position.x;
        double diff_y = goal_point.pose.position.y - goal_point_minus.pose.position.y;

        double vec_len = sqrt(diff_x*diff_x + diff_y *diff_y);
        double angle = atan2(diff_y, diff_x);
        goal_point.pose.orientation.z = sin(angle/2);
        goal_point.pose.orientation.w = cos(angle/2);

        // Check to see goal tolerance
        double xydist_from_goal = std::hypot(goal_point.pose.position.x-global_pose.pose.position.x, goal_point.pose.position.y-global_pose.pose.position.y);
        double global_yaw = tf2::getYaw(global_pose.pose.orientation);
        double yaw_from_goal = angles::shortest_angular_distance(global_yaw, angle);
        // if (fabs(yaw_from_goal)<=yaw_goal_tolerance && xydist_from_goal<=xy_goal_tolerance)
        // ROS_INFO("goal tolerance: %f", xy_goal_tolerance);
        if (xydist_from_goal<=xy_goal_tolerance)
        {
            ROS_INFO("Reach Goal");
            cmd_vel.linear.x = 0.0;
            cmd_vel.linear.y = 0.0;
            cmd_vel.angular.z = 0.0;
            reached_goal_ = true;
            valid_local_path = false;
            return true;
        }
        else
        {
            valid_local_path = false;
            // TODO: Define the bound for space - THIS IS A HACK, need to add this as a class variable
            std::vector<double> spaceBound{6.0, 6.0, M_PI};
            base_local_planner::Trajectory new_path;
            auto start_time = std::chrono::high_resolution_clock::now();
            if (!tc_->isStateValid(global_pose))
            {
                ROS_INFO("Robot is in collision");
                path.resetPoints();
                local_plan.clear();
                return false;
            }
            tc_->getPath(global_pose, goal_point, spaceBound, new_path);
            auto stop_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time);
            // ROS_INFO("Time taken to Plan : %ld microseconds", duration.count());
            // ROS_INFO("Number of points in new path : %ud", new_path.getPointsSize());


            if (new_path.getPointsSize()>1) 
            {
                path = new_path;
                valid_local_path = true;
            }
            else
            {
                if (local_plan.size()<=1)
                {
                    // tc_->getPathRRT_star(global_pose, goal_point, new_path);
                    // Check if we can connect the new_path and old_path
                    if (new_path.getPointsSize()>1)
                    {
                        ROS_INFO("Path from RRT star");
                        path = new_path;
                        valid_local_path = true;
                    }
                    else
                    {
                        ROS_INFO("Did not find a path in the initial search");
                        return false;        
                    }
                }
            }
            // ROS_INFO("Number of points in local path: %lud", local_plan.size());
            if (!local_plan.empty())
                pruneLocalPlan(global_pose, local_plan);
            // ROS_INFO("Number of points in local path after pruning: %lud", local_plan.size());
            
            if (valid_local_path)
            {
                local_plan.clear();
                for (unsigned int i=0; i < path.getPointsSize(); i++)
                {
                    double p_x, p_y, p_th;
                    path.getPoint(i, p_x, p_y, p_th);
                    geometry_msgs::PoseStamped pose;
                    pose.header.frame_id = global_frame_;
                    pose.header.stamp = ros::Time::now();
                    pose.pose.position.x = p_x;
                    pose.pose.position.y = p_y;
                    pose.pose.position.z = 0.0;
                    tf2::Quaternion q;
                    q.setRPY(0, 0, p_th);
                    tf2::convert(q, pose.pose.orientation);
                    local_plan.push_back(pose);
                }
            }

        }
        // Publish information to the visualizer
        base_local_planner::publishPlan(transformed_plan, g_plan_pub_);
        base_local_planner::publishPlan(local_plan, l_plan_pub_);

        cmd_vel.linear.x = 0.0;
        cmd_vel.linear.y = 0.0;
        cmd_vel.angular.z = 0.0;

        return true;
    }

    bool MpnetLocalPlanner::isGoalReached(){
        if (!isInitialized())
        {
            ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
            return false;
        }
        return reached_goal_;
    }

    void MpnetLocalPlanner::pruneLocalPlan(const geometry_msgs::PoseStamped& global_pose, std::vector<geometry_msgs::PoseStamped>& plan)
    {
        std::vector<geometry_msgs::PoseStamped>::iterator it = plan.begin();
        while(it != plan.end()){
            const geometry_msgs::PoseStamped& w = *it;
            // Fixed error bound of 2 meters for now. Can reduce to a portion of the map size or based on the resolution
            double x_diff = global_pose.pose.position.x - w.pose.position.x;
            double y_diff = global_pose.pose.position.y - w.pose.position.y;
            double distance_sq = x_diff * x_diff + y_diff * y_diff;
            if(distance_sq < 0.25){
                ROS_DEBUG("Nearest waypoint to <%f, %f> is <%f, %f>\n", global_pose.pose.position.x, global_pose.pose.position.y, w.pose.position.x, w.pose.position.y);
                break;
            }
            it = plan.erase(it);
        }
    }
}