#include "pcl_processor/pcl_processor_node.h"


int main(int argc, char **argv)
{

	ros::init(argc, argv, "pcl_processor");
	ros::NodeHandle nh;

  //create ros wrapper object
  PclProcessorRos pcl_processor_ros;


	ROS_INFO("Ready to calculate pose.");
	ros::spin();

	return 0;
}



PclProcessorRos::PclProcessorRos() :
    nh_(ros::this_node::getName())
{
  uint32_t queue_size = 1;
	filtered_pointcloud_pub = nh_.advertise<sensor_msgs::PointCloud2>("pcl_cloud_filtered", queue_size);
	capture_pointcloud_pub = nh_.advertise<sensor_msgs::PointCloud2>("pcl_cloud_capture", queue_size);

	calculate_objectpose_service = nh_.advertiseService("calculate_object_pose", &PclProcessorRos::CalculateObjectpose, this);
}

PclProcessorRos::~PclProcessorRos()
{

}

//void PclProcessorRos::process(){
//}

bool PclProcessorRos::CalculateObjectpose(pcl_processor_msgs::CalculateObjectposeFromPointcloud::Request  &request,
         pcl_processor_msgs::CalculateObjectposeFromPointcloud::Response &response)
{

  ROS_INFO("CalculateObjectposeFromPointcloud start");

	sensor_msgs::PointCloud2 input_pointcloud;
	{
		sensor_msgs::PointCloud2ConstPtr msg = ros::topic::waitForMessage<sensor_msgs::PointCloud2>("/camera/depth/color/points", ros::Duration(10));
	  if (msg == NULL){
				ROS_INFO("No point clound messages received");
				response.success = false;
				return false;
		}
	  else
	      input_pointcloud = *msg;
	}

	capture_pointcloud_pub.publish(input_pointcloud);

	{
  	pcl::PCLPointCloud2* cloud2 = new pcl::PCLPointCloud2;
	  pcl_conversions::toPCL(input_pointcloud, *cloud2);  // From ROS-PCL to PCL2

	  pcl::PointCloud<pcl::PointXYZ>::Ptr pcl_cloud(new pcl::PointCloud<pcl::PointXYZ>);
	  pcl::fromPCLPointCloud2(*cloud2, *pcl_cloud); // From PCL2 to PCL
		pcl_processor.setInputCloud(pcl_cloud);
	}

	pcl_processor.process();
	filtered_pcl_cloud = pcl_processor.getFilteredCloud();

	{
		pcl::PCLPointCloud2* filtered_cloud2 = new pcl::PCLPointCloud2;
		pcl::toPCLPointCloud2(*filtered_pcl_cloud, *filtered_cloud2); // From PCL to PCL2

	  /* initialaize pointcloud */
	  ROS_INFO("Create filtered ROS cloud");

	  sensor_msgs::PointCloud2 filtered_ros_cloud;
	  pcl::toROSMsg(*filtered_pcl_cloud, filtered_ros_cloud);

	  filtered_ros_cloud.header.frame_id = input_pointcloud.header.frame_id;

	  /* publish point cloud */
	  ROS_INFO("Publish filtered cloud");
	  filtered_pointcloud_pub.publish(filtered_ros_cloud);
	}

	{
		geometry_msgs::Vector3 object_position = pcl_processor.getObjectPosition();

		geometry_msgs::TransformStamped static_transformStamped;

		tf2_ros::Buffer tfBuffer;
		tf2_ros::TransformListener tfListener(tfBuffer);

		static tf2_ros::StaticTransformBroadcaster static_broadcaster;
		ROS_INFO("add static transform to object");
		/* add static transform to object */
		static_transformStamped.header.stamp = ros::Time::now();
		static_transformStamped.header.frame_id = "camera_depth_optical_frame";
		static_transformStamped.child_frame_id = "object_to_grasp";

		static_transformStamped.transform.translation = object_position;
		static_transformStamped.transform.rotation.x = 0;
		static_transformStamped.transform.rotation.y = 0;
		static_transformStamped.transform.rotation.z = 0;
		static_transformStamped.transform.rotation.w = 1.0;
		static_broadcaster.sendTransform(static_transformStamped);

		/* wait a while */
		ros::Duration(1.0).sleep();
		ROS_INFO("get object pose relative to world ");
		/* get object pose relative to world */
		try{
			geometry_msgs::TransformStamped transformStamped;
			geometry_msgs::PoseStamped poseStamped;
			poseStamped.pose.orientation.x = 0;
			poseStamped.pose.orientation.y = 0;
			poseStamped.pose.orientation.z = 0;
			poseStamped.pose.orientation.w = 1.0;
			transformStamped = tfBuffer.lookupTransform("world", "object_to_grasp", ros::Time(0));
			tf2::doTransform(poseStamped, poseStamped, transformStamped); // object_to_grasp is the PoseStamped I want to transform

			response.success = true;
			response.object_pose = poseStamped;

		}
		catch (tf2::TransformException &ex) {
			ROS_ERROR("Error lookupTransform.");
			response.success = false;
		}
	}

	ROS_INFO("CalculateObjectposeFromPointcloud exit");
	/* return object pose in response*/
	return true;
}
