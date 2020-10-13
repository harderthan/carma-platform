/*
 * Copyright (C) 2019-2020 LEIDOS.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include <ros/ros.h>
#include <string>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <algorithm>
#include <tf/transform_datatypes.h>
#include "inlanecruising_plugin.h"


namespace inlanecruising_plugin
{
    InLaneCruisingPlugin::InLaneCruisingPlugin() :
                    current_speed_(0.0),
                    trajectory_time_length_(6.0),
                    trajectory_point_spacing_(0.1) {}

    void InLaneCruisingPlugin::initialize()
    {
        nh_.reset(new ros::CARMANodeHandle());
        pnh_.reset(new ros::CARMANodeHandle("~"));
        
        trajectory_srv_ = nh_->advertiseService("plugins/InLaneCruisingPlugin/plan_trajectory", &InLaneCruisingPlugin::plan_trajectory_cb, this);
        base_waypoints_pub_ = nh_->advertise<autoware_msgs::Lane>("plugin_base_waypoints", 1);                

        inlanecruising_plugin_discovery_pub_ = nh_->advertise<cav_msgs::Plugin>("plugin_discovery", 1);
        plugin_discovery_msg_.name = "InLaneCruisingPlugin";
        plugin_discovery_msg_.versionId = "v1.0";
        plugin_discovery_msg_.available = true;
        plugin_discovery_msg_.activated = false;
        plugin_discovery_msg_.type = cav_msgs::Plugin::TACTICAL;
        plugin_discovery_msg_.capability = "tactical_plan/plan_trajectory";

        waypoints_sub_ = nh_->subscribe("final_waypoints", 1, &InLaneCruisingPlugin::waypoints_cb, this);
        pose_sub_ = nh_->subscribe("current_pose", 1, &InLaneCruisingPlugin::pose_cb, this);
        twist_sub_ = nh_->subscribe("current_velocity", 1, &InLaneCruisingPlugin::twist_cd, this);
        pnh_->param<double>("trajectory_time_length", trajectory_time_length_, 6.0);
        pnh_->param<double>("trajectory_point_spacing", trajectory_point_spacing_, 0.1);

        ros::CARMANodeHandle::setSpinCallback([this]() -> bool {
            inlanecruising_plugin_discovery_pub_.publish(plugin_discovery_msg_);
            return true;
        });
    }


    void InLaneCruisingPlugin::run()
    {
        initialize();
        ros::CARMANodeHandle::spin();
    }



    bool InLaneCruisingPlugin::plan_trajectory_cb(cav_srvs::PlanTrajectoryRequest &req, cav_srvs::PlanTrajectoryResponse &resp){

        ROS_WARN_STREAM("PlanTrajectory");
        cav_msgs::TrajectoryPlan trajectory;
        trajectory.header.frame_id = "map";
        trajectory.header.stamp = ros::Time::now();
        trajectory.trajectory_id = boost::uuids::to_string(boost::uuids::random_generator()());
        ROS_WARN_STREAM("1");
        trajectory.trajectory_points = compose_trajectory_from_waypoints(waypoints_list);
        ROS_WARN_STREAM("2");
        trajectory_msg = trajectory;

        resp.trajectory_plan = trajectory_msg;
        resp.related_maneuvers.push_back(cav_msgs::Maneuver::LANE_FOLLOWING);
        resp.maneuver_status.push_back(cav_srvs::PlanTrajectory::Response::MANEUVER_IN_PROGRESS);

        ROS_WARN_STREAM("3");

        return true;
    }

    Point2DRTree InLaneCruisingPlugin::set_waypoints(const std::vector<autoware_msgs::Waypoint>& waypoints)
    {
        // guaranteed to get non-empty waypoints due to the protection in the callback function
        waypoints_list = waypoints;
        
        Point2DRTree empty_rtree;
        rtree = empty_rtree; // Overwrite the existing RTree

        ROS_WARN_STREAM("5");

        size_t index = 0;
        ROS_WARN_STREAM("6");
        for (auto wp : waypoints_list) {
            ROS_WARN_STREAM("7");
            Boost2DPoint p(wp.pose.pose.position.x, wp.pose.pose.position.y);
            ROS_WARN_STREAM("8");
            rtree.insert(std::make_pair(p, index));
            ROS_WARN_STREAM("9");
            index++;
        }
        return rtree; // TODO make return meaningful
    }

    size_t getNearestWaypointIndex(const Point2DRTree& rtree, const geometry_msgs::PoseStamped& pose_msg) {
        Boost2DPoint vehicle_point(pose_msg.pose.position.x, pose_msg.pose.position.y);
        std::vector<PointIndexPair> nearest_points;
        ROS_WARN_STREAM("16");
        rtree.query(boost::geometry::index::nearest(vehicle_point, 1), std::back_inserter(nearest_points));

        ROS_WARN_STREAM("17");
        if (nearest_points.size() == 0) {
            ROS_ERROR_STREAM("Failed to find nearest waypoint");
            return -1;
        }

        ROS_WARN_STREAM("18");

        // Get waypoints from nearest waypoint to time boundary
        return std::get<1>(nearest_points[0]);
    }

    void InLaneCruisingPlugin::waypoints_cb(const autoware_msgs::LaneConstPtr& msg)
    {
        if(msg->waypoints.size() == 0)
        {
            ROS_WARN_STREAM("Received an empty trajectory!");
            return;
        }
        set_waypoints(msg->waypoints);
    }

    void InLaneCruisingPlugin::pose_cb(const geometry_msgs::PoseStampedConstPtr& msg)
    {
        pose_msg_ = msg;
    }

    void InLaneCruisingPlugin::twist_cd(const geometry_msgs::TwistStampedConstPtr& msg)
    {
        current_speed_ = msg->twist.linear.x;
    }

    std::vector<autoware_msgs::Waypoint> get_back_waypoints(const std::vector<autoware_msgs::Waypoint>& waypoints, size_t index, size_t mpc_back_waypoints_num) {
        std::vector<autoware_msgs::Waypoint> back_waypoints;
        size_t min_index = 0;
        if (mpc_back_waypoints_num < index) {
            min_index = index - mpc_back_waypoints_num;
        } 

        for (int i = index; i >= min_index; i--) {
            back_waypoints.push_back(waypoints[i]);
        }

        std::reverse(back_waypoints.begin(), back_waypoints.end());
        return back_waypoints;
    }

    std::vector<lanelet::BasicPoint2d> waypointsToBasicPoints(const std::vector<autoware_msgs::Waypoint>& waypoints) {
        std::vector<lanelet::BasicPoint2d> basic_points;
        for (auto wp : waypoints) {
            lanelet::BasicPoint2d pt(wp.pose.pose.position.x, wp.pose.pose.position.y);
            basic_points.push_back(pt);
        }

        return basic_points;
    }

    double compute_speed_for_curvature(double curvature, double lateral_accel_limit)
    {
        // Check at compile time for infinity availability
        static_assert(std::numeric_limits<double>::has_infinity, "This code requires compilation using a system that supports IEEE 754 for access to positive infinity values");

        // Solve a = v^2/r (k = 1/r) for v
        // a = v^2 * k
        // a / k = v^2
        // v = sqrt(a / k)
        
        if (fabs(curvature) < 0.00000001) { // Check for curvature of 0.
            return std::numeric_limits<double>::infinity();
        }
        return std::sqrt(fabs(lateral_accel_limit / curvature));
    }
    std::vector<double> compute_ideal_speeds(std::vector<double> curvatures,
                                                                double lateral_accel_limit)
    {
        std::vector<double> out;
        for (double k : curvatures)
        {
            out.push_back(compute_speed_for_curvature(k, lateral_accel_limit));
        }

        return out;
    }

    std::vector<double> apply_speed_limits(const std::vector<double> speeds,
                                                            const std::vector<double> speed_limits)
    {
        ROS_ERROR_STREAM("Speeds list size: " << speeds.size());
        ROS_ERROR_STREAM("SpeedLimits list size: " << speed_limits.size());
        if (speeds.size() != speed_limits.size())
        {
            throw std::invalid_argument("Speeds and speed limit lists not same size");
        }
        std::vector<double> out;
        for (int i = 0; i < speeds.size(); i++)
        {
            out.push_back(std::min(speeds[i], speed_limits[i]));
        }

        return out;
    }

    std::vector<cav_msgs::TrajectoryPlanPoint> InLaneCruisingPlugin::compose_trajectory_from_waypoints(const std::vector<autoware_msgs::Waypoint>& waypoints)
    {
        std::vector<cav_msgs::TrajectoryPlanPoint> final_trajectory;
        size_t nearest_pt_index = getNearestWaypointIndex(rtree, *pose_msg_);

        if (nearest_pt_index == -1) {
            ROS_ERROR_STREAM("Nearest waypoint not found");
            return final_trajectory;
        }

        std::vector<autoware_msgs::Waypoint> future_waypoints(waypoints.begin() + nearest_pt_index + 1, waypoints.end());
        std::vector<autoware_msgs::Waypoint> time_bound_waypoints = get_waypoints_in_time_boundary(future_waypoints, trajectory_time_length_);
        std::vector<autoware_msgs::Waypoint> previous_waypoints = get_back_waypoints(waypoints, nearest_pt_index, mpc_back_waypoints_num_);
        std::vector<autoware_msgs::Waypoint> combined_waypoints(previous_waypoints.begin(), previous_waypoints.end());

        size_t new_nearest_wp_index = combined_waypoints.size() - 1;

        combined_waypoints.insert( combined_waypoints.end(), time_bound_waypoints.begin(), time_bound_waypoints.end());

        std::vector<lanelet::BasicPoint2d> basic_points = waypointsToBasicPoints(combined_waypoints);

        //auto curve = carma_wm::geometry::compute_fit(basic_points); // Returned data type TBD
        std::vector<double> sampling_points;
        for (auto wp : combined_waypoints) {
            sampling_points.push_back(wp.pose.pose.position.x);
        }

        std::vector<double> yaw_values;// = carma_wm::geometry::compute_orientations_from_fit(curve, sampling_points);
        std::vector<double> curvatures; //carma_wm::geometry::compute_curvatures_from_fit(curve, sampling_points);

        std::vector<double> speed_limits(curvatures.size(), 6.7056); // TODO use lanelets to get these values
        std::vector<double> ideal_speeds = compute_ideal_speeds(curvatures, 1.5);
        std::vector<double> actual_speeds = apply_speed_limits(ideal_speeds, speed_limits);


        // Apply new values to combined waypoint set
        int i = 0;
        for (auto& wp : combined_waypoints) {
            wp.twist.twist.linear.x = actual_speeds[i];
            wp.pose.pose.orientation = tf::createQuaternionMsgFromYaw(yaw_values[i]);
            i++;
        }

        autoware_msgs::Lane lane;
        lane.lane_id = 1;

        std_msgs::Header header;
        header.frame_id = "map";
        header.seq = 0;
        header.stamp = ros::Time::now();
        lane.header = header;
        lane.waypoints = combined_waypoints;

        base_waypoints_pub_.publish(lane);
        
        std::vector<autoware_msgs::Waypoint> final_waypoints(combined_waypoints.begin() + new_nearest_wp_index + 1, combined_waypoints.end());
        std::vector<cav_msgs::TrajectoryPlanPoint> uneven_traj = create_uneven_trajectory_from_waypoints(final_waypoints);
        final_trajectory = post_process_traj_points(uneven_traj);


        return final_trajectory;
    }

// TODO comments: Takes in a waypoint list that is from the next waypoint till the time boundary
    std::vector<cav_msgs::TrajectoryPlanPoint> InLaneCruisingPlugin::create_uneven_trajectory_from_waypoints(const std::vector<autoware_msgs::Waypoint>& waypoints)
    {
        std::vector<cav_msgs::TrajectoryPlanPoint> uneven_traj;
        // TODO land id is not populated because we are not using it in Autoware
        // Adding current vehicle location as the first trajectory point if it is not on the first waypoint
        // TODO there is an equivalent loop to this in the platooning plugin that should also be updated to assign the orientation value
        // Add vehicle location as first point
        cav_msgs::TrajectoryPlanPoint starting_point;
        starting_point.target_time = ros::Time(0.0);
        starting_point.x = pose_msg_->pose.position.x;
        starting_point.y = pose_msg_->pose.position.y;
        double roll,pitch,yaw;
        carma_wm::geometry::rpyFromQuaternion(pose_msg_->pose.orientation, roll, pitch, yaw);
        starting_point.yaw = yaw;
        uneven_traj.push_back(starting_point);

        if (waypoints.size() == 0) {
            ROS_ERROR_STREAM("Trying to create uneven trajectory from 0 waypoints");
            return uneven_traj;
        }
        // Update previous wp
        double previous_wp_v = waypoints[0].twist.twist.linear.x;
        double previous_wp_x = starting_point.x;
        double previous_wp_y = starting_point.y;
        double previous_wp_yaw = starting_point.yaw;
        ros::Time previous_wp_t = starting_point.target_time;

        ROS_WARN_STREAM("previous_wp_v" << previous_wp_v);

        for(int i = 0; i < waypoints.size(); i++)
        {


            cav_msgs::TrajectoryPlanPoint traj_point;
            // assume the vehicle is starting from stationary state because it is the same assumption made by pure pursuit wrapper node
            double average_speed = std::max(previous_wp_v, 2.2352); // TODO need better solution for this
            double delta_d = sqrt(pow(waypoints[i].pose.pose.position.x - previous_wp_x, 2) + pow(waypoints[i].pose.pose.position.y - previous_wp_y, 2));
            ros::Duration delta_t(delta_d / average_speed);
            traj_point.target_time = previous_wp_t + delta_t;
            traj_point.x = waypoints[i].pose.pose.position.x;
            traj_point.y = waypoints[i].pose.pose.position.y;
            carma_wm::geometry::rpyFromQuaternion(waypoints[i].pose.pose.orientation, roll, pitch, yaw);
            traj_point.yaw = yaw;
            uneven_traj.push_back(traj_point);

            previous_wp_v = waypoints[i].twist.twist.linear.x;
            previous_wp_x = uneven_traj.back().x;
            previous_wp_y = uneven_traj.back().y;
            previous_wp_y = uneven_traj.back().y;
            previous_wp_t = uneven_traj.back().target_time;
        }

        return uneven_traj;
    }

    // TODO comments: This method takes in all waypoints from the nearest waypoint + 1 and returns all waypoints in that set that fit within the time boundary
    std::vector<autoware_msgs::Waypoint> InLaneCruisingPlugin::get_waypoints_in_time_boundary(const std::vector<autoware_msgs::Waypoint>& waypoints, double time_span)
    {
        // Find nearest waypoint
        ROS_WARN_STREAM("15");
        std::vector<autoware_msgs::Waypoint> sublist;

        double total_time = 0.0;
        for(int i = 0; i < waypoints.size(); ++i) // 
        {
            sublist.push_back(waypoints[i]);
            if(i == 0)
            {
                ROS_WARN_STREAM("21");
                continue;
            }
            ROS_WARN_STREAM("20");
            double delta_x_square = pow(waypoints[i].pose.pose.position.x - waypoints[i - 1].pose.pose.position.x, 2);
            double delta_y_square = pow(waypoints[i].pose.pose.position.y - waypoints[i - 1].pose.pose.position.y, 2);
            //double delta_z_square = waypoints[i].pose.pose.position.z - waypoints[i - 1].pose.pose.position.z;
            // Here we ignore z attribute because it is not used by Autoware
            double delta_d = sqrt(delta_x_square + delta_y_square);
            double average_v = 0.5 * (waypoints[i].twist.twist.linear.x + waypoints[i - 1].twist.twist.linear.x);
            double delta_t = delta_d / average_v;
            total_time += delta_t;
            if(total_time >= time_span)
            {
                break;
            }
        }
        return sublist;
    }

    std::vector<cav_msgs::TrajectoryPlanPoint> InLaneCruisingPlugin::post_process_traj_points(std::vector<cav_msgs::TrajectoryPlanPoint> trajectory)
    {
        ros::Time now = ros::Time::now();
        ros::Duration now_duration(now.sec, now.nsec);
        for(int i = 0; i < trajectory.size(); i++)
        {
            trajectory[i].controller_plugin_name = "default";
            trajectory[i].planner_plugin_name = "autoware";
            trajectory[i].target_time += now_duration;
        }

        return trajectory;
    }
}