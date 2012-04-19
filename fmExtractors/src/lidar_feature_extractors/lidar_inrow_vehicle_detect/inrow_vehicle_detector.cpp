/****************************************************************************
 # Inrow vehicle detector
 # Copyright (c) 2012 Jeppe Pedersen <jepe009@student.sdu.dk>
 #
 # Permission is hereby granted, free of charge, to any person obtaining a copy
 # of this software and associated documentation files (the "Software"), to deal
 # in the Software without restriction, including without limitation the rights
 # to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 # copies of the Software, and to permit persons to whom the Software is
 # furnished to do so, subject to the following conditions:
 #
 # The above copyright notice and this permission notice shall be included in
 # all copies or substantial portions of the Software.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 # IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 # AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 # OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 # THE SOFTWARE.
 #
 *****************************************************************************
 # File: inrow_vehicle_detector.cpp
 # Purpose: place vehicle in row.
 # Project: Field Robot Event 2012 - Warhorse
 # Author: Jeppe Pedersen <baronjeppe@gmail.com>
 # Created: Mar 15 2012, Jeppe Pedersen, Source written
 ****************************************************************************/

#include "inrow_vehicle_detector.h"

InRowVehicleDetector::InRowVehicleDetector()
{
	particlefilter = ParticleFilter(1000,0,0,0.75,0.375,M_PI/2,0,0.1,0.1);

	old_theta = 0;
	old_x = 0;
	old_y = 0;
}

void InRowVehicleDetector::positionCallback(const fmMsgs::Vector3::ConstPtr& position)
{
	x = position->x;
	y = position->y;
	theta = position->th;
}

void InRowVehicleDetector::processLaserScan(const sensor_msgs::LaserScan::ConstPtr& laser_scan )
{
	sensor_msgs::PointCloud cloud;

	try
    {
    	projector.projectLaser(*laser_scan, cloud);
    }
    catch (tf::TransformException& e)
    {
        std::cout << "Error: " << e.what();
        return;
    }

    cloud.header.frame_id = "base_link";
    point_cloud_pub.publish(cloud);

	fmMsgs::vehicle_position vp = particlefilter.update(cloud,dx,dy,dtheta);

	vehicle_position_pub.publish(vp);

	sensor_msgs::PointCloud cloud_rotated;

	for (int i = 0; i<cloud.points.size(); i++)
	{
		geometry_msgs::Point32 t;

		t.x = cloud.points[i].x * cos(vp.position.th) - cloud.points[i].y * sin(vp.position.th) + vp.position.x;
		t.y = cloud.points[i].x * sin(vp.position.th) + cloud.points[i].y * cos(vp.position.th) + vp.position.y;
		t.z = 0;

		cloud_rotated.points.push_back(t);
	}

    cloud_rotated.header.frame_id = "base_link";

    point_cloud_rotated_pub.publish(cloud_rotated);

    particlefilter.updateParticlesMarker();

	visualization_msgs::MarkerArray markerArray = particlefilter.getParticlesMarker();

	marker_pub.publish(markerArray);
}

