#include <ros/ros.h>
#include <kinematics_base/kinematics_base.h>
#include <urdf/model.h>
#include <arm_kinematics_constraint_aware/ik_fast_solver.h>

namespace armadillo_manipulator_kinematics
{

//autogenerated file
#include "armadillo_manipulator_ikfast_output.cpp"

class IKFastKinematicsPlugin : public kinematics::KinematicsBase
{
  std::vector<std::string> joint_names_;
  std::vector<double> joint_min_vector_;
  std::vector<double> joint_max_vector_;
  std::vector<bool> joint_has_limits_vector_;
  std::vector<std::string> link_names_;
  arm_kinematics_constraint_aware::ik_solver_base* ik_solver_;
  size_t num_joints_;
  std::vector<int> free_params_;
  void (*fk)(const IKReal* j, IKReal* eetrans, IKReal* eerot);
  
public:

  IKFastKinematicsPlugin():ik_solver_(0) {}
  ~IKFastKinematicsPlugin(){ if(ik_solver_) delete ik_solver_;}

  void fillFreeParams(int count, int *array) { free_params_.clear(); for(int i=0; i<count;++i) free_params_.push_back(array[i]); }
  
  bool initialize(const std::string& group_name,
                  const std::string& base_name,
                  const std::string& tip_name,
                  const double& search_discretization) {
    setValues(group_name, base_name, tip_name,search_discretization);

    ros::NodeHandle node_handle("~/"+group_name);

    std::string robot;
    node_handle.param("robot",robot,std::string());
      
    fillFreeParams(getNumFreeParameters(),getFreeParameters());
    num_joints_ = getNumJoints();
    ik_solver_ = new arm_kinematics_constraint_aware::ikfast_solver<IKSolution>(ik, num_joints_);
    fk=fk;
      
    if(free_params_.size()>1){
      ROS_FATAL("Only one free joint paramter supported!");
      return false;
    }
      
    urdf::Model robot_model;
    std::string xml_string;

    std::string urdf_xml,full_urdf_xml;
    node_handle.param("urdf_xml",urdf_xml,std::string("robot_description"));
    node_handle.searchParam(urdf_xml,full_urdf_xml);

    ROS_DEBUG("Reading xml file from parameter server\n");
    if (!node_handle.getParam(full_urdf_xml, xml_string))
    {
      ROS_FATAL("Could not load the xml from parameter server: %s\n", urdf_xml.c_str());
      return false;
    }

    node_handle.param(full_urdf_xml,xml_string,std::string());
    robot_model.initString(xml_string);

    boost::shared_ptr<urdf::Link> link = boost::const_pointer_cast<urdf::Link>(robot_model.getLink(tip_name_));
    while(link->name != base_name_ && joint_names_.size() <= num_joints_){
      //	ROS_INFO("link %s",link->name.c_str());
      link_names_.push_back(link->name);
      boost::shared_ptr<urdf::Joint> joint = link->parent_joint;
      if(joint){
        if (joint->type != urdf::Joint::UNKNOWN && joint->type != urdf::Joint::FIXED) {
	  joint_names_.push_back(joint->name);
          float lower, upper;
          int hasLimits;
          if ( joint->type != urdf::Joint::CONTINUOUS ) {
            if(joint->safety) {
              lower = joint->safety->soft_lower_limit; 
              upper = joint->safety->soft_upper_limit;
            } else {
              lower = joint->limits->lower;
              upper = joint->limits->upper;
            }
            hasLimits = 1;
          } else {
            lower = -M_PI;
            upper = M_PI;
            hasLimits = 0;
          }
          if(hasLimits) {
            joint_has_limits_vector_.push_back(true);
            joint_min_vector_.push_back(lower);
            joint_max_vector_.push_back(upper);
          } else {
            joint_has_limits_vector_.push_back(false);
            joint_min_vector_.push_back(-M_PI);
            joint_max_vector_.push_back(M_PI);
          }
        }
      } else{
        ROS_WARN("no joint corresponding to %s",link->name.c_str());
      }
      link = link->getParent();
    }
    
    if(joint_names_.size() != num_joints_){
      ROS_FATAL("Joints number mismatch.");
      return false;
    }
      
    std::reverse(link_names_.begin(),link_names_.end());
    std::reverse(joint_names_.begin(),joint_names_.end());
    std::reverse(joint_min_vector_.begin(),joint_min_vector_.end());
    std::reverse(joint_max_vector_.begin(),joint_max_vector_.end());
    std::reverse(joint_has_limits_vector_.begin(), joint_has_limits_vector_.end());

    for(size_t i=0; i <num_joints_; ++i)
      ROS_INFO_STREAM(joint_names_[i] << " " << joint_min_vector_[i] << " " << joint_max_vector_[i] << " " << joint_has_limits_vector_[i]);

    return true;
  }

