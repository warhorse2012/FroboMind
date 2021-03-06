#include <ros/ros.h>
#include <stdio.h>
#include "fmMsgs/odometry.h"
#include "fmMsgs/Vector3.h"
#include "math.h"
#include "fmMsgs/ypr.h"
#include "fmMsgs/kalman_output.h"
#include "fmMsgs/vehicle_coordinate.h"
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>


using namespace std;

bool start, left_updated, right_updated;
double x,y,th,vl,vr,vx,vy,vth,xr,xl, lxr, lxl, offset, kalman_th, odo_th, odom_x, odom_y;
ros::Time current_time, last_time;
double lengthBetweenTwoWheels = 0.39;
ros::Time right_time, right_last_time, left_time, left_last_time;

void right_callback(fmMsgs::odometry odo_msg_in)
{
	right_last_time = right_time;
	right_time = odo_msg_in.header.stamp;
	lxr = xr;
	xr = odo_msg_in.position;
	right_updated = true;
	return;
}

void left_callback(fmMsgs::odometry odo_msg_in)
{	
	left_last_time = left_time;
	left_time = odo_msg_in.header.stamp;
	lxl = xl;
	xl = odo_msg_in.position;
	left_updated = true;
	return;
}

void kalman_callback(fmMsgs::kalman_output msg)
{
	kalman_th = msg.yaw;
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "odometry_position_extractor");
	
	ros::NodeHandle h;
	ros::NodeHandle nh("~");

	std::string left_sub, right_sub, kalman_sub, odom_pub_top, coordi_pub_top, odom_th_pub_top;
	
	nh.param<std::string> ("Left_encoder_sub_top", left_sub, "/fmSensors/left_odometry");
	nh.param<std::string> ("Right_encoder_sub_top", right_sub, "/fmSensors/right_odometry");
	nh.param<std::string> ("Kalman_sub_top", kalman_sub, "/fmProcessors/Kalman_AngularVelocity");
	nh.param<std::string> ("Odom_pub_top", odom_pub_top, "xyz_position");
	nh.param<std::string> ("Coordi_pub_top", coordi_pub_top, "vehicle_coordinate");
	nh.param<std::string> ("Odom_th_pub_top", odom_th_pub_top, "/fmExtractors/odom_th");
	
	vl = vr = vy = vx = th = x = y = vth = xr = xl = lxl = lxr = kalman_th = odo_th = odom_x = odom_y = 0;
	left_last_time = right_last_time = ros::Time::now();
	start = true;
	left_updated = false;
	right_updated = false;

	ros::Subscriber sub_left = h.subscribe(left_sub, 1, left_callback);
	ros::Subscriber sub_right = h.subscribe(right_sub, 1, right_callback);
	ros::Subscriber sub_kalman = h.subscribe(kalman_sub, 1, kalman_callback);
	
	ros::Publisher odom_pub2 = h.advertise<nav_msgs::Odometry>("/odom", 50);
	tf::TransformBroadcaster odom_broadcaster;

   	fmMsgs::Vector3 pub_msg;
   	fmMsgs::Vector3 odom_th_pub_msg;
	fmMsgs::vehicle_coordinate coord_pub_msg;

	ros::Rate loop_rate(20);

        ros::Publisher odom_pub = h.advertise<fmMsgs::Vector3>(odom_pub_top, 1); 
        ros::Publisher odom_th_pub = h.advertise<fmMsgs::Vector3>(odom_th_pub_top, 1); 
        ros::Publisher coordi_pub = h.advertise<fmMsgs::vehicle_coordinate>(coordi_pub_top, 1); 

	while(h.ok()){

	    ros::spinOnce();
		
	    if (left_updated && right_updated)
	    {
			vl = (xl - lxl)/(left_time - left_last_time).toSec();
			vr = (xr - lxr)/(right_time - right_last_time).toSec();
			current_time = ros::Time::now();

			double dt = (current_time - last_time).toSec();

			vx = (vl+vr)/2;
			vy = 0;
			vth = (vr-vl)/lengthBetweenTwoWheels; //angular velocity in radian per second.

			//compute odometry in a typical way given the velocities of the robot

			double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
			double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
			double delta_th = (vth * dt);


			x += delta_x;
			y += delta_y;
			th = kalman_th;
			odo_th += delta_th;

			delta_x = (vx * cos(odo_th) - vy * sin(odo_th)) * dt;
			delta_y = (vx * sin(odo_th) + vy * cos(odo_th)) * dt;

			odom_y += delta_y;
			odom_x += delta_x;

			odom_th_pub_msg.y = odom_y;
			odom_th_pub_msg.x = odom_x;
			odom_th_pub_msg.th = odo_th;

			pub_msg.x = vx;
			pub_msg.y = vy;
			pub_msg.th = vth;
			pub_msg.header.stamp = ros::Time::now();

			coord_pub_msg.x = x;
			coord_pub_msg.y = y;
			coord_pub_msg.th = th;

			//since all odometry is 6DOF we'll need a quaternion created from yaw
			geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th);

			//first, we'll publish the transform over tf
			geometry_msgs::TransformStamped odom_trans;
			odom_trans.header.stamp = current_time;
			odom_trans.header.frame_id = "/odom";
			odom_trans.child_frame_id = "/base_link";

			odom_trans.transform.translation.x = x;
			odom_trans.transform.translation.y = y;
			odom_trans.transform.translation.z = 0.0;
			odom_trans.transform.rotation = odom_quat;

			//send the transform
			odom_broadcaster.sendTransform(odom_trans);

			//next, we'll publish the odometry message over ROS
			nav_msgs::Odometry odom;
			odom.header.stamp = current_time;
			odom.header.frame_id = "odom";

			//set the position
			odom.pose.pose.position.x = x;
			odom.pose.pose.position.y = y;
			odom.pose.pose.position.z = 0.0;
			odom.pose.pose.orientation = odom_quat;

			//set the velocity
			odom.child_frame_id = "base_link";
			odom.twist.twist.linear.x = vx;
			odom.twist.twist.linear.y = vy;
			odom.twist.twist.angular.z = vth;

			//publish the message
			odom_pub2.publish(odom);

			coordi_pub.publish(coord_pub_msg);

			odom_pub.publish(pub_msg);

			odom_th_pub.publish(odom_th_pub_msg);

			last_time = current_time;
			left_updated = false;
			right_updated = false;
	    }
	    
	    loop_rate.sleep();
		
	}
}
