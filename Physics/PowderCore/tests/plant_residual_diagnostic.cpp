#include "powder/plant_pressure.hpp"
#include <iostream>
#include <iomanip>
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("data output.csv");
 auto d=powder::read_plant_data(std::string(argv[1])+"/runtime.plant");auto routes=powder::read_plant_routes(std::string(argv[1])+"/runtime.routes");
 powder::plant_set_regulated_pressures(d,routes,{500,-500,0},2,2,false,false,false);
 powder::GranularConfig c;c.plant=true;c.plant_threaded=true;c.kernel_optimize=true;c.plant_wall_mode=5;c.plant_data=d;c.radius=.002f;c.dt=.00001f;c.iterations=8;c.count=1;
 std::vector<powder::Grain> p={{.306363791227f,1.40037477016f,2.02166008949f,1}};
 powder::GranularGPU g(c,p,{{0,0,0,0}});std::ofstream out(argv[2]);out<<std::setprecision(12)<<"sim_s,x,y,z,vx,vy,vz,nx,ny,nz\n";
 for(int i=0;i<=10000;i+=100){auto q=g.snapshot();auto v=g.velocity_snapshot();auto h=g.inspect_boundaries(q,0,true,true);out<<g.time()<<','<<q[0].x<<','<<q[0].y<<','<<q[0].z<<','<<v[0].x<<','<<v[0].y<<','<<v[0].z<<','<<h[0].normal.x<<','<<h[0].normal.y<<','<<h[0].normal.z<<'\n';if(i<10000)g.advance(100,0,false);}
 std::cout<<"Diagnostic only: isolated residual position, zero initial velocity, 0.1 seconds; full PIE run remains separate."<<std::endl;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
