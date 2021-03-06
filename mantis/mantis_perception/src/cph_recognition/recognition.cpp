//mantis_object_recognition_node
//based on work done at UT Austin by Brian O'Neil
#include "ros/ros.h"

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/common/transforms.h>
#include <pcl/console/parse.h>
#include <pcl/console/print.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/extract_indices.h>
#include <sensor_msgs/point_cloud_conversion.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>

#include <iostream>
#include <fstream>
#include <math.h>
#include <string.h>

#include <visualization_msgs/Marker.h>

#include <boost/filesystem.hpp>

#include "cph.h"
#include "mantis_perception/mantis_recognition.h"
#include "nrg_object_recognition/recognition.h"
#include "tabletop_object_detector/Table.h"

ros::ServiceClient cph_client;
ros::ServiceClient region_client;
ros::Publisher rec_pub;
ros::Publisher vis_pub;
ros::Publisher noise_pub;


double _plug_pick_point_z;
double _enc_pick_point_z;
double _pvct_pick_point_z;
double _pvc_elbow_pick_point_z;
double _small_plug_pick_point_z;
double _pvct_1_x_offset;
double _pvct_1_y_offset;
double _pvct_1_z_offset;
double _enc_1_x_offset;
double _enc_1_y_offset;
double _enc_1_z_offset;
double _plug_1_x_offset;
double _plug_1_y_offset;
double _plug_1_z_offset;


