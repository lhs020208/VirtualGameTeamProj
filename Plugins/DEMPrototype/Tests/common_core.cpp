#define main legacy_main
#include "validate.cpp"
#undef main
#define dem dem_before
#include "../CommonValidation/Before/Source/DEMPrototype/Public/DEMSolver.h"
#undef dem

std::vector<Particle> initial(Config c,int count,double z=.3)
{
    Solver mass; mass.c=c;
    std::vector<Particle> out;
    for(int i=0;i<count;++i) {Particle a;a.id=i;a.r=c.r;mass.setMass(a);
        a.p={((i%10)-4.5)*.021,(((i/10)%10)-4.5)*.021,z+(i/100)*.021};out.push_back(a);}
    return out;
}
std::vector<Plane> box()
{
    return {{0,{0,0,0},{1,0,0},{0,1,0},.15,.15,true},
    {1,{-.15,0,.125},{0,1,0},{0,0,1},.15,.125,false},
    {2,{.15,0,.125},{0,1,0},{0,0,1},.15,.125,false},
    {3,{0,-.15,.125},{1,0,0},{0,0,1},.15,.125,false},
    {4,{0,.15,.125},{1,0,0},{0,0,1},.15,.125,false}};
}
struct Metrics { double ke=0,height=0,support=0,overlap=0,mass=0;uint64_t crossings=0,escapes=0; };
Metrics finiteDeposit(Config c,const std::string& name,const std::string& dir)
{
    Solver s;s.resetExternal(c,initial(c,200),box());Metrics m;m.mass=s.s.mass;
    std::ofstream csv(dir+"/"+name+"_trace.csv");csv<<std::setprecision(17)<<"time,ke,height,support,overlap,contacts,candidates,crossings,escapes\n";
    const uint64_t steps=uint64_t(std::llround(5/c.dt)); int samples=0;
    for(uint64_t i=0;i<steps&&!s.failed;++i)
    {
        s.step();m.overlap=std::max(m.overlap,s.s.maxOverlap);m.crossings=s.boundaryCrossings;
        uint64_t escaped=0;
        for(const auto&a:s.p) if(a.p.z<0 || std::abs(a.p.x)>.15+a.r || std::abs(a.p.y)>.15+a.r) ++escaped;
        m.escapes=std::max(m.escapes,escaped);
        if(s.s.time>4) {m.ke+=s.s.ke+s.s.rotKE;m.support+=s.s.support;++samples;}
        if(i%uint64_t(std::llround(.01/c.dt))==0) csv<<s.s.time<<","<<s.s.ke+s.s.rotKE<<","<<s.s.height<<","<<s.s.support<<","<<s.s.maxOverlap<<","<<s.s.contacts<<","<<s.s.candidates<<","<<s.boundaryCrossings<<","<<escaped<<"\n";
    }
    m.ke/=std::max(1,samples);m.support/=std::max(1,samples);m.height=s.s.height;
    check((name+"_5seconds").c_str(),s.s.steps==steps,s.s.time,5);
    check((name+"_mass_count").c_str(),s.p.size()==200 && s.s.mass==m.mass,s.s.mass,m.mass);
    check((name+"_finite").c_str(),!s.failed,s.s.errors,0);
    check((name+"_crossings").c_str(),m.crossings==0,m.crossings,0);
    check((name+"_escapes").c_str(),m.escapes==0,m.escapes,0);
    check((name+"_overlap").c_str(),m.overlap<.01,m.overlap,.01);
    check((name+"_settled_ke").c_str(),m.ke<1e-5,m.ke,1e-5);
    check((name+"_support_weight").c_str(),std::abs(m.support-m.mass*9.81)/(m.mass*9.81)<.1,m.support,m.mass*9.81);
    check((name+"_no_step_allocation").c_str(),s.s.growths==0,s.s.growths,0);
    std::cout<<name<<" ke="<<m.ke<<" support="<<m.support<<" overlap="<<m.overlap<<std::endl;
    return m;
}
int main(int argc,char**argv)
{
    std::string dir=argc>1?argv[1]:".";
    checks.open(dir+"/core_checks.csv");checks<<"test,status,value,criterion\n";
    trace.open(dir+"/legacy_trace.csv");bench.open(dir+"/legacy_performance.csv");
    basic();
    { Config c;Solver n;n.reset(c);dem_before::Config old;dem_before::Solver b;b.reset(old);
      for(int i=0;i<3000;++i){n.step();b.step();}
      double e=0;for(size_t i=0;i<n.p.size();++i)e=std::max(e,std::abs(n.p[i].p.z-b.p[i].p.z)+std::abs(n.p[i].v.x-b.p[i].v.x));
      check("legacy_before_after_exact",e==0,e,0); }
    Config c;c.kn=500000;c.kt=250000;c.dt=1e-5;c.walls=false;
    {auto p=initial(c,1,1);p[0].p={0,0,1};Solver s;s.resetExternal(c,p,{});run(s,.1);
     double err=std::abs(s.p[0].p.z-(1-.5*9.81*.01));check("external_freefall_position",std::abs(err-.5*9.81*.1*c.dt)<1e-11,err,.5*9.81*.1*c.dt);
     check("external_freefall_velocity",std::abs(s.p[0].v.z+.981)<1e-11,s.p[0].v.z,-.981);}
    {auto p=initial(c,2,1);p[0].p={-.011,0,1};p[1].p={.011,0,1};p[0].v={1,.1,0};p[1].v={-1,-.1,0};Config cfg=c;cfg.gravity={};Solver s;s.resetExternal(cfg,p,{});
     double energy=s.s.ke,maxC=0;for(int i=0;i<3000;++i){s.step();maxC=std::max(maxC,s.s.maxCoulombRatio);}
     check("external_pair_momentum",(s.p[0].v*s.p[0].m+s.p[1].v*s.p[1].m).norm()<1e-12,0,0);
     check("external_pair_dissipation",s.s.ke+s.s.rotKE<energy,s.s.ke+s.s.rotKE,energy);check("external_pair_coulomb",maxC<=1+1e-12,maxC,1);}
    {auto p=initial(c,1);p[0].p={0,0,.0099};p[0].v={.1,0,0};Config cfg=c;cfg.gravity={};Solver s;s.resetExternal(cfg,p,{box()[0]});s.step();
     check("finite_face_friction_torque",s.p[0].f.x<0&&s.p[0].t.y>0,s.p[0].t.y,0);
     s.p[0].p={0,0,1};s.step();check("finite_history_expiry",s.history.empty(),s.history.size(),0);}
    {Plane f;f.id=19;f.center={.3,-.2,.4};f.u={std::sqrt(.5),0,std::sqrt(.5)};f.v={0,1,0};f.halfU=.1;f.halfV=.2;
     auto sample=[&](V at){auto p=initial(c,1);p[0].p=at;Config cfg=c;cfg.gravity={};Solver s;s.resetExternal(cfg,p,{f});s.step();return s;};
     auto inside=sample(f.center+f.normal()*.009);check("rotated_face_interior",inside.s.contacts==1&&inside.p[0].f.dot(f.normal())>0,inside.s.contacts,1);
     auto edge=sample(f.center+f.u*.105+f.normal()*.005);check("rotated_face_edge",edge.s.contacts==1&&edge.p[0].f.dot(f.u)>0,edge.s.contacts,1);
     auto corner=sample(f.center+f.u*.104+f.v*.204+f.normal()*.004);check("rotated_face_corner",corner.s.contacts==1&&corner.p[0].f.dot(f.v)>0,corner.s.contacts,1);
     auto outside=sample(f.center+f.u*.12+f.normal()*.001);check("finite_outside_no_contact",outside.s.contacts==0,outside.s.contacts,0);
     auto back=sample(f.center-f.normal()*.009);check("two_sided_back",back.s.contacts==1&&back.p[0].f.dot(f.normal())<0,back.s.contacts,1);}
    {Config cfg=c;cfg.gravity={};auto p=initial(c,1);Solver a,b;a.resetExternal(cfg,p,{});b.resetExternal(cfg,p,{});b.setExternalLoad(0,{},{});run(a,.1);run(b,.1);
     check("zero_load_exact",(a.p[0].p-b.p[0].p).norm2()==0,0,0);
     b.resetExternal(cfg,p,{});b.setExternalLoad(0,{p[0].m*2,0,0},{0,0,p[0].I*3});run(b,.1);
     check("constant_force_velocity",std::abs(b.p[0].v.x-.2)<1e-11,b.p[0].v.x,.2);
     check("constant_force_position",std::abs(b.p[0].p.x-p[0].p.x-(.01+.1*c.dt))<1e-11,b.p[0].p.x-p[0].p.x,.01+.1*c.dt);
     check("constant_torque_spin",std::abs(b.p[0].w.z-.3)<1e-11,b.p[0].w.z,.3);
     check("bad_load_id",!b.setExternalLoad(2,{},{}),b.failed,1);b.resetExternal(cfg,p,{});
     check("bad_load_nan",!b.setExternalLoad(0,{NAN,0,0},{}),b.failed,1);}
    auto a=finiteDeposit(c,"dt",dir);c.dt*=.5;auto b=finiteDeposit(c,"half",dir);
    check("dt_height_5pct",std::abs(a.height-b.height)/b.height<.05,std::abs(a.height-b.height)/b.height,.05);
    check("dt_support_5pct",std::abs(a.support-b.support)/b.support<.05,std::abs(a.support-b.support)/b.support,.05);
    check("dt_energy_absolute",std::abs(a.ke-b.ke)<1e-5,std::abs(a.ke-b.ke),1e-5);
    check("dt_overlap_absolute",std::abs(a.overlap-b.overlap)<.002,std::abs(a.overlap-b.overlap),.002);
    std::ofstream json(dir+"/core_summary.json");json<<std::setprecision(17)<<"{\"failures\":"<<failures<<",\"dt\":{\"ke\":"<<a.ke<<",\"height\":"<<a.height<<",\"support\":"<<a.support<<",\"max_overlap\":"<<a.overlap<<",\"mass\":"<<a.mass<<",\"crossings\":"<<a.crossings<<",\"escapes\":"<<a.escapes<<"},\"half\":{\"ke\":"<<b.ke<<",\"height\":"<<b.height<<",\"support\":"<<b.support<<",\"max_overlap\":"<<b.overlap<<"}}\n";
    return failures?1:0;
}
