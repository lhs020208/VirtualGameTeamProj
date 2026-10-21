#include "flow/solver.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr double pi=3.14159265358979323846;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
double pressure_error(int nx,int ny,bool mixed){
    flow::Contract c;c.grid.nx=nx;c.grid.ny=ny;c.dt_s=.0001;
    if(mixed)c.boundaries[0]={flow::BoundaryKind::ParabolicVelocity,.02};
    flow::Layout l(c);std::vector<double> rhs(l.pressure_count()),exact(rhs.size()),left(ny),right(ny);
    const double kx=(mixed?.5:1)*pi/c.grid.length_m,ky=2*pi/c.grid.height_m;
    for(int j=0;j<ny;++j){const double y=(j+.5)*c.grid.height_m/ny;
        right[j]=mixed?0:1;left[j]=0;
        for(int i=0;i<nx;++i){const double x=(i+.5)*c.grid.length_m/nx;const auto k=l.p_index(i,j);
            const double wave=(mixed?std::cos(kx*x):std::sin(kx*x))*std::cos(ky*y);
            exact[k]=wave+(mixed?0:x/c.grid.length_m);rhs[k]=(kx*kx+ky*ky)*wave;
        }
    }
    const auto solved=flow::solve_pressure(c,rhs,left,right,std::vector<double>(rhs.size()));
    double error=0,scale=0;for(std::size_t k=0;k<rhs.size();++k){error+=std::pow(solved.pressure[k]-exact[k],2);scale+=exact[k]*exact[k];}
    return std::sqrt(error/scale);
}
double profile_error(const flow::Contract& c,const flow::Fields& f){
    flow::Layout l(c);double error=0,scale=0;
    for(int j=0;j<c.grid.ny;++j){const double ref=flow::analytic_velocity(c,(j+.5)*c.grid.height_m/c.grid.ny);
        for(int i=0;i<=c.grid.nx;++i){error+=std::pow(f.u_m_s[l.u_index(i,j)]-ref,2);scale+=ref*ref;}}
    return std::sqrt(error/scale);
}
double transient_reference(const flow::Contract& c,double y,double time){
    const auto d=flow::validate(c);const double h=c.grid.height_m,g=d.estimated_pressure_drop_pa/c.grid.length_m;
    double sum=0;for(int n=1;n<201;n+=2)sum+=std::sin(n*pi*y/h)*std::exp(-d.nu_m2_s*n*n*pi*pi*time/(h*h))/(static_cast<double>(n)*n*n);
    return g*y*(h-y)/(2*flow::ViscosityPaS)-4*g*h*h/(flow::ViscosityPaS*pi*pi*pi)*sum;
}
double run_transient(double dt,flow::Fields* result=nullptr){
    flow::Contract c;c.grid.nx=64;c.grid.ny=32;c.dt_s=dt;c.duration_s=.1;
    auto f=flow::make_initial_fields(c);const auto steps=flow::validate(c).steps;
    for(std::uint64_t k=0;k<steps;++k)flow::advance(c,f);
    flow::Layout l(c);double error=0,scale=0;
    for(int j=0;j<c.grid.ny;++j){double ref=transient_reference(c,(j+.5)*c.grid.height_m/c.grid.ny,c.duration_s);error+=std::pow(f.u_m_s[l.u_index(16,j)]-ref,2);scale+=ref*ref;}
    if(result)*result=f;return std::sqrt(error/scale);
}
}
int main(int argc,char** argv){try{
    std::ostream* output=&std::cout;std::ofstream file;if(argc==2){file.open(argv[1]);require(bool(file),"cannot open test metrics");output=&file;}
    auto& o=*output;o<<std::setprecision(17)<<"case,value,auxiliary\n";
    flow::Contract zero;zero.boundaries[0].value=zero.boundaries[1].value;zero.duration_s=.01;
    auto z=flow::make_initial_fields(zero);
    for(int k=0;k<20;++k)flow::advance(zero,z);
    require(flow::diagnostics(zero,z).max_speed_m_s==0,"zero drive moved");o<<"zero_drive,0,0\n";
    for(bool mixed:{false,true}){
        double previous=0;for(int ny:{8,16,32}){double error=pressure_error(4*ny,ny,mixed);o<<(mixed?"poisson_mixed_":"poisson_dirichlet_")<<ny<<','<<error<<','<<(previous?previous/error:0)<<'\n';
            if(previous)require(previous/error>3&&previous/error<5,"Poisson spatial convergence not second order");previous=error;
        }require(previous<.01,"fine Poisson reference error");
    }
    double previous=0;
    for(int ny:{8,16,32}){
        flow::Contract c;c.grid.nx=4*ny;c.grid.ny=ny;c.duration_s=10;c.dt_s=.0005;
        auto f=flow::make_initial_fields(c);double divergence=0,mass=0,residual=0;
        for(std::uint64_t k=0;k<flow::validate(c).steps;++k){auto r=flow::advance(c,f);divergence=std::max(divergence,r.divergence_linf_s_inv);mass=std::max(mass,r.mass_balance_relative);residual=std::max(residual,r.pressure_residual_pa_m2);}
        const double error=profile_error(c,f);o<<"steady_velocity_"<<ny<<','<<error<<','<<(previous?previous/error:0)<<'\n';
        if(previous)require(previous/error>3&&previous/error<5,"steady spatial convergence not second order");previous=error;
        flow::Layout l(c);const auto d=flow::validate(c);double pe=0;
        for(int j=0;j<ny;++j)for(int i=0;i<c.grid.nx;++i)pe=std::max(pe,std::abs(f.pressure_gauge_pa[l.p_index(i,j)]-flow::analytic_pressure_gauge(c,l.p_position(i,j)[0]))/d.estimated_pressure_drop_pa);
        require(pe<.01,"steady pressure error");o<<"steady_pressure_"<<ny<<','<<pe<<",0\n";
        o<<"divergence_"<<ny<<','<<divergence<<",0\n"<<"mass_balance_"<<ny<<','<<mass<<",0\n"<<"residual_"<<ny<<','<<residual<<",0\n";
    }
    require(previous<.01,"fine steady velocity error");
    flow::Fields t1,t2,t3;double e1=run_transient(.0005,&t1),e2=run_transient(.00025,&t2),e3=run_transient(.000125,&t3);
    double diff12=0,diff23=0;for(std::size_t k=0;k<t1.u_m_s.size();++k){diff12+=std::pow(t1.u_m_s[k]-t2.u_m_s[k],2);diff23+=std::pow(t2.u_m_s[k]-t3.u_m_s[k],2);}
    double ratio=std::sqrt(diff12/diff23);require(ratio>1.8&&ratio<2.2,"Euler time convergence");
    require(e3<.01,"transient series error");o<<"transient_dt_0.0005,"<<e1<<",0\ntransient_dt_0.00025,"<<e2<<",0\ntransient_dt_0.000125,"<<e3<<",0\ntime_self_convergence,"<<ratio<<",0\n";
    flow::Contract vel;vel.grid.nx=32;vel.grid.ny=16;vel.boundaries[0]={flow::BoundaryKind::ParabolicVelocity,.02};vel.duration_s=1;
    auto vf=flow::make_initial_fields(vel);double maxmass=0;
    for(std::uint64_t k=0;k<flow::validate(vel).steps;++k){auto r=flow::advance(vel,vf);maxmass=std::max(maxmass,r.mass_balance_relative);}
    auto vr=flow::diagnostics(vel,vf);require(std::abs(vr.inlet_mass_kg_s/flow::validate(vel).analytic_mass_flow_kg_s-1)<1e-12,"velocity inlet flux integral");
    flow::Layout vl(vel);double grad=0;for(int j=0;j<vel.grid.ny;++j)grad+=(vf.pressure_gauge_pa[vl.p_index(vel.grid.nx-3,j)]-vf.pressure_gauge_pa[vl.p_index(vel.grid.nx-2,j)])/(vel.grid.length_m/vel.grid.nx)/vel.grid.ny;
    const double gradient_error=std::abs(grad/(flow::validate(vel).estimated_pressure_drop_pa/vel.grid.length_m)-1);
    require(gradient_error<.01,"velocity-driven pressure gradient reference");o<<"velocity_inlet_mass_error,"<<maxmass<<",0\nvelocity_pressure_gradient_error,"<<gradient_error<<",0\n";
    // A genuinely two-dimensional perturbation exercises v momentum and projection.
    flow::Contract pert;pert.grid.nx=32;pert.grid.ny=16;pert.initial=flow::InitialCondition::SteadyAnalyticReference;pert.duration_s=.005;
    auto pf=flow::make_initial_fields(pert);flow::Layout pl(pert);
    for(int j=1;j<pert.grid.ny;++j)for(int i=0;i<pert.grid.nx;++i){const auto xy=pl.v_position(i,j);pf.v_m_s[pl.v_index(i,j)]=.0001*std::sin(pi*xy[0]/pert.grid.length_m)*std::sin(pi*xy[1]/pert.grid.height_m);}
    auto before=pf;auto first=flow::advance(pert,pf);require(first.divergence_linf_s_inv<pert.projection.divergence_linf_s_inv,"2D projection failed");require(pf.v_m_s!=before.v_m_s,"transverse momentum unchanged");
    auto repeat=before;flow::advance(pert,repeat);require(repeat.u_m_s==pf.u_m_s&&repeat.v_m_s==pf.v_m_s&&repeat.pressure_gauge_pa==pf.pressure_gauge_pa,"repeat not deterministic");
    o<<"two_dimensional_projection,"<<first.divergence_linf_s_inv<<','<<first.mass_balance_relative<<'\n';
    auto failc=pert;failc.projection.max_iterations=1;auto failed=before;bool caught=false;try{flow::advance(failc,failed);}catch(const std::exception&){caught=true;}
    require(caught&&failed.u_m_s==before.u_m_s&&failed.v_m_s==before.v_m_s&&failed.pressure_gauge_pa==before.pressure_gauge_pa&&failed.accepted_steps==before.accepted_steps,"failed step mutated state");
    auto invalid=before;invalid.v_m_s[0]=1;auto saved=invalid;caught=false;try{flow::advance(pert,invalid);}catch(const std::exception&){caught=true;}
    require(caught&&invalid.v_m_s==saved.v_m_s,"invalid field was accepted");
    o<<"transactional_failure,1,0\n";
    // A common pressure offset changes no gradient (keep density reference fixed).
    flow::Contract offset=pert;offset.boundaries[0].value+=100;offset.boundaries[1].value+=100;
    auto shifted=before;for(auto& p:shifted.pressure_gauge_pa)p+=100;
    flow::advance(offset,shifted);double shift_error=0;for(std::size_t k=0;k<pf.u_m_s.size();++k)shift_error=std::max(shift_error,std::abs(pf.u_m_s[k]-shifted.u_m_s[k]));
    require(shift_error<1e-10,"pressure offset changed velocity");o<<"pressure_offset_velocity_difference,"<<shift_error<<",0\n";
    auto complete=z;caught=false;try{flow::advance(zero,complete);}catch(const std::exception&){caught=true;}
    require(caught&&complete.accepted_steps==20,"advanced past requested duration");
    std::cout<<"PASS flow solver numerical verification\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
