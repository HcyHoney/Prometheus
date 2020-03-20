#include "apf.h"

namespace local_planner
{

void APF::set_local_map(sensor_msgs::PointCloud2ConstPtr &local_map_ptr){
    local_map_ptr_ = local_map_ptr;
    ros::Time begin_load_point_cloud = ros::Time::now();
    ROS_INFO("--- SDFMAP_GLOBAL: have pcl, begin ---");
    pcl::fromROSMsg(*local_map_ptr, latest_local_pcl_);

    begin_update_map = ros::Time::now();
    has_local_map_=true;
}

void APF::set_odom(nav_msgs::Odometry cur_odom){
    cur_odom_ = cur_odom;
}


bool APF::compute_force(Eigen::Matrix<double, 3, 1> &goal, Eigen::Matrix<double, 3, 1> current_odom, Eigen::Vector3d &desired_vel){
    if(!has_local_map_)
        return false;

    if ((int)latest_local_pcl_.points.size() == 0) 
        return false;

    if (isnan(current_odom(0)) || isnan(current_odom(1)) || isnan(current_odom(2)))
        return false;

    if (isnan(goal(0)) || isnan(goal(1)) || isnan(goal(2)))
        return false;

    vector<Eigen::Vector3d> obstacles;
    pcl::PointXYZ pt;
    Eigen::Vector3d p3d;
    ros::Time begin_collision = ros::Time::now();
    ROS_INFO("--- SDFMAP_GLABAL: begin collision_map, time: %f,  obs_distance: %f, min_dist: %f", begin_collision.toSec()-begin_update_map.toSec(), 
                                                                                                                                                                                                        obs_distance, min_dist);
    ROS_INFO("point size: %d", latest_local_pcl_.points.size());

    // 引力
    double dist_att = (goal - current_odom).norm();
    if(dist_att > max_att_dist){
        attractive_force = k_att * max_att_dist * (goal - current_odom)/dist_att;
    }else
    {
        attractive_force = k_att * (goal - current_odom);
    }

    // 排斥力
    double uav_height = cur_odom_.pose.pose.position.z;
    push_force = Eigen::Vector3d(0.0, 0, 0);
    for (size_t i = 0; i < latest_local_pcl_.points.size(); ++i) {
        pt = latest_local_pcl_.points[i];
        p3d(0) = pt.x, p3d(1) = pt.y, p3d(2) = pt.z;
        

        Eigen::Matrix<double, 3,1> current_odom_local(0.0, 0,0);
        // Eigen::Matrix<double, 3,1> current_odom_local = current_odom;
        // remove the ground point 
        double point_height_global = uav_height+p3d(2);
        if(fabs(point_height_global)<ground_height)
            continue;

        double dist_push = (current_odom_local - p3d).norm();
        if(dist_push > obs_distance || isnan(dist_push))
            continue;

        if(dist_push < min_dist){
            // should be addressed carefully.
            printf("the distance is very dangerous, dist: %f\n", dist_push);
            dist_push = 0.25;
        }

        obstacles.push_back(p3d);
        double push_gain = k_push * (1/dist_push - 1/obs_distance)* 1.0/(dist_push * dist_push);

        if(dist_att<1.0){
            push_gain *= dist_att;  // to gaurantee to reach the goal.
        }
        std::cout<<"dist_push" << dist_push << std::endl;
        std::cout << "push_gain: " << push_gain << std::endl;
        std::cout << "p3d: " << p3d << std::endl;
        push_force += push_gain * (current_odom_local - p3d)/dist_push;
    }

    if(obstacles.size() != 0){
        printf("obstacle size: %d\n", obstacles.size());
        push_force=push_force/obstacles.size();
    }

    Eigen::Quaterniond cur_rotation_local_to_global(cur_odom_.pose.pose.orientation.w, 
                                                                                                            cur_odom_.pose.pose.orientation.x,  
                                                                                                            cur_odom_.pose.pose.orientation.y,  
                                                                                                            cur_odom_.pose.pose.orientation.z); 

    // printf("odom q:[%f, %f, %f, %f]\n", cur_odom_.pose.pose.orientation.w, 
    //                                                                                                         cur_odom_.pose.pose.orientation.x,  
    //                                                                                                         cur_odom_.pose.pose.orientation.y,  
    //                                                                                                         cur_odom_.pose.pose.orientation.z);

    Eigen::Matrix<double,3,3> rotation_mat_local_to_global = cur_rotation_local_to_global.toRotationMatrix();


    push_force = rotation_mat_local_to_global * push_force; 

    if(uav_height<ground_safe_height){
            printf("[compute_force]: near the ground, the height%f \n", uav_height);
            push_force = push_force + Eigen::Matrix<double, 3, 1>(0, 0, (ground_safe_height-uav_height)*3.0);
    }

    push_force = push_force;

    ROS_INFO("push force: [%f, %f, %f], attractive force: [%f, %f, %f], obs size: %d, obs_dis: %f, k_push: %f", push_force(0), push_force(1), 
    push_force(2), attractive_force(0), attractive_force(1), attractive_force(2), obstacles.size(), obs_distance, k_push);
    
    // 合力
    desired_vel = push_force + attractive_force;

    return true;
}

void APF::init(ros::NodeHandle& nh){
    has_local_map_ = false;

    nh.param("apf/obs_distance", obs_distance, 2.5);  // 感知障碍物距离
    nh.param("apf/k_push", k_push, 1.0);                         // 推力增益
    nh.param("apf/k_att", k_att, 0.4);                                  // 引力增益
    nh.param("apf/min_dist", min_dist, 0.3);                            // 最小壁障距离
    nh.param("apf/max_att_dist", max_att_dist, 5.0);             // 最大吸引距离
    nh.param("apf/ground_height", ground_height, 0.1);  // 地面高度
    nh.param("apf/ground_safe_height", ground_safe_height, 0.2);  // 地面安全距离
    
}


}