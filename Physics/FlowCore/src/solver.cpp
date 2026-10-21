#include "flow/solver.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace flow {
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double dot(const std::vector<double>& a,const std::vector<double>& b){
    double sum=0;for(std::size_t k=0;k<a.size();++k)sum+=a[k]*b[k];return sum;
}
double norm(const std::vector<double>& a){double v=0;for(double x:a){check(std::isfinite(x),"nonfinite pressure iteration");v=std::max(v,std::abs(x));}return v;}
struct Operator {
    int nx,ny;double ax,ay;bool left_dir;
    std::size_t p(int i,int j)const{return static_cast<std::size_t>(j)*nx+i;}
    double diagonal(int i,int j)const{
        return (i>0?ax:(left_dir?2*ax:0))+(i+1<nx?ax:2*ax)+(j>0?ay:0)+(j+1<ny?ay:0);
    }
    void apply(const std::vector<double>& x,std::vector<double>& y)const{
        for(int j=0;j<ny;++j)for(int i=0;i<nx;++i){const auto k=p(i,j);double v=0;
            if(i>0)v+=ax*(x[k]-x[p(i-1,j)]);else if(left_dir)v+=2*ax*x[k];
            if(i+1<nx)v+=ax*(x[k]-x[p(i+1,j)]);else v+=2*ax*x[k];
            if(j>0)v+=ay*(x[k]-x[p(i,j-1)]);
            if(j+1<ny)v+=ay*(x[k]-x[p(i,j+1)]);
            y[k]=v;
        }
    }
};
double flux(double speed,double minus,double plus){return speed*(speed>=0?minus:plus);}
}

PressureResult solve_pressure(const Contract& c,const std::vector<double>& source,
    const std::vector<double>& left,const std::vector<double>& right,
    const std::vector<double>& guess,double cap){
    const auto d=validate(c);const auto n=static_cast<std::size_t>(c.grid.nx)*c.grid.ny;
    check(source.size()==n&&guess.size()==n&&left.size()==static_cast<std::size_t>(c.grid.ny)&&right.size()==left.size(),"Poisson dimensions mismatch");
    check(std::isfinite(cap)&&cap>0,"invalid residual cap");
    norm(source);norm(guess);norm(left);norm(right);
    Operator a{c.grid.nx,c.grid.ny,1/(d.dx_m*d.dx_m),1/(d.dy_m*d.dy_m),c.boundaries[0].kind==BoundaryKind::PressureAbsolute};
    auto b=source;for(int j=0;j<a.ny;++j){if(a.left_dir)b[a.p(0,j)]+=2*a.ax*left[j];b[a.p(a.nx-1,j)]+=2*a.ax*right[j];}
    const double tolerance=std::min(cap,std::max(c.projection.residual_absolute_pa_m2,c.projection.residual_relative*norm(b)));
    PressureResult result;result.pressure=guess;
    std::vector<double> r(n),z(n),direction(n),ad(n);
    a.apply(result.pressure,ad);for(std::size_t k=0;k<n;++k)r[k]=b[k]-ad[k];
    result.residual_linf_pa_m2=norm(r);if(result.residual_linf_pa_m2<=tolerance)return result;
    auto precondition=[&](){for(int j=0;j<a.ny;++j)for(int i=0;i<a.nx;++i){auto k=a.p(i,j);z[k]=r[k]/a.diagonal(i,j);}};
    precondition();direction=z;double rz=dot(r,z);
    for(int iteration=1;iteration<=c.projection.max_iterations;++iteration){
        a.apply(direction,ad);const double denominator=dot(direction,ad);
        check(std::isfinite(denominator)&&denominator>0,"pressure CG breakdown");
        const double alpha=rz/denominator;
        for(std::size_t k=0;k<n;++k){result.pressure[k]+=alpha*direction[k];r[k]-=alpha*ad[k];}
        result.iterations=iteration;
        // Always verify the true residual before accepting; periodically replace
        // recursively accumulated residuals and restart conjugacy.
        const bool refresh=norm(r)<=tolerance||iteration%100==0;
        if(refresh){a.apply(result.pressure,ad);for(std::size_t k=0;k<n;++k)r[k]=b[k]-ad[k];}
        result.residual_linf_pa_m2=norm(r);
        if(result.residual_linf_pa_m2<=tolerance)return result;
        precondition();const double next=dot(r,z);check(std::isfinite(next)&&next>0,"invalid pressure residual");
        const double beta=refresh?0:next/rz;
        for(std::size_t k=0;k<n;++k)direction[k]=z[k]+beta*direction[k];rz=next;
    }
    throw std::runtime_error("pressure iteration limit reached; step not accepted");
}

