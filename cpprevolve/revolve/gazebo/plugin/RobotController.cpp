/* export PATH=~/installed/gazebo_debug/bin:$PATH
export LD_LIBRARY_PATH=~/installed/gazebo_debug/lib

* Copyright (C) 2017 Vrije Universiteit Amsterdam
*
* Licensed under the Apache License, Version 2.0 (the "License");
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Author: Elte Hupkes
* Date: May 3, 2015
*
*/

#include  <stdexcept>

#include <gazebo/sensors/sensors.hh>

#include <revolve/gazebo/motors/MotorFactory.h>
#include <revolve/gazebo/sensors/SensorFactory.h>
#include <revolve/gazebo/brains/Brains.h>
#include <revolve/gazebo/brains/DifferentialCPGClean.h>

#include "RobotController.h"

namespace gz = gazebo;

using namespace revolve::gazebo;

/////////////////////////////////////////////////
/// Default actuation time is given and this will be overwritten by the plugin
/// config in Load.
RobotController::RobotController()
    : actuationTime_(0)
{
}

/////////////////////////////////////////////////
RobotController::~RobotController()
{
  this->node_.reset();
  this->world_.reset();
  this->motorFactory_.reset();
  this->sensorFactory_.reset();
}

/////////////////////////////////////////////////
void RobotController::Load(
    ::gazebo::physics::ModelPtr _parent,
    sdf::ElementPtr _sdf)
{
  // Store the pointer to the model / world
  this->model_ = _parent;
  this->world_ = _parent->GetWorld();
  this->initTime_ = this->world_->SimTime().Double();

  // Create transport node
  this->node_.reset(new gz::transport::Node());
  this->node_->Init();

  // Subscribe to robot battery state updater
  this->batterySetSub_ = this->node_->Subscribe(
      "~/battery_level/request",
      &RobotController::UpdateBattery,
      this);
  this->batterySetPub_ = this->node_->Advertise< gz::msgs::Response >(
      "~/battery_level/response");

  if (not _sdf->HasElement("rv:robot_config"))
  {
    std::cerr
        << "No `rv:robot_config` element found, controller not initialized."
        << std::endl;
    return;
  }

  auto robotConfiguration = _sdf->GetElement("rv:robot_config");

  if (robotConfiguration->HasElement("rv:update_rate"))
  {
    auto updateRate = robotConfiguration->GetElement("rv:update_rate")->Get< double >();
    this->actuationTime_ = 1.0 / updateRate;
  }

  // Load motors
  this->motorFactory_ = this->MotorFactory(_parent);
  this->LoadActuators(robotConfiguration);

  // Load sensors
  this->sensorFactory_ = this->SensorFactory(_parent);
  this->LoadSensors(robotConfiguration);

  // Load brain, this needs to be done after the motors and sensors so they
  // can potentially be reordered.
  this->LoadBrain(robotConfiguration);

  // Call the battery loader
  this->LoadBattery(robotConfiguration);

  // Call startup function which decides on actuation
  this->Startup(_parent, _sdf);
}

/////////////////////////////////////////////////
void RobotController::UpdateBattery(ConstRequestPtr &_request)
{
  if (_request->data() not_eq this->model_->GetName() and
      _request->data() not_eq this->model_->GetScopedName())
  {
    return;
  }

  gz::msgs::Response resp;
  resp.set_id(_request->id());
  resp.set_request(_request->request());

  if (_request->request() == "set_battery_level")
  {
    resp.set_response("success");
    this->SetBatteryLevel(_request->dbl_data());
  }
  else
  {
    std::stringstream ss;
    ss << this->BatteryLevel();
    resp.set_response(ss.str());
  }

  batterySetPub_->Publish(resp);
}

/////////////////////////////////////////////////
void RobotController::LoadActuators(const sdf::ElementPtr _sdf)
{
  if (not _sdf->HasElement("rv:brain")
      or not _sdf->GetElement("rv:brain")->HasElement("rv:actuators"))
  {
    return;
  }
  auto actuators = _sdf->GetElement("rv:brain")->GetElement("rv:actuators");

  // Load actuators of type servomotor
  if (actuators->HasElement("rv:servomotor"))
  {
    auto servomotor = actuators->GetElement("rv:servomotor");
    while (servomotor)
    {
      auto servomotorObj = this->motorFactory_->Create(servomotor);
      motors_.push_back(servomotorObj);
      servomotor = servomotor->GetNextElement("rv:servomotor");
    }
  }
}

/////////////////////////////////////////////////
void RobotController::LoadSensors(const sdf::ElementPtr _sdf)
{
  if (not _sdf->HasElement("rv:brain")
      or not _sdf->GetElement("rv:brain")->HasElement("rv:sensors"))
  {
    return;
  }
  auto sensors = _sdf->GetElement("rv:brain")->GetElement("rv:sensors");

  // Load sensors
  auto sensor = sensors->GetElement("rv:sensor");
  while (sensor)
  {
    auto sensorObj = this->sensorFactory_->Create(sensor);
    sensors_.push_back(sensorObj);
    sensor = sensor->GetNextElement("rv:sensor");
  }
}