bool rec_cb(mantis_perception::mantis_recognition::Request &main_request,
            mantis_perception::mantis_recognition::Response &main_response)
{  
  //Offset points from centroid of part to object training frame and thus center/grasp point
  float pvc_elbow_1_x_offset = 0.005878;//kinect_1=0.004949 asus_1=0.005878, avg=0.00541
  float pvc_elbow_1_y_offset = -0.00108;//kinect_1=-0.006247 asus_1=-0.00108, avg=-0.00366
  float pvc_elbow_1_z_offset = 0.02261;	//kinect_1=0.0182536 asus_1=0.02261, avg=0.02043
  float small_plug_x_offset = 0.002;
  float small_plug_y_offset = 0.00;
  float small_plug_z_offset = 0.038;	//55
  //Set vertical offset from part frame/center to pick frame/top of part
/*  std::string paramNamespace = "";
  paramNamespace = "/" + ros::this_node::getName();
  double _plug_pick_point_z;
  double _enc_pick_point_z;
  double _pvct_pick_point_z;
  double _pvc_elbow_pick_point_z;
  double _small_plug_pick_point_z;
  double _pvct_1_x_offset;
  double _pvct_1_y_offset;
  double _pvct_1_z_offset;
  double _enc_1_x_offset;
  double _enc_1_y_offset;
  double _enc_1_z_offset;
  double _plug_1_x_offset;
  double _plug_1_y_offset;
  double _plug_1_z_offset;
  ros::param::param(paramNamespace + "/plug_pick_point_z", _plug_pick_point_z, 0.048);
  ros::param::param(paramNamespace + "/enc_pick_point_z", _enc_pick_point_z, 0.051);
  ros::param::param(paramNamespace + "/pvct_pick_point_z", _pvct_pick_point_z, 0.025);
  ros::param::param(paramNamespace + "/pvc_elbow_pick_point_z", _pvc_elbow_pick_point_z, 0.030);
  ros::param::param(paramNamespace + "/small_plug_pick_point_z", _small_plug_pick_point_z, 0.045);
  ros::param::param(paramNamespace + "/plug_offset_x", _plug_1_x_offset,-0.0036);
  ros::param::param(paramNamespace + "/plug_offset_y", _plug_1_y_offset, 0.002);
  ros::param::param(paramNamespace + "/plug_offset_z", _plug_1_z_offset, 0.028);
  ros::param::param(paramNamespace + "/enclosure_offset_x", _enc_1_x_offset, 0.00563);
  ros::param::param(paramNamespace + "/enclosure_offset_y", _enc_1_y_offset, -0.0044);
  ros::param::param(paramNamespace + "/enclosure_offset_z", _enc_1_z_offset, 0.049);
  ros::param::param(paramNamespace + "/pvct_offset_x", _pvct_1_x_offset, 0.0034);
  ros::param::param(paramNamespace + "/pvct_offset_y", _pvct_1_y_offset, -0.0019);
  ros::param::param(paramNamespace + "/pvct_offset_z", _pvct_1_z_offset, 0.012);
*/
  bool use_region_growing = false;

  visualization_msgs::Marker make_marker(geometry_msgs::PoseStamped pick_pose, tf::Quaternion part_orientation);
  void publish_matching_PC(std::basic_string<char> label, nrg_object_recognition::pose pose,
		  tabletop_object_detector::Table table);

  ROS_INFO("Starting mantis recognition");
  ROS_INFO("Number of clusters received in request = %d", (int)main_request.clusters.size());
/*
  std::vector<sensor_msgs::PointCloud> received_cluster;
  std::vector<sensor_msgs::PointCloud2> r_clusters;
  sensor_msgs::PointCloud2 r_cluster;
  std::vector<pcl::PointCloud<pcl::PointXYZ> > seg_clouds;
  pcl::PointCloud<pcl::PointXYZ> seg_cloud;
  for (int j=0; j<main_request.clusters.size(); j++)
  {
	  received_cluster.push_back(main_request.clusters.at(j));
	  sensor_msgs::convertPointCloudToPointCloud2(received_cluster.at(j), r_cluster);
	  r_clusters.push_back(r_cluster);
	  pcl::fromROSMsg(r_clusters.at(j), seg_cloud);
	  seg_clouds.push_back(seg_cloud);
  }

  ROS_INFO_STREAM("Number of converted clusters for highest point evaluation: "<<seg_clouds.size());

  double meanz, sum, max_meanz=0.0, max_z=0.0, maxz;
  pcl::PointCloud<pcl::PointXYZ> highest_cluster;
  for (int k=0; k<seg_clouds.size(); k++)
  {
	meanz=0.0;
//FIND ABS MAX Z IN EACH CLUSTER AND TAKE CLUSTER WITH HIGHEST POINT INSTEAD OF MEANZ
	//maxz=0.0;
	//for(int i=0;i<seg_clouds.at(k).points.size();i++)
	//	{
	//		maxz=seg_clouds.at(k).points.at(i).z;
	//		if(maxz > max_z)
	//		{
	//			max_z= maxz;
	//			highest_cluster=seg_clouds.at(k);
	//			//ROS_INFO_STREAM("Highest cluster has index of "<<k);
	//		}
	//	}
	sum=0.0;
	for(int i=0;i<seg_clouds.at(k).points.size();i++)//temp_pc.points.size()
		{
			sum += seg_clouds.at(k).points.at(i).z;
		}
			ROS_INFO_STREAM("Sum of z points of  "<<k <<" cloud: "<< sum);
	meanz = sum/seg_clouds.at(k).points.size();
	ROS_INFO_STREAM("Average of z points of  "<<k <<" cloud: "<< meanz);
	if(meanz > max_meanz)
	{
		max_meanz = meanz;n
		highest_cluster=seg_clouds.at(k);
		ROS_INFO_STREAM("Highest cluster has index of "<<k);
	}

  }*/

  //convert segmentation results from array of PointCloud to single PointCloud2
  sensor_msgs::PointCloud received_clusters;
  received_clusters=main_request.clusters.at(0);
  sensor_msgs::PointCloud2 cluster;
  /*pcl::toROSMsg(highest_cluster, cluster);*/
  sensor_msgs::convertPointCloudToPointCloud2(received_clusters, cluster);
  cluster.header.frame_id=main_request.table.pose.header.frame_id;
  cluster.header.stamp=main_request.table.pose.header.stamp;

  ROS_INFO_STREAM("Sending highest cluster to cph recognition");
  //Brian's recognition service
  nrg_object_recognition::recognition rec_srv;

//////////////////////////Logic for % confidence in recognition result
  //Need to save recognition results into array (or not...)
  //std::vector<std::string> response_labels;

  //Then need to find percentage of label per set/array
  int percent_conf_pvct=0, percent_conf_plug=0, percent_conf_encl=0;

  rec_srv.request.cluster = cluster;
  rec_srv.request.threshold = 1500;
  //calling flann recognition service 10 times
  //and increasing percent confidence based on label
  for (int a=0; a<10; a++)
  {
    //Call recognition service
    if (!cph_client.call(rec_srv))
    {
      ROS_ERROR("Call to cph recognition service failed");
    }
    //response_labels.push_back(rec_srv.response.label);
    ROS_INFO_STREAM("Object labeled as "<< rec_srv.response.label);

    std::size_t found;
    std::string label = rec_srv.response.label;
    found=label.find_last_of("/");
    //Depending on label and angle, assign marker mesh, model_id, position for mesh and for part frame
    ROS_INFO_STREAM("Object labeled as "<< label.substr(found+1));

    if(label.substr(found+1)=="enclosure")
    {
      percent_conf_encl+=10;
    }
    else if(label.substr(found+1)=="plug")
    {
      percent_conf_plug+=10;
    }
    else if(label.substr(found+1)=="pvct" || label.substr(found+1)=="pvct_1")
    {
      percent_conf_pvct+=10;
    }
  }
  ROS_INFO("CPH recognition complete");
  float theta = rec_srv.response.pose.rotation*3.14159/180;//radians

////////////////////Assign response values/////////////////////////

  if (percent_conf_pvct > percent_conf_encl && percent_conf_pvct > percent_conf_plug)
  {
    main_response.label="pvct";
  }
  else if (percent_conf_plug > percent_conf_encl && percent_conf_plug > percent_conf_pvct)
  {
    main_response.label="plug";
  }
  else if (percent_conf_encl > percent_conf_plug && percent_conf_encl > percent_conf_pvct)
  {
    main_response.label="enclosure";
  }
  else
  {
    main_response.label="NA";
    ROS_ERROR_STREAM("Recognition produced no consistent result");
  }
  //main_response.label = rec_srv.response.label;
  main_response.pose = rec_srv.response.pose;

  ROS_INFO_STREAM("Recgonized pose: \n x: " << rec_srv.response.pose.x << "\n y: "<<rec_srv.response.pose.y <<
		  "\n z: "<<rec_srv.response.pose.z << "\n theta: "<<rec_srv.response.pose.rotation);
  //ROS_INFO_STREAM("Object labeled as "<< rec_srv.response.label);

////////////////////Take response rotation and make pick pose
  //Get the orientation of the part from the returned recognized object
  tf::Quaternion part_orientation;
  part_orientation.setValue(0, 0, sin(theta/2), cos(theta/2));
  //Invert the orientation 180 deg around the x axis (to get z down for pick pose frame)
  tf::Quaternion rot_part;
  rot_part.setValue(sin(3.14159/2), 0, 0, cos(3.14159/2));
  //get the pick pose frame by multiplying the part orientation with the 180 deg rotation
  tf::Quaternion rotated_part = part_orientation * rot_part;
  //convert tf::Quaternion to geometry_msgs::Quaternion for relaying pick pose frame
  geometry_msgs::Quaternion pick_frame;
  tf::quaternionTFToMsg(rotated_part, pick_frame);

  geometry_msgs::PoseStamped pick_pose;
  pick_pose.header.stamp=main_request.table.pose.header.stamp;
  pick_pose.header.frame_id=main_request.table.pose.header.frame_id;
  pick_pose.pose.orientation.x = pick_frame.x;
  pick_pose.pose.orientation.y = pick_frame.y;
  pick_pose.pose.orientation.z = pick_frame.z;
  pick_pose.pose.orientation.w = pick_frame.w;

////////////////////Assign marker and response properties
  //Assign id and marker based on label
  visualization_msgs::Marker mesh_marker;
  mesh_marker = make_marker(pick_pose, part_orientation);

  //Determine label and position and mesh resource based on that label
  /*std::size_t found;
  std::string label = main_response.label;
  found=label.find_last_of("/");
  //Depending on label and angle, assign marker mesh, model_id, position for mesh and for part frame
  ROS_WARN_STREAM("Object labeled as "<< label.substr(found+1));*/
  /*if(label.substr(found+1)=="enclosure" ||
		  label.substr(found+1)=="enclosure_a_2"||
		  label.substr(found+1)=="enclosure_a_1" ||
		  label.substr(found+1)=="enclosure_1")*/
  if(main_response.label=="enclosure")
  {
    main_response.model_id=1;
    mesh_marker.mesh_resource = "package://mantis_perception/data/meshes/demo_parts/elec_enclosure.STL";
    pick_pose.pose.position.x = rec_srv.response.pose.x - (_enc_1_x_offset);//+enc_pick_point_x;//enc_pick.x();
    pick_pose.pose.position.y = rec_srv.response.pose.y - (_enc_1_y_offset);//enc_pick.y();
    pick_pose.pose.position.z = rec_srv.response.pose.z - (_enc_1_z_offset)+_enc_pick_point_z;
    mesh_marker.pose.position.x=rec_srv.response.pose.x - (_enc_1_x_offset);
    mesh_marker.pose.position.y=rec_srv.response.pose.y - (_enc_1_y_offset);
    mesh_marker.pose.position.z=rec_srv.response.pose.z - (_enc_1_z_offset);
    ROS_INFO_STREAM("Pick pose z position: "<<pick_pose.pose.position.z);
    ROS_WARN_STREAM("Recognition returned enclosure with "<< percent_conf_encl<<" percent confidence");
  }
  /*else if (label.substr(found+1)=="small_plug" || label.substr(found+1)=="small_plug_a_1" || label.substr(found+1)=="small_plug_a_2" || label.substr(found+1)=="small_plug_a_3")
  {
    main_response.model_id=2;
    mesh_marker.mesh_resource = "package://mantis_perception/data/meshes/demo_parts/small_plug.STL";
    pick_pose.pose.position.x = rec_srv.response.pose.x-(small_plug_x_offset);
	pick_pose.pose.position.y = rec_srv.response.pose.y-(small_plug_y_offset);
	pick_pose.pose.position.z = rec_srv.response.pose.z-(small_plug_z_offset) + _small_plug_pick_point_z;
	mesh_marker.pose.position.x=rec_srv.response.pose.x-(small_plug_x_offset);
	mesh_marker.pose.position.y=rec_srv.response.pose.y-(small_plug_y_offset);
	mesh_marker.pose.position.z=rec_srv.response.pose.z-(small_plug_z_offset);
  }*/
  /*else if (label.substr(found+1)=="pvct" ||
		  label.substr(found+1)=="pvct_a_4" ||
		  label.substr(found+1)=="pvct_a_1" ||
		  label.substr(found+1)=="pvct_a_2" ||
		  label.substr(found+1)=="pvct_a_3" ||
		  label.substr(found+1)=="pvct_1" ||
		  label.substr(found+1)=="pcvt_2" )*/
  else if (main_response.label=="pvct")
  {
    main_response.model_id=3;
    mesh_marker.mesh_resource = "package://mantis_perception/data/meshes/demo_parts/pvc_t.STL";
    pick_pose.pose.position.x = rec_srv.response.pose.x - (_pvct_1_x_offset);
    pick_pose.pose.position.y = rec_srv.response.pose.y - (_pvct_1_y_offset);
    pick_pose.pose.position.z = rec_srv.response.pose.z - (_pvct_1_z_offset)+_pvct_pick_point_z/2;
    mesh_marker.pose.position.x=rec_srv.response.pose.x - (_pvct_1_x_offset);
    mesh_marker.pose.position.y=rec_srv.response.pose.y - (_pvct_1_y_offset);
    mesh_marker.pose.position.z=rec_srv.response.pose.z - (_pvct_1_z_offset);
    ROS_INFO_STREAM("Pick pose z position: "<<pick_pose.pose.position.z);
    ROS_WARN_STREAM("Recognition returned pvc tee with "<< percent_conf_pvct<<" percent confidence");
  }
  /*else if (label.substr(found+1)=="plug" ||
		  label.substr(found+1)=="plug_1" ||
		  label.substr(found+1)=="plugf" ||
		  label.substr(found+1)=="plug_a_1" ||
		  label.substr(found+1)=="plug_a_2"||
		  label.substr(found+1)=="plug_a_3")*/
  else if (main_response.label=="plug")
  {
    main_response.model_id=4;
    mesh_marker.mesh_resource = "package://mantis_perception/data/meshes/demo_parts/white_plug.STL";
    pick_pose.pose.position.x = rec_srv.response.pose.x - (_plug_1_x_offset);
    pick_pose.pose.position.y = rec_srv.response.pose.y - (_plug_1_y_offset);
    pick_pose.pose.position.z = rec_srv.response.pose.z - (_plug_1_z_offset)+_plug_pick_point_z;
    mesh_marker.pose.position.x=rec_srv.response.pose.x - (_plug_1_x_offset);
    mesh_marker.pose.position.y=rec_srv.response.pose.y - (_plug_1_y_offset);
    mesh_marker.pose.position.z=rec_srv.response.pose.z - (_plug_1_z_offset);
    ROS_INFO_STREAM("Pick pose z position: "<<pick_pose.pose.position.z);
    ROS_WARN_STREAM("Recognition returned plug with "<< percent_conf_plug<<" percent confidence");
  }
  /*else if (label.substr(found+1)=="pvc_elbow_1" || label.substr(found+1)=="pvcelbow" || label.substr(found+1)=="pvc_elbow_a_1")
  {
    main_response.model_id=5;
    mesh_marker.mesh_resource = "package://mantis_perception/data/meshes/demo_parts/pvc_elbow.STL";
    pick_pose.pose.position.x = rec_srv.response.pose.x - (pvc_elbow_1_x_offset);
    pick_pose.pose.position.y = rec_srv.response.pose.y - (pvc_elbow_1_y_offset);
    pick_pose.pose.position.z = rec_srv.response.pose.z - (pvc_elbow_1_z_offset)+_pvc_elbow_pick_point_z;
    mesh_marker.pose.position.x=rec_srv.response.pose.x - (pvc_elbow_1_x_offset);
    mesh_marker.pose.position.y=rec_srv.response.pose.y - (pvc_elbow_1_y_offset);
    mesh_marker.pose.position.z=rec_srv.response.pose.z - (pvc_elbow_1_z_offset);

  }*/
  else if (main_response.label=="NA")
  {
	main_response.model_id=0;
	ROS_ERROR_STREAM("Object not identified with high enough confidence");
  }
  else
  {
        main_response.model_id=0;
        ROS_ERROR_STREAM("Object not properly identified");
  }
  ROS_WARN_STREAM("Object labeled as "<< main_response.label<<" with model id "<<main_response.model_id);

  //Push current pick pose into array of pick poses as part of recognition service
  main_response.pick_poses.push_back(pick_pose);

  //finish inputing marker properties and publish
  ROS_INFO_STREAM("Marker pose: \n x: " << mesh_marker.pose.position.x << "\n y: "<<mesh_marker.pose.position.y <<
		  "\n z: "<<mesh_marker.pose.position.z << "\n theta: "<<rec_srv.response.pose.rotation);
  main_response.mesh_marker = mesh_marker;
  vis_pub.publish( mesh_marker );


//////Visualization: Matching PointCloud//////////////////////////////////////////////////////
  publish_matching_PC(rec_srv.response.label, rec_srv.response.pose, main_request.table);
/////////end visualization////////////////////////////////////////////////////

  return true;
}