StepReport diagnostics(const Contract& c,const Fields& f){
    validate_fields(c,f);const auto d=validate(c);Layout l(c);StepReport r;
    for(int j=0;j<c.grid.ny;++j){r.inlet_mass_kg_s+=f.u_m_s[l.u_index(0,j)];r.outlet_mass_kg_s+=f.u_m_s[l.u_index(c.grid.nx,j)];
        for(int i=0;i<c.grid.nx;++i){
            const double ul=f.u_m_s[l.u_index(i,j)],ur=f.u_m_s[l.u_index(i+1,j)],vb=f.v_m_s[l.v_index(i,j)],vt=f.v_m_s[l.v_index(i,j+1)];
            r.divergence_linf_s_inv=std::max(r.divergence_linf_s_inv,std::abs((ur-ul)/d.dx_m+(vt-vb)/d.dy_m));
            r.kinetic_energy_j+=.5*d.rho_kg_m3*d.dx_m*d.dy_m*c.grid.reporting_depth_m*(.25*(ul+ur)*(ul+ur)+.25*(vb+vt)*(vb+vt));
        }
    }
    const double scale=d.rho_kg_m3*d.dy_m*c.grid.reporting_depth_m;r.inlet_mass_kg_s*=scale;r.outlet_mass_kg_s*=scale;
    r.mass_balance_relative=std::abs(r.inlet_mass_kg_s-r.outlet_mass_kg_s)/std::max({std::abs(r.inlet_mass_kg_s),std::abs(r.outlet_mass_kg_s),1e-12});
    for(double u:f.u_m_s)r.max_speed_m_s=std::max(r.max_speed_m_s,std::abs(u));
    for(double v:f.v_m_s)r.max_speed_m_s=std::max(r.max_speed_m_s,std::abs(v));
    return r;
}