  bool getCount(int &count, 
                const int &max_count, 
                const int &min_count)
  {
    if(count > 0)
    {
      if(-count >= min_count)
      {   
        count = -count;
        return true;
      }
      else if(count+1 <= max_count)
      {
        count = count+1;
        return true;
      }
      else
      {
        return false;
      }
    }
    else
    {
      if(1-count <= max_count)
      {
        count = 1-count;
        return true;
      }
      else if(count-1 >= min_count)
      {
        count = count -1;
        return true;
      }
      else
        return false;
    }
  }

  bool getPositionIK(const geometry_msgs::Pose &ik_pose,
                     const std::vector<double> &ik_seed_state,
                     std::vector<double> &solution,
                     int &error_code) {
	
    std::vector<double> vfree(free_params_.size());
    for(std::size_t i = 0; i < free_params_.size(); ++i){
      int p = free_params_[i];
      //	    ROS_ERROR("%u is %f",p,ik_seed_state[p]);
      vfree[i] = ik_seed_state[p];
    }

    KDL::Frame frame;
    tf::PoseMsgToKDL(ik_pose,frame);

    int numsol = ik_solver_->solve(frame,vfree);

		
    if(numsol){
      for(int s = 0; s < numsol; ++s){
        std::vector<double> sol;
        ik_solver_->getSolution(s,sol);
        bool obeys_limits = true;
        for(unsigned int i = 0; i < sol.size(); i++) {
          if(joint_has_limits_vector_[i] && (sol[i] < joint_min_vector_[i] || sol[i] > joint_max_vector_[i])) {
            obeys_limits = false;
            break;
          }
          //ROS_INFO_STREAM("Num " << i << " value " << sol[i] << " has limits " << joint_has_limits_vector_[i] << " " << joint_min_vector_[i] << " " << joint_max_vector_[i]);
        }
        if(obeys_limits) {
          ik_solver_->getSolution(s,solution);
          error_code = kinematics::SUCCESS;
          return true;
        }
      }
    }
	
    error_code = kinematics::NO_IK_SOLUTION; 
    return false;
  }
  bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                        const std::vector<double> &ik_seed_state,
                        const double &timeout,
                        std::vector<double> &solution,
                        int &error_code) {
  
    if(free_params_.size()==0){
      return getPositionIK(ik_pose, ik_seed_state,solution, error_code);
    }
	
    KDL::Frame frame;
    tf::PoseMsgToKDL(ik_pose,frame);

    std::vector<double> vfree(free_params_.size());

    ros::Time maxTime = ros::Time::now() + ros::Duration(timeout);
    int counter = 0;

    double initial_guess = ik_seed_state[free_params_[0]];
    vfree[0] = initial_guess;

    int num_positive_increments = (joint_max_vector_[free_params_[0]]-initial_guess)/search_discretization_;
    int num_negative_increments = (initial_guess-joint_min_vector_[free_params_[0]])/search_discretization_;

    ROS_INFO_STREAM("Free param is " << free_params_[0] << " initial guess is " << initial_guess << " " << num_positive_increments << " " << num_negative_increments);

    while(1) {
      int numsol = ik_solver_->solve(frame,vfree);
      
      //ROS_INFO_STREAM("Solutions number is " << numsol);

      //ROS_INFO("%f",vfree[0]);
	    
      if(numsol > 0){
        for(int s = 0; s < numsol; ++s){
          std::vector<double> sol;
          ik_solver_->getSolution(s,sol);
          bool obeys_limits = true;
          for(unsigned int i = 0; i < sol.size(); i++) {
            if(joint_has_limits_vector_[i] && (sol[i] < joint_min_vector_[i] || sol[i] > joint_max_vector_[i])) {
              obeys_limits = false;
              break;
            }
            //ROS_INFO_STREAM("Num " << i << " value " << sol[i] << " has limits " << joint_has_limits_vector_[i] << " " << joint_min_vector_[i] << " " << joint_max_vector_[i]);
          }
          if(obeys_limits) {
            ik_solver_->getSolution(s,solution);
            error_code = kinematics::SUCCESS;
            return true;
          }
        }
      }
      
      // if(numsol > 0){
      //   for(unsigned int i = 0; i < sol.size(); i++) {
      //     if(i == 0) {
      //       ik_solver_->getClosestSolution(ik_seed_state,solution);
      //     } else {
      //       ik_solver_->getSolution(s,sol);            
      //     }
      //   }
      //   bool obeys_limits = true;
      //   for(unsigned int i = 0; i < solution.size(); i++) {
      //     if(joint_has_limits_vector_[i] && (solution[i] < joint_min_vector_[i] || solution[i] > joint_max_vector_[i])) {
      //       obeys_limits = false;
      //       break;
      //     }
      //     //ROS_INFO_STREAM("Num " << i << " value " << sol[i] << " has limits " << joint_has_limits_vector_[i] << " " << joint_min_vector_[i] << " " << joint_max_vector_[i]);
      //   }
      //   if(obeys_limits) {
      //     error_code = kinematics::SUCCESS;
      //     return true;
      //   }
      // }
      if(!getCount(counter, num_positive_increments, num_negative_increments)) {
        error_code = kinematics::NO_IK_SOLUTION; 
        return false;
      }
      
      vfree[0] = initial_guess+search_discretization_*counter;
      ROS_DEBUG_STREAM(counter << " " << vfree[0]);
    }
    //shouldn't ever get here
    error_code = kinematics::NO_IK_SOLUTION; 
    return false;
  }      

  bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                        const std::vector<double> &ik_seed_state,
                        const double &timeout,
                        const unsigned int& redundancy,
                        const double &consistency_limit,
                        std::vector<double> &solution,
                        int &error_code) {

    if(free_params_.size()==0){
      //TODO - how to check consistency when there are no free params?
      return getPositionIK(ik_pose, ik_seed_state,solution, error_code);
      ROS_WARN_STREAM("No free parameters, so can't search");
    }

    if(redundancy != (unsigned int)free_params_[0]) {
      ROS_WARN_STREAM("Calling consistency search with wrong free param");
      return false;
    }
	
    KDL::Frame frame;
    tf::PoseMsgToKDL(ik_pose,frame);

    std::vector<double> vfree(free_params_.size());

    ros::Time maxTime = ros::Time::now() + ros::Duration(timeout);
    int counter = 0;

    double initial_guess = ik_seed_state[free_params_[0]];
    vfree[0] = initial_guess;

    double max_limit = fmin(joint_max_vector_[free_params_[0]], initial_guess+consistency_limit);
    double min_limit = fmax(joint_min_vector_[free_params_[0]], initial_guess-consistency_limit);

    int num_positive_increments = (int)((max_limit-initial_guess)/search_discretization_);
    int num_negative_increments = (int)((initial_guess-min_limit)/search_discretization_);

    ROS_DEBUG_STREAM("Free param is " << free_params_[0] << " initial guess is " << initial_guess << " " << num_positive_increments << " " << num_negative_increments);

    while(1) {
      int numsol = ik_solver_->solve(frame,vfree);
      
      //ROS_INFO_STREAM("Solutions number is " << numsol);

      //ROS_INFO("%f",vfree[0]);
	    
      if(numsol > 0){
        for(int s = 0; s < numsol; ++s){
          std::vector<double> sol;
          ik_solver_->getSolution(s,sol);
          bool obeys_limits = true;
          for(unsigned int i = 0; i < sol.size(); i++) {
            if(joint_has_limits_vector_[i] && (sol[i] < joint_min_vector_[i] || sol[i] > joint_max_vector_[i])) {
              obeys_limits = false;
              break;
            }
            //ROS_INFO_STREAM("Num " << i << " value " << sol[i] << " has limits " << joint_has_limits_vector_[i] << " " << joint_min_vector_[i] << " " << joint_max_vector_[i]);
          }
          if(obeys_limits) {
            ik_solver_->getSolution(s,solution);
            error_code = kinematics::SUCCESS;
            return true;
          }
        }
      }
      
      // if(numsol > 0){
      //   for(unsigned int i = 0; i < sol.size(); i++) {
      //     if(i == 0) {
      //       ik_solver_->getClosestSolution(ik_seed_state,solution);
      //     } else {
      //       ik_solver_->getSolution(s,sol);            
      //     }
      //   }
      //   bool obeys_limits = true;
      //   for(unsigned int i = 0; i < solution.size(); i++) {
      //     if(joint_has_limits_vector_[i] && (solution[i] < joint_min_vector_[i] || solution[i] > joint_max_vector_[i])) {
      //       obeys_limits = false;
      //       break;
      //     }
      //     //ROS_INFO_STREAM("Num " << i << " value " << sol[i] << " has limits " << joint_has_limits_vector_[i] << " " << joint_min_vector_[i] << " " << joint_max_vector_[i]);
      //   }
      //   if(obeys_limits) {
      //     error_code = kinematics::SUCCESS;
      //     return true;
      //   }
      // }
      if(!getCount(counter, num_positive_increments, num_negative_increments)) {
        error_code = kinematics::NO_IK_SOLUTION; 
        return false;
      }
      
      vfree[0] = initial_guess+search_discretization_*counter;
      ROS_DEBUG_STREAM(counter << " " << vfree[0]);
    }
    //shouldn't ever get here
    error_code = kinematics::NO_IK_SOLUTION; 
    return false;
  }      

  bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                        const std::vector<double> &ik_seed_state,
                        const double &timeout,
                        std::vector<double> &solution,
                        const boost::function<void(const geometry_msgs::Pose &ik_pose,const std::vector<double> &ik_solution,int &error_code)> &desired_pose_callback,
                        const boost::function<void(const geometry_msgs::Pose &ik_pose,const std::vector<double> &ik_solution,int &error_code)> &solution_callback,
                        int &error_code){

    if(free_params_.size()==0){
      if(!getPositionIK(ik_pose, ik_seed_state,solution, error_code)) {
        ROS_DEBUG_STREAM("No solution whatsoever");
        error_code = kinematics::NO_IK_SOLUTION; 
        return false;
      } 
      solution_callback(ik_pose,solution,error_code);
      if(error_code == kinematics::SUCCESS) {
        ROS_DEBUG_STREAM("Solution passes");
        return true;
      } else {
        ROS_DEBUG_STREAM("Solution has error code " << error_code);
        return false;
      }
    }

    if(!desired_pose_callback.empty())
      desired_pose_callback(ik_pose,ik_seed_state,error_code);
    if(error_code < 0)
    {
      ROS_ERROR("Could not find inverse kinematics for desired end-effector pose since the pose may be in collision");
      return false;
    }

    KDL::Frame frame;
    tf::PoseMsgToKDL(ik_pose,frame);

    std::vector<double> vfree(free_params_.size());

    ros::Time maxTime = ros::Time::now() + ros::Duration(timeout);
    int counter = 0;

    double initial_guess = ik_seed_state[free_params_[0]];
    vfree[0] = initial_guess;

    int num_positive_increments = (joint_max_vector_[free_params_[0]]-initial_guess)/search_discretization_;
    int num_negative_increments = (initial_guess - joint_min_vector_[free_params_[0]])/search_discretization_;

    ROS_DEBUG_STREAM("Free param is " << free_params_[0] << " initial guess is " << initial_guess << " " << num_positive_increments << " " << num_negative_increments);

    unsigned int solvecount = 0;
    unsigned int countsol = 0;

    ros::WallTime start = ros::WallTime::now();

    std::vector<double> sol;
    while(1) {
      int numsol = ik_solver_->solve(frame,vfree);
      if(solvecount == 0) {
        if(numsol == 0) {
          ROS_DEBUG_STREAM("Bad solve time is " << ros::WallTime::now()-start);
        } else {
          ROS_DEBUG_STREAM("Good solve time is " << ros::WallTime::now()-start);
        }
      }
      solvecount++;
      if(numsol > 0){
        if(solution_callback.empty()){
          ik_solver_->getClosestSolution(ik_seed_state,solution);
          error_code = kinematics::SUCCESS;
          return true;
        }
        
        for(int s = 0; s < numsol; ++s){
          ik_solver_->getSolution(s,sol);
          countsol++;
          bool obeys_limits = true;
          for(unsigned int i = 0; i < sol.size(); i++) {
            if(joint_has_limits_vector_[i] && (sol[i] < joint_min_vector_[i] || sol[i] > joint_max_vector_[i])) {
              obeys_limits = false;
              break;
            }
          }
          if(obeys_limits) {
            solution_callback(ik_pose,sol,error_code);
            if(error_code == kinematics::SUCCESS){
              solution = sol;
              ROS_DEBUG_STREAM("Took " << (ros::WallTime::now() - start) << " to return true " << countsol << " " << solvecount);
              return true;
            }
          }
        }
      }
      if(!getCount(counter, num_positive_increments, num_negative_increments)) {
        error_code = kinematics::NO_IK_SOLUTION; 
        ROS_DEBUG_STREAM("Took " << (ros::WallTime::now() - start) << " to return false " << countsol << " " << solvecount);
        return false;
      }
      vfree[0] = initial_guess+search_discretization_*counter;
      ROS_DEBUG_STREAM(counter << " " << vfree[0]);
    }
    error_code = kinematics::NO_IK_SOLUTION; 
    return false;
  }      

  bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                        const std::vector<double> &ik_seed_state,
                        const double &timeout,
                        const unsigned int& redundancy,
                        const double &consistency_limit,
                        std::vector<double> &solution,
                        const boost::function<void(const geometry_msgs::Pose &ik_pose,const std::vector<double> &ik_solution,int &error_code)> &desired_pose_callback,
                        const boost::function<void(const geometry_msgs::Pose &ik_pose,const std::vector<double> &ik_solution,int &error_code)> &solution_callback,
                        int &error_code){

    if(free_params_.size()==0){
      if(!getPositionIK(ik_pose, ik_seed_state,solution, error_code)) {
        ROS_DEBUG_STREAM("No solution whatsoever");
        error_code = kinematics::NO_IK_SOLUTION; 
        return false;
      } 
      solution_callback(ik_pose,solution,error_code);
      if(error_code == kinematics::SUCCESS) {
        ROS_DEBUG_STREAM("Solution passes");
        return true;
      } else {
        ROS_DEBUG_STREAM("Solution has error code " << error_code);
        return false;
      }
    }

    if(redundancy != (unsigned int) free_params_[0]) {
      ROS_WARN_STREAM("Calling consistency search with wrong free param");
      return false;
    }

    if(!desired_pose_callback.empty())
      desired_pose_callback(ik_pose,ik_seed_state,error_code);
    if(error_code < 0)
    {
      ROS_ERROR("Could not find inverse kinematics for desired end-effector pose since the pose may be in collision");
      return false;
    }

    KDL::Frame frame;
    tf::PoseMsgToKDL(ik_pose,frame);

    std::vector<double> vfree(free_params_.size());

    ros::Time maxTime = ros::Time::now() + ros::Duration(timeout);
    int counter = 0;

    double initial_guess = ik_seed_state[free_params_[0]];
    vfree[0] = initial_guess;

    double max_limit = fmin(joint_max_vector_[free_params_[0]], initial_guess+consistency_limit);
    double min_limit = fmax(joint_min_vector_[free_params_[0]], initial_guess-consistency_limit);

    int num_positive_increments = (int)((max_limit-initial_guess)/search_discretization_);
    int num_negative_increments = (int)((initial_guess-min_limit)/search_discretization_);

    ROS_DEBUG_STREAM("Free param is " << free_params_[0] << " initial guess is " << initial_guess << " " << num_positive_increments << " " << num_negative_increments);

    unsigned int solvecount = 0;
    unsigned int countsol = 0;

    ros::WallTime start = ros::WallTime::now();

    std::vector<double> sol;
    while(1) {
      int numsol = ik_solver_->solve(frame,vfree);
      if(solvecount == 0) {
        if(numsol == 0) {
          ROS_DEBUG_STREAM("Bad solve time is " << ros::WallTime::now()-start);
        } else {
          ROS_DEBUG_STREAM("Good solve time is " << ros::WallTime::now()-start);
        }
      }
      solvecount++;
      if(numsol > 0){
        if(solution_callback.empty()){
          ik_solver_->getClosestSolution(ik_seed_state,solution);
          error_code = kinematics::SUCCESS;
          return true;
        }
        
        for(int s = 0; s < numsol; ++s){
          ik_solver_->getSolution(s,sol);
          countsol++;
          bool obeys_limits = true;
          for(unsigned int i = 0; i < sol.size(); i++) {
            if(joint_has_limits_vector_[i] && (sol[i] < joint_min_vector_[i] || sol[i] > joint_max_vector_[i])) {
              obeys_limits = false;
              break;
            }
          }
          if(obeys_limits) {
            solution_callback(ik_pose,sol,error_code);
            if(error_code == kinematics::SUCCESS){
              solution = sol;
              ROS_DEBUG_STREAM("Took " << (ros::WallTime::now() - start) << " to return true " << countsol << " " << solvecount);
              return true;
            }
          }
        }
      }
      if(!getCount(counter, num_positive_increments, num_negative_increments)) {
        error_code = kinematics::NO_IK_SOLUTION; 
        ROS_DEBUG_STREAM("Took " << (ros::WallTime::now() - start) << " to return false " << countsol << " " << solvecount);
        return false;
      }
      vfree[0] = initial_guess+search_discretization_*counter;
      ROS_DEBUG_STREAM(counter << " " << vfree[0]);
    }
    error_code = kinematics::NO_IK_SOLUTION; 
    return false;
  }      

  bool getPositionFK(const std::vector<std::string> &link_names,
                     const std::vector<double> &joint_angles, 
                     std::vector<geometry_msgs::Pose> &poses){
    return false;
    
    KDL::Frame p_out;

    if(link_names.size() == 0) {
      ROS_WARN_STREAM("Link names with nothing");
      return false;
    }

    if(link_names.size()!=1 || link_names[0]!=tip_name_){
      ROS_ERROR("Can compute FK for %s only",tip_name_.c_str());
      return false;
    }
	
    bool valid = true;
	
   double eerot[9], eetrans[3];
    fk(&joint_angles[0],eetrans,eerot);
    for(int i=0; i<3;++i) p_out.p.data[i] = eetrans[i];
    for(int i=0; i<9;++i) p_out.M.data[i] = eerot[i];
	
    poses.resize(1);
    tf::PoseKDLToMsg(p_out,poses[0]);	
	
    return valid;
  }      
  const std::vector<std::string>& getJointNames() const { return joint_names_; }
  const std::vector<std::string>& getLinkNames() const { return link_names_; }
};
}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_DECLARE_CLASS(armadillo_manipulator_kinematics, IKFastKinematicsPlugin, armadillo_manipulator_kinematics::IKFastKinematicsPlugin, kinematics::KinematicsBase);

