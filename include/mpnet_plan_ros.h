/** 
 * Defines the class for mpnet local planner
 */
#include <memory>
#include <torch/script.h>

#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Polygon.h>
#include <geometry_msgs/PolygonStamped.h>
#include <geometry_msgs/Point.h>

#include <tf2_ros/buffer.h>

#include <costmap_2d/costmap_2d_ros.h>
#include <costmap_2d/costmap_2d.h>

#include <base_local_planner/world_model.h>
#include <base_local_planner/costmap_model.h>

#include <nav_core/base_local_planner.h>

#include <mpnet_plan.h>
#include <ompl/base/ScopedState.h>
#include <ompl/base/spaces/DubinsStateSpace.h>
#include <ompl/geometric/SimpleSetup.h>

#include <odometry_helper_ros.h>
#include <Controller.h>

#include <costmap_2d/footprint.h>

namespace ob = ompl::base;
namespace og = ompl::geometric;

namespace mpnet_local_planner{

    class MpnetLocalPlanner: public nav_core::BaseLocalPlanner{
        /**
         * @class MpnetLocalPlanner
         * @brief Provides an interface for the mpnet local planner, with a model precitive controller
         */

        public:
            /**
             * @brief Compute the velocity commmand to send to the base
             */
            bool computeVelocityCommands(geometry_msgs::Twist& cmd_vel);

            /**
             * @brief Check if the goal pose has been achieved by the local planner
             */
            bool isGoalReached();
            
            /**
             * @brief Set the plan that the local planner is following
             * @param plan The plan to pass to the local planner
             * @return True if the plan was updated successfully, false otherwise
             */
            bool setPlan(const std::vector<geometry_msgs::PoseStamped>& plan);

            /**
             * @brief Constructs the local planner
             * @param name The name given to this instance of the local planner
             * @param tf A pointer to a transform listener
             * @param costmap_ros The costmap map to use for assigning costs to local plans
             */
            void initialize(std::string name, tf2_ros::Buffer* tf, costmap_2d::Costmap2DROS* costmap_ros);
            
            /**
             * @brief Check the initialization state of the controller
             * @returns True it the planner is initialized
             */
            bool isInitialized()
            {
                return initialized_;
            }

            /**
             * @brief Bundle the current pose and target pose to a normalized torch tensor
             * @param start The current position of the robot
             * @param goal The goal to be achieved in the local costmap
             * @param origin_x The x co-ordinate of the origin of the local costmap
             * @param origin_y The y co-ordinate of the origin of the local costmap
             */
            torch::Tensor getInputTensor(const ob::ScopedState<> &start, const ob::ScopedState<> &goal, double origin_x, double origin_y);

            /**
             * @brief Constructs the ros wrapper
             * @param name The name to give this instance of the planner
             * @param tf A pointer to the transform listener
             * @param costmap The costmap to use for trajectory planning
             */
            MpnetLocalPlanner(std::string name, tf2_ros::Buffer* tf, costmap_2d::Costmap2DROS* costmap_ros);
            
            double distanceBetweenPoints(geometry_msgs::PoseStamped from, geometry_msgs::PoseStamped to);
            /**
             * @brief A function to prune the local plan
             * @param global_pose
             * @param plan
             */
            void pruneLocalPlan(const geometry_msgs::PoseStamped& global_pose, std::vector<geometry_msgs::PoseStamped>& plan);

            /**
             * Destructor for the interface
             */
            ~MpnetLocalPlanner();

            MpnetLocalPlanner();

            
        private:
            tf2_ros::Buffer* tf_;
            costmap_2d::Costmap2DROS* navigation_costmap_ros_;
            costmap_2d::Costmap2D* costmap_; /** @brief A costmap mpnet will use */
            mpnet_local_planner::MpnetPlanner* tc_; /** @brief The model to used to get local paths from mpne */
            bool initialized_;
            std::string global_frame_; /** @brief The frame in which the controller will run */
            std::string robot_base_frame_; /** @brief Used as the base frame id of the robot */
            ros::Publisher g_plan_pub_, l_plan_pub_; /** @brief A publisher to print the local plan generated by mpnet*/
            ros::ServiceClient resetController;
            ros::Publisher goal_footprint_pub;
            ros::Publisher footprintPolygon;
            double xy_goal_tolerance, yaw_goal_tolerance;
            bool reached_goal_;
            bool prune_plan_;
            
            bool valid_local_path;
            base_local_planner::Trajectory path;
            std::vector<geometry_msgs::PoseStamped> global_plan_;
            std::vector<geometry_msgs::PoseStamped> local_plan;
            geometry_msgs::PoseStamped prev_goal;

            std::vector<geometry_msgs::Point> robot_footprint;
            std::vector<geometry_msgs::Point> goal_region_footprint;
            geometry_msgs::PoseStamped prev_goal_point;
            bool set_prev_goal;
            // Controller controller;
            OdometryHelperRos odom_helper_;
            int plan_freq, plan_freq_count;
    };
}