static StepReport advance_impl(const Contract& c,Fields& f,const MomentumSource* source){
    validate_fields(c,f);const auto d=validate(c);check(f.accepted_steps<d.steps,"requested simulation duration already reached");
    const int nx=c.grid.nx,ny=c.grid.ny;Layout l(c);Fields candidate=f;
    const auto ui=[nx](int i,int j){return static_cast<std::size_t>(j)*(nx+1)+i;};
    const auto vi=[nx](int i,int j){return static_cast<std::size_t>(j)*nx+i;};
    const auto u=[&](int i,int j){double sign=1;if(j<0){j=0;sign=-1;}if(j>=ny){j=ny-1;sign=-1;}if(i<0)i=1;if(i>nx)i=nx-1;return sign*f.u_m_s[ui(i,j)];};
    const auto v=[&](int i,int j){double sign=1;if(i<0){i=0;sign=-1;}if(i>=nx){i=nx-1;sign=-1;}j=std::clamp(j,0,ny);return sign*f.v_m_s[vi(i,j)];};
    for(int j=0;j<ny;++j)for(int i=0;i<=nx;++i){
        if(i==0&&c.boundaries[0].kind==BoundaryKind::ParabolicVelocity)continue;
        const double q=u(i,j),e=u(i+1,j),w=u(i-1,j),n=u(i,j+1),s=u(i,j-1);
        const double vn=.5*(v(i-1,j+1)+v(i,j+1)),vs=.5*(v(i-1,j)+v(i,j));
        const double convection=(flux(.5*(q+e),q,e)-flux(.5*(w+q),w,q))/d.dx_m+(flux(vn,q,n)-flux(vs,s,q))/d.dy_m;
        candidate.u_m_s[ui(i,j)]=q+c.dt_s*(-convection+d.nu_m2_s*((e-2*q+w)/(d.dx_m*d.dx_m)+(n-2*q+s)/(d.dy_m*d.dy_m)));
    }
    for(int j=1;j<ny;++j)for(int i=0;i<nx;++i){
        const double q=v(i,j),e=v(i+1,j),w=v(i-1,j),n=v(i,j+1),s=v(i,j-1);
        const double ue=.5*(u(i+1,j-1)+u(i+1,j)),uw=.5*(u(i,j-1)+u(i,j));
        const double convection=(flux(ue,q,e)-flux(uw,w,q))/d.dx_m+(flux(.5*(q+n),q,n)-flux(.5*(s+q),s,q))/d.dy_m;
        candidate.v_m_s[vi(i,j)]=q+c.dt_s*(-convection+d.nu_m2_s*((e-2*q+w)/(d.dx_m*d.dx_m)+(n-2*q+s)/(d.dy_m*d.dy_m)));
    }
    std::array<double,2> applied{{0,0}};
    if(source){
        check(source->u_force_n.size()==candidate.u_m_s.size()&&source->v_force_n.size()==candidate.v_m_s.size(),"momentum source dimensions mismatch");
        const double mass=d.rho_kg_m3*d.dx_m*d.dy_m*c.grid.reporting_depth_m;
        for(int j=0;j<ny;++j)for(int i=0;i<=nx;++i){
            const auto k=ui(i,j);const double force=source->u_force_n[k];check(std::isfinite(force),"nonfinite momentum source");
            if(i==0||i==nx){check(force==0,"open boundary force source unsupported");continue;}
            const double old=candidate.u_m_s[k];candidate.u_m_s[k]+=c.dt_s*force/mass;applied[0]+=mass*(candidate.u_m_s[k]-old);
        }
        for(int j=0;j<=ny;++j)for(int i=0;i<nx;++i){
            const auto k=vi(i,j);const double force=source->v_force_n[k];check(std::isfinite(force),"nonfinite momentum source");
            if(j==0||j==ny){check(force==0,"wall force source unsupported");continue;}
            const double old=candidate.v_m_s[k];candidate.v_m_s[k]+=c.dt_s*force/mass;applied[1]+=mass*(candidate.v_m_s[k]-old);
        }
    }
    std::vector<double> rhs(l.pressure_count());
    for(int j=0;j<ny;++j)for(int i=0;i<nx;++i)rhs[l.p_index(i,j)]=-d.rho_kg_m3/c.dt_s*((candidate.u_m_s[ui(i+1,j)]-candidate.u_m_s[ui(i,j)])/d.dx_m+(candidate.v_m_s[vi(i,j+1)]-candidate.v_m_s[vi(i,j)])/d.dy_m);
    const double left=c.boundaries[0].kind==BoundaryKind::PressureAbsolute?c.boundaries[0].value-c.reference_pressure_absolute_pa:0;
    const double right=c.boundaries[1].value-c.reference_pressure_absolute_pa;
    // Solve relative to the outlet to avoid cancellation from an arbitrary
    // pressure reference. Convert continuity tolerances into residual units;
    // a fixed tiny absolute tolerance is unattainable at impulsive startup.
    double inlet=0,outlet=0;
    for(int j=0;j<ny;++j){inlet+=candidate.u_m_s[ui(0,j)];outlet+=candidate.u_m_s[ui(nx,j)];}
    double flux_scale=d.rho_kg_m3*d.dy_m*c.grid.reporting_depth_m*std::max(std::abs(inlet),std::abs(outlet));
    flux_scale=std::max({flux_scale,1e-12,c.dt_s*d.estimated_pressure_drop_pa*c.grid.height_m*c.grid.reporting_depth_m/c.grid.length_m});
    const double volume=c.grid.length_m*c.grid.height_m*c.grid.reporting_depth_m;
    const double cap=.05*std::min(c.projection.divergence_linf_s_inv*d.rho_kg_m3/c.dt_s,c.projection.mass_balance_relative*flux_scale/(c.dt_s*volume));
    auto guess=f.pressure_gauge_pa;for(auto& q:guess)q-=right;
    const auto pressure=solve_pressure(c,rhs,std::vector<double>(ny,left-right),std::vector<double>(ny,0),guess,cap);
    candidate.pressure_gauge_pa=pressure.pressure;
    // Gradients use the shifted pressure; restore the user's reference only
    // after correction so its arbitrary offset cannot contaminate velocities.
    const auto p=[&](int i,int j){return candidate.pressure_gauge_pa[l.p_index(i,j)];};
    for(int j=0;j<ny;++j)for(int i=0;i<=nx;++i){
        if(i==0&&c.boundaries[0].kind==BoundaryKind::ParabolicVelocity)continue;
        const double gradient=i==0?2*(p(0,j)-(left-right))/d.dx_m:(i==nx?-2*p(nx-1,j)/d.dx_m:(p(i,j)-p(i-1,j))/d.dx_m);
        candidate.u_m_s[ui(i,j)]-=c.dt_s/d.rho_kg_m3*gradient;
    }
    for(int j=1;j<ny;++j)for(int i=0;i<nx;++i)candidate.v_m_s[vi(i,j)]-=c.dt_s/d.rho_kg_m3*(p(i,j)-p(i,j-1))/d.dy_m;
    for(auto& q:candidate.pressure_gauge_pa)q+=right;
    ++candidate.accepted_steps;candidate.physical_time_s=static_cast<double>(candidate.accepted_steps)*c.dt_s;
    auto report=diagnostics(c,candidate);report.applied_source_impulse_n_s=applied;report.pressure_iterations=pressure.iterations;report.pressure_residual_pa_m2=pressure.residual_linf_pa_m2;
    check(report.divergence_linf_s_inv<=c.projection.divergence_linf_s_inv,"projected divergence exceeds tolerance; step not accepted");
    check(report.mass_balance_relative<=c.projection.mass_balance_relative,"mass balance exceeds tolerance; step not accepted");
    f=std::move(candidate);return report;
}
StepReport advance(const Contract& c,Fields& f){return advance_impl(c,f,nullptr);}
StepReport advance_with_source(const Contract& c,Fields& f,const MomentumSource& source){return advance_impl(c,f,&source);}
}
