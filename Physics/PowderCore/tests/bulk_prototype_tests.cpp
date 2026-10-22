#include "powder/bulk_prototype.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
using namespace powder;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
 BulkPrototype::Config c;c.gravity=0;c.markers=1000;BulkPrototype model(c);auto initial=model.masses();model.air=2;model.valve_open=false;
 for(unsigned i=0;i<4000;++i)model.advance(.005);
 for(unsigned i=model.valve;i<c.cells;++i)check(BulkPrototype::total(model.cells[i])==0,"closed valve leaked mass");
 for(auto p:model.markers)check(p.x<model.valve*model.dx()&&std::abs(p.y)<.2&&std::abs(p.z)<.2,"marker crossed wall/valve");
 auto mass=model.masses();for(unsigned g=0;g<3;++g)check(std::abs(mass[g]-initial[g])<1e-10,"group conservation");
 model.valve_open=true;for(unsigned i=0;i<2000;++i)model.advance(.005);double downstream=0;for(unsigned i=model.valve;i<c.cells;++i)downstream+=BulkPrototype::total(model.cells[i]);check(downstream>1,"opening has no effect");
 model.air=-2;model.advance(.5);check(model.velocity<0,"drag does not oppose relative motion");
 BulkPrototype exact(c);exact.air=2;exact.advance(.1);check(std::abs(exact.velocity-2*(1-std::exp(-1.)))<1e-12,"drag analytic solution");
 BulkPrototype noMarkers({128,0,30}),withMarkers({128,2500,30});for(unsigned i=0;i<1000;++i){noMarkers.advance(.005);withMarkers.advance(.005);}check(noMarkers.cells==withMarkers.cells,"display changes physics");
 bool rejected=false;auto before=exact.cells;try{exact.advance(-1);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&before==exact.cells,"invalid dt atomicity");

 BulkPrototype coarse(c),fine(c);for(unsigned i=0;i<400;++i)coarse.advance(.005);for(unsigned i=0;i<800;++i)fine.advance(.0025);
 auto centre=[](const BulkPrototype& b){double moment=0,mass=0;for(unsigned i=0;i<b.cells.size();++i){double v=BulkPrototype::total(b.cells[i]);mass+=v;moment+=v*(i+.5)*b.dx();}return moment/mass;};
 check(std::abs(centre(coarse)-centre(fine))<.02,"time refinement drift");
 BulkPrototype resting(c);resting.air=0;auto still=resting.cells;resting.advance(.5);check(still==resting.cells,"zero force creates flow");
 if(argc==2){std::ofstream out(argv[1]);out<<"mass_kg,cells,markers,repeat,physical_s,wall_s,rate,max_group_error\n"<<std::setprecision(17);
 for(double kg:{10.,30.,50.})for(unsigned n:{64u,128u,512u})for(unsigned repeat=0;repeat<3;++repeat){BulkPrototype sim({n,1500,kg});auto startMass=sim.masses();auto start=std::chrono::steady_clock::now();for(unsigned step=0;step<12000;++step){sim.valve_open=step>=2000;sim.air=step<8000?1:-.5;sim.advance(.005);}double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),error=0;auto endMass=sim.masses();for(unsigned g=0;g<3;++g)error=std::max(error,std::abs(endMass[g]-startMass[g]));check(error<1e-9,"benchmark mass error");for(auto m:sim.cells){check(BulkPrototype::total(m)<=sim.capacity()+1e-10,"overcapacity");for(double v:m)check(v>=-1e-12&&std::isfinite(v),"invalid mass");}out<<kg<<','<<n<<",1500,"<<repeat<<",60,"<<wall<<','<<60/wall<<','<<error<<'\n';}check(bool(out),"write failed");}
 std::cout<<"PASS bulk prototype: mass, capacity, valve, markers, drag, display independence\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
