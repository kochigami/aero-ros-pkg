#include <aero_std/AeroMoveitInterface.hh>
#include <aero_std/time.h>

int main(int argc, char **argv)
{
  // init ros
  ros::init(argc, argv, "trajectory_lifter_sample_node");
  ros::NodeHandle nh;
  
  // init robot interface
  aero::interface::AeroMoveitInterfacePtr interface(new aero::interface::AeroMoveitInterface(nh));
  ROS_INFO("reseting robot pose");
  interface->sendResetManipPose();

  const double z = -0.2;

  // prepare
  interface->sendLifter(-0.2, z);
  usleep(1000 * 1000);

  //
  std::vector<int> times;
  std::vector<std::pair<double, double>> tra;
  times.reserve(10);
  tra.reserve(10);

  double x;
  for (int i=0; i < 10; ++i) {
    x = -0.2 + 0.04 * (i + 1.0);
    times.push_back(1000);// [ms]
    tra.push_back(std::pair<double, double>(x, z));
  }

  ROS_INFO("starting trajectory");
  if (!interface->sendLifterTrajectory(tra, times)) {
    ROS_INFO("lifter ik failed");
  } else {
    ROS_INFO("lifter ik succeeded");
    usleep(1000 * 1000);
    interface->sendLifter(0.0,0.0);
  }

  ROS_INFO("demo node finished");
  ros::shutdown();
  return 0;
}