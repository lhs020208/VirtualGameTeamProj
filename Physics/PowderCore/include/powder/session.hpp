#pragma once
#include "powder/world.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
namespace powder {
// Fixed physical steps. Rendering only supplies a work budget, never physical dt.
class Session {
public:
 Session(ParticleSystem initial,double dt):initial_(std::move(initial)),dt_(dt){
  if(!std::isfinite(dt)||dt<=0)throw std::invalid_argument("invalid session dt");
  initial_.reset_history();reset();
 }
 void reset(){world_=initial_;world_.reset_history();steps_=0;running_=false;error_.clear();report_={};}
 void toggle(){if(error_.empty())running_=!running_;}
 void pause(){running_=false;}
 bool single_step(){running_=false;return advance_one();}
 void frame(std::size_t budget){for(std::size_t i=0;i<budget&&running_;++i)if(!advance_one())break;}
 // Supply provides a copy preserving existing contacts and particles. Clock is unchanged.
 void adopt_supply(ParticleSystem world){world_=std::move(world);}
 const ParticleSystem& world()const{return world_;}
 const StepReport& report()const{return report_;}
 std::uint64_t steps()const{return steps_;}
 double simulation_seconds()const{return static_cast<double>(steps_)*dt_;}
 double dt()const{return dt_;}
 bool running()const{return running_;}
 const std::string& error()const{return error_;}
private:
 bool advance_one(){
  if(!error_.empty())return false;
  try{report_=world_.step(dt_);++steps_;return true;}
  catch(const std::exception& e){error_=e.what();running_=false;return false;}
 }
 ParticleSystem initial_,world_;
 double dt_;
 std::uint64_t steps_=0;
 bool running_=false;
 std::string error_;
 StepReport report_;
};
}