visualization_msgs::Marker make_marker(geometry_msgs::PoseStamped pick_pose,
		tf::Quaternion part_orientation)
{
  visualization_msgs::Marker marker;
  marker.type = visualization_msgs::Marker::MESH_RESOURCE;
  marker.action = visualization_msgs::Marker::ADD;
  marker.lifetime = ros::Duration();
  marker.header.frame_id = pick_pose.header.frame_id;
  marker.header.stamp= pick_pose.header.stamp;//ros::Time();
  marker.scale.x = 1;
  marker.scale.y = 1;
  marker.scale.z = 1;
  marker.pose.orientation.x = part_orientation.x();
  marker.pose.orientation.y = part_orientation.y();
  marker.pose.orientation.z = part_orientation.z();
  marker.pose.orientation.w = part_orientation.w();
  marker.color.a = 1.0;
  marker.color.r = 0.0;
  marker.color.g = 1.0;
  marker.color.b = 0.0;
  return marker;
}

void publish_matching_PC(std::basic_string<char> label, nrg_object_recognition::pose pose, tabletop_object_detector::Table table)
{
  //Import pcd file which matches recognition response
  std::stringstream fileName;
  fileName << "/home"<<label << "_" << pose.rotation << ".pcd";
  //Load and convert file.
  pcl::PointCloud<pcl::PointXYZ>::Ptr trainingMatch (new pcl::PointCloud<pcl::PointXYZ>);

  pcl::io::loadPCDFile(fileName.str(), *trainingMatch);
  //Translate to location:
  Eigen::Vector4f centroid;
  pcl::compute3DCentroid(*trainingMatch, centroid);
  pcl::demeanPointCloud<pcl::PointXYZ> (*trainingMatch, centroid, *trainingMatch);

  Eigen::Vector3f translate;
  Eigen::Quaternionf rotate;
  translate(0) = pose.x;
  translate(1) = pose.y;
  translate(2) = pose.z;
  rotate.setIdentity();

  pcl::transformPointCloud(*trainingMatch, *trainingMatch, translate, rotate);
  //make into ros message for publlishing
  sensor_msgs::PointCloud2 recognized_cloud;
  pcl::toROSMsg(*trainingMatch, recognized_cloud);
  //Add transform to header
  recognized_cloud.header.frame_id = table.pose.header.frame_id;
  recognized_cloud.header.stamp=table.pose.header.stamp;
  //Publish to topic /recognition_result.
  rec_pub.publish(recognized_cloud);
}

