#include "powder/plant.hpp"
#include "powder/plant_empty.hpp"
#include "powder/plant_mixer_obstacle.hpp"
#include <chrono>
#include <iostream>

namespace { void check(bool b,const char* s){if(!b)throw std::runtime_error(s);} }
int main(int argc,char** argv){try{
 check(argc==2,"data directory required");
 using namespace powder;using namespace powder::mixer_obstacle;
 // A known radial face: first contact occurs just short of a quarter turn.
 const std::vector<Triangle> face{{{{-.1,.5,0},{.1,.5,0},{0,.6,0}}}};
 MixerObstacleBox simple;simple.centre={0,0,.5};simple.half={.2,.04,.04};
 auto stops=build(face,{0,0,0},{simple});
 check(stops.enabled&&stops.upper_rad>1.47&&stops.upper_rad<1.50,"geometric contact angle");
 for(int i=0;i<2000;++i){double a=stops.lower_rad+(stops.upper_rad-stops.lower_rad)*i/1999.;
  check(!intersects(face[0],simple,{0,0,0},a),"free sweep intersects obstacle");}
 check(intersects(face[0],simple,{0,0,0},stops.upper_rad+.01),"stop unrelated to contact");
 auto away=simple;away.centre[0]=2;check(!build(face,{0,0,0},{away}).enabled,"disjoint X slab blocked rotor");
 away=simple;away.centre[2]=2;check(!build(face,{0,0,0},{away}).enabled,"outside swept radius blocked rotor");
 auto overlap=simple;overlap.centre={0,.5,0};bool refused=false;
 try{build(face,{0,0,0},{overlap});}catch(const std::invalid_argument&){refused=true;}
 check(refused,"initial overlap silently accepted");
 // A narrow obstacle between angular samples must not be skipped.
 auto thin=simple;thin.half={.02,.000002,.000002};thin.centre={0,.5*std::cos(.30031),.5*std::sin(.30031)};
 auto thinStop=build(face,{0,0,0},{thin});check(thinStop.enabled&&thinStop.upper_rad<.30031&&thinStop.upper_rad>.299,"thin obstacle tunnel");
 // Rotating the OBB axes by 45 degrees changes the actual contact, not just its AABB.
 auto rotated=simple;double q=std::sqrt(.5);rotated.axes={{{q,q,0},{-q,q,0},{0,0,1}}};
 auto tilted=build(face,{0,0,0},{rotated});check(tilted.enabled&&std::abs(tilted.upper_rad-stops.upper_rad)>.01,"OBB rotation ignored");

 auto data=read_plant_data(std::string(argv[1])+"/runtime.plant");
 MixerObstacleBox block;block.centre={2.38,0,.715};block.half={.10,.09,.09};
 auto begin=std::chrono::steady_clock::now();auto actual=build_mixer_obstacle_stops(data,{block});
 std::cout<<"actual block limits ["<<actual.lower_rad<<", "<<actual.upper_rad<<"] rad; build_s="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<std::endl;
 check(actual.enabled&&actual.lower_rad<-.001&&actual.upper_rad>.001,"actual level block has no usable clearance");
 auto triangles=rotor_triangles(data);auto pivot=point(data[1]);
 for(const auto& t:triangles)for(double a:{0.,double(actual.lower_rad),double(actual.upper_rad)})
  check(!intersects(t,block,pivot,a),"actual rotor penetrates at contact limit");
 bool crossed=false;for(const auto& t:triangles)crossed|=intersects(t,block,pivot,actual.upper_rad+.001);
 check(crossed,"actual stop not on paddle surface");

 GranularConfig c;c.plant=true;c.plant_data=data;c.count=0;c.dt=.001f;c.plant_powered_mixer=true;c.mixer_stops=actual;
 EmptyPlantDrive drive(c);double maxReaction=0,atContact=0;
 for(int i=0;i<7000;++i){drive.advance(c.dt,15);auto s=drive.mixer;
  check(s.angle_rad<=actual.upper_rad&&s.angle_rad>=actual.lower_rad,"CPU crossed geometry limit");
  check(std::abs(s.motor_nm)<=750.01&&s.electrical_w<=1302.01,"CPU exceeded ratings");
  maxReaction=std::max(maxReaction,std::abs(double(s.contact_nm)));
  if(s.contact_nm<0&&s.stopped()&&!s.tripped){atContact+=c.dt;check(std::abs(s.contact_nm+s.motor_nm)<.1||atContact<.002,"holding torque not balanced by reaction");}
 }
 check(drive.mixer.stopped()&&drive.mixer.tripped&&maxReaction>700&&atContact>1.9,"CPU obstruction did not stall/trip");
 std::cout<<"CPU stall angle="<<drive.mixer.angle_rad<<" peak reaction_Nm="<<maxReaction<<" trip="<<drive.mixer.tripped<<std::endl;
 drive.advance(c.dt,0);check(!drive.mixer.tripped,"stop did not acknowledge trip");
 double held=drive.mixer.angle_rad;for(int i=0;i<200;++i)drive.advance(c.dt,-15);
 check(drive.mixer.angle_rad<held-.001&&drive.mixer.contact_nm==0,"reverse did not release contact");
 drive.reset();drive.mixer.omega_rad_s=100;drive.advance(.001,15);
 check(drive.mixer.angle_rad<=actual.upper_rad,"large step crossed obstacle");
 for(int i=0;i<100;++i)drive.advance(.001,15);
 check(drive.mixer.angle_rad<=actual.upper_rad,"inertial impact tunneled");
 auto removed=c;removed.mixer_stops={};EmptyPlantDrive free(removed);
 for(int i=0;i<5000;++i)free.advance(.001,15);
 check(free.mixer.angle_rad>actual.upper_rad+1&&std::abs(free.mixer.rpm()-15)<.1,"removing obstacle did not restore free drive");

 // Real OpenCL drive with one distant particle: same angular constraint and
 // reaction, with short test trip time to avoid a long GPU soak.
 auto gpuConfig=c;gpuConfig.count=1;gpuConfig.radius=.01f;gpuConfig.dt=.00025f;gpuConfig.iterations=4;
 gpuConfig.plant_threaded=true;gpuConfig.plant_wall_mode=5;
 gpuConfig.mixer_drive.stiffness_n_m=1000;gpuConfig.mixer_drive.ramp_seconds=.01f;gpuConfig.mixer_drive.trip_seconds=.03f;
 auto gate=data[int(data[5].x)];std::vector<Grain> particle{{gate.x,gate.y,gate.z+1,0}},velocity(1);
 GranularGPU gpu(gpuConfig,particle,velocity);EmptyPlantDrive cpu(gpuConfig);
 bool sawReaction=false;
 for(int batch=0;batch<40;++batch){gpu.advance(100,15,false);for(int i=0;i<100;++i)cpu.advance(gpuConfig.dt,15);
  auto s=gpu.plant_mixer_state();sawReaction|=s.contact_nm<0;
  check(s.angle_rad<=actual.upper_rad+1.e-7&&s.angle_rad>=actual.lower_rad-1.e-7,"OpenCL crossed contact limit");
  check(std::abs(s.angle_rad-cpu.mixer.angle_rad)<.001&&std::abs(s.rpm()-cpu.mixer.rpm())<.05,"CPU/OpenCL obstacle mismatch");
 }
 auto s=gpu.plant_mixer_state();check(sawReaction&&s.stopped()&&s.tripped,"OpenCL obstacle did not stall/trip");
 std::cout<<"OpenCL stall angle="<<s.angle_rad<<" trip="<<s.tripped<<std::endl;
 gpu.advance(1,0,false);for(int i=0;i<20;++i)gpu.advance(100,-15,false);
 check(gpu.plant_mixer_state().angle_rad<actual.upper_rad-.001,"OpenCL reverse locked");
 gpu.set_state(particle,velocity);check(gpu.plant_mixer_state().angle_rad==0&&!gpu.plant_mixer_state().tripped,"reset retained obstruction state");
 std::cout<<"PASS actual geometry, continuous sweep, rotated boxes, fixed reaction, trip, reverse, reset, CPU/OpenCL\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