/////////////////////////////////////////////////
MotorFactoryPtr RobotController::MotorFactory(
    ::gazebo::physics::ModelPtr _model)
{
  return MotorFactoryPtr(new class MotorFactory(_model));
}

/////////////////////////////////////////////////
SensorFactoryPtr RobotController::SensorFactory(
    ::gazebo::physics::ModelPtr _model)
{
  return SensorFactoryPtr(new class SensorFactory(_model));
}

/////////////////////////////////////////////////
void RobotController::LoadBrain(const sdf::ElementPtr _sdf)
{
  if (not _sdf->HasElement("rv:brain"))
  {
    std::cerr << "No robot brain detected, this is probably an error."
              << std::endl;
    return;
  }

  auto brain = _sdf->GetElement("rv:brain");
  auto controller = brain->GetElement("rv:controller")->GetAttribute("type")->GetAsString();
  auto learner = brain->GetElement("rv:learner")->GetAttribute("type")->GetAsString();
  std::cout << "Loading controller " << controller << " and learner " << learner << std::endl;

  if ("offline" == learner and "ann" == controller)
  {
    brain_.reset(new NeuralNetwork(this->model_, brain, motors_, sensors_));
  }
  else if ("rlpower" == learner and "spline" == controller)
  {
    if (not motors_.empty()) {
        brain_.reset(new RLPower(this->model_, brain, motors_, sensors_));
    }
  }
  else if ("bo" == learner and "cpg" == controller)
  {
    brain_.reset(new DifferentialCPG(this->model_, _sdf, motors_, sensors_));
  }
  else if ("offline" == learner and "cpg" == controller)
  {
      // Dummy params for testing. Actual parameters should come from sdf in the end.
      // Attention with dummy weights: have to be as many as actual weights in the robot
      revolve::DifferentialCPG::ControllerParams params;
      params.reset_neuron_random = false;
      params.use_frame_of_reference = false;
      params.init_neuron_state = 0.707;
      params.range_ub = 1.0;
      params.signal_factor_all = 1.0;
      params.signal_factor_mid = 2.5;
      params.signal_factor_left_right = 2.5;
      params.abs_output_bound = 1.0;
      // Specific weights for spider running forward.
      params.weights = {0.482167, 0.560357, 0.753772, 0.221536, 0.44513, 0.667353, 0.580933, 0.246228, 0.111797,
      0.110425, 0.667353, 0.519204, 0.11134, 0.667353, 0.70439, 0.000228624, 0.444673, 0.287837};
      brain_.reset(new DifferentialCPGClean(params, motors_));
  }
  else
  {
    throw std::runtime_error("Robot brain is not defined.");
  }
}

/////////////////////////////////////////////////
/// Default startup, bind to CheckUpdate
void RobotController::Startup(
    ::gazebo::physics::ModelPtr /*_parent*/,
    sdf::ElementPtr /*_sdf*/)
{
  this->updateConnection_ = gz::event::Events::ConnectWorldUpdateBegin(
      boost::bind(&RobotController::CheckUpdate, this, _1));
}

/////////////////////////////////////////////////
void RobotController::CheckUpdate(const ::gazebo::common::UpdateInfo _info)
{
  auto diff = _info.simTime - lastActuationTime_;

  if (diff.Double() > actuationTime_)
  {
    this->DoUpdate(_info);
    lastActuationTime_ = _info.simTime;
  }
}

/////////////////////////////////////////////////
/// Default update function simply tells the brain to perform an update
void RobotController::DoUpdate(const ::gazebo::common::UpdateInfo _info)
{
  auto currentTime = _info.simTime.Double() - initTime_;

  if (brain_)
    brain_->Update(motors_, sensors_, currentTime, actuationTime_);
}

/////////////////////////////////////////////////
void RobotController::LoadBattery(const sdf::ElementPtr _sdf)
{
  if (_sdf->HasElement("rv:battery"))
  {
    this->batteryElem_ = _sdf->GetElement("rv:battery");
  }
}

/////////////////////////////////////////////////
double RobotController::BatteryLevel()
{
  if (not batteryElem_ or not batteryElem_->HasElement("rv:level"))
  {
    return 0.0;
  }

  return batteryElem_->GetElement("rv:level")->Get< double >();
}

/////////////////////////////////////////////////
void RobotController::SetBatteryLevel(double _level)
{
  if (batteryElem_ and batteryElem_->HasElement("rv:level"))
  {
    batteryElem_->GetElement("rv:level")->Set(_level);
  }
}