int main(int argc, char **argv)
{
  
  ros::init(argc, argv, "mantis_object_recognition");
  ros::NodeHandle n;  
  
  ros::ServiceServer rec_serv = n.advertiseService("/mantis_object_recognition", rec_cb);
  cph_client = n.serviceClient<nrg_object_recognition::recognition>("/cph_recognition");
  rec_pub = n.advertise<sensor_msgs::PointCloud2>("/recognition_result",1);
  vis_pub = n.advertise<visualization_msgs::Marker>( "matching_mesh_marker", 10 );
  noise_pub=n.advertise<sensor_msgs::PointCloud2>("/noisy_points",1);
  
  ROS_INFO("mantis object detection/recognition node ready!");

  std::string paramNamespace = "";
  paramNamespace = "/" + ros::this_node::getName();
  ROS_INFO_STREAM("Reading parameters");
  ros::param::param(paramNamespace + "/plug_pick_point_z", _plug_pick_point_z, 0.048);
  ros::param::param(paramNamespace + "/enc_pick_point_z", _enc_pick_point_z, 0.051);
  ros::param::param(paramNamespace + "/pvct_pick_point_z", _pvct_pick_point_z, 0.025);
  ros::param::param(paramNamespace + "/pvc_elbow_pick_point_z", _pvc_elbow_pick_point_z, 0.030);
  ros::param::param(paramNamespace + "/small_plug_pick_point_z", _small_plug_pick_point_z, 0.045);
  ros::param::param(paramNamespace + "/plug_offset_x", _plug_1_x_offset,-0.0036);
  ros::param::param(paramNamespace + "/plug_offset_y", _plug_1_y_offset, 0.002);
  ros::param::param(paramNamespace + "/plug_offset_z", _plug_1_z_offset, 0.028);
  ros::param::param(paramNamespace + "/enclosure_offset_x", _enc_1_x_offset, 0.00563);
  ros::param::param(paramNamespace + "/enclosure_offset_y", _enc_1_y_offset, -0.0044);
  ros::param::param(paramNamespace + "/enclosure_offset_z", _enc_1_z_offset, 0.049);
  ros::param::param(paramNamespace + "/pvct_offset_x", _pvct_1_x_offset, 0.0034);
  ros::param::param(paramNamespace + "/pvct_offset_y", _pvct_1_y_offset, -0.0019);
  ros::param::param(paramNamespace + "/pvct_offset_z", _pvct_1_z_offset, 0.012);
  ROS_INFO_STREAM("Enclosure pick offset: "<<_enc_pick_point_z);
  ROS_INFO_STREAM("PVC t pick offset: "<<_pvct_pick_point_z);
  ROS_INFO_STREAM("Plug pick offset: "<<_plug_pick_point_z);
  
  ros::spin(); 
}




