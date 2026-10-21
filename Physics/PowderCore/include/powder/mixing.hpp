#pragma once
#include "powder/clump.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace powder {
// A body (including a multi-pebble clump) contributes its mass exactly once,
// at its centre of mass. This is a grid-dependent uniformity, NOT Lacey's index.
struct MixingConfig {
    int count_a=16, count_b=16;
    double density_a=2500, density_b=2500;
    bool premixed=false;
    Vec3 lower{-.3,-.3,0}, upper{.3,.3,.3};
    std::array<int,3> grid{{2,2,1}};
    int minimum_bodies=4;
    double dt=2e-6, duration_s=.5, sample_s=.05, rpm=30;
    std::uint64_t steps(double seconds) const {
        const double n=seconds/dt;
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(n)||n<1||n>5000000||std::abs(n-std::round(n))>1e-6)
            throw std::invalid_argument("mixing time must be an integral number of fixed steps (1..5000000)");
        return static_cast<std::uint64_t>(std::llround(n));
    }
    void validate() const {
        if(count_a<1||count_b<1||count_a>128||count_b>128||!std::isfinite(density_a)||density_a<=0||!std::isfinite(density_b)||density_b<=0||!std::isfinite(rpm)||minimum_bodies<1)
            throw std::invalid_argument("invalid mixing charge/material/sampling settings");
        const double lo[]={lower.x,lower.y,lower.z},hi[]={upper.x,upper.y,upper.z};
        int cells=1;
        for(int i=0;i<3;++i){if(grid[i]<1||grid[i]>8||!std::isfinite(lo[i])||!std::isfinite(hi[i])||hi[i]<=lo[i])throw std::invalid_argument("invalid mixing region/grid (1..8 per axis)");cells*=grid[i];}
        const auto end=steps(duration_s), interval=steps(sample_s);
        if(interval>end||(end/interval+3)*static_cast<std::uint64_t>(cells)>1000000)throw std::invalid_argument("mixing sample storage limit exceeded or interval exceeds duration");
    }
};
inline std::vector<ClumpBody> mixing_charge(const MixingConfig& c) {
    c.validate();std::vector<ClumpBody> result;int used[2]={0,0};const int quota[2]={c.count_a,c.count_b};
    for(int layer=0;layer<4;++layer)for(int row=0;row<4;++row)for(int col=0;col<4;++col)for(int quadrant=0;quadrant<4;++quadrant){
        const int xsign=(quadrant%2)?1:-1, ysign=(quadrant/2)?1:-1;
        int group=c.premixed?((layer*16+row*4+col)%2):(xsign>0?1:0);
        if(c.premixed&&used[group]>=quota[group])group=1-group;
        if(used[group]>=quota[group])continue;
        auto b=make_clump(static_cast<ParticleId>(result.size()+1),static_cast<std::uint32_t>(group),{{{0,0,0},.04}},group?c.density_b:c.density_a);
        b.position_m={xsign*(col+.5)*.055,ysign*(row+1)*.055,.022+layer*.05};
        result.push_back(b);++used[group];
    }
    if(used[0]!=quota[0]||used[1]!=quota[1])throw std::logic_error("mixing charge capacity");
    return result;
}
struct MixingCell {std::array<int,2> count{{0,0}};std::array<double,2> mass{{0,0}};};
struct MixingSample {
    std::uint64_t step=0;double time_s=0,index=0;bool valid=false;
    std::string status;std::vector<MixingCell> cells;
    int occupied=0,undersampled=0,outside=0;
    std::array<int,2> count{{0,0}};std::array<double,2> mass{{0,0}},outside_mass{{0,0}};
};
inline MixingSample measure_mixing(const MixingConfig& c,const std::vector<ClumpBody>& bodies,const std::map<ParticleId,unsigned>& groups,std::uint64_t step) {
    c.validate();MixingSample s;s.step=step;s.time_s=static_cast<double>(step)*c.dt;s.cells.resize(static_cast<std::size_t>(c.grid[0]*c.grid[1]*c.grid[2]));
    if(groups.size()!=bodies.size())throw std::invalid_argument("mixing body/group count mismatch");
    std::map<ParticleId,const ClumpBody*> ordered;
    for(const auto& b:bodies)if(!ordered.emplace(b.id,&b).second)throw std::invalid_argument("duplicate mixing body ID");
    for(const auto& entry:ordered){const auto& b=*entry.second;auto it=groups.find(b.id);if(it==groups.end()||it->second>1)throw std::invalid_argument("missing or unknown mixing group");const auto g=it->second;const double m=clump_properties(b.pebbles,b.density_kg_m3).mass_kg;if(!std::isfinite(m)||m<=0)throw std::invalid_argument("invalid mixing mass");++s.count[g];s.mass[g]+=m;
        const double p[]={b.position_m.x,b.position_m.y,b.position_m.z},lo[]={c.lower.x,c.lower.y,c.lower.z},hi[]={c.upper.x,c.upper.y,c.upper.z};int ix[3]={0,0,0};bool outside=false;
        for(int a=0;a<3;++a){if(!std::isfinite(p[a]))throw std::invalid_argument("nonfinite mixing position");if(p[a]<lo[a]||p[a]>hi[a])outside=true;else ix[a]=std::min(c.grid[a]-1,static_cast<int>((p[a]-lo[a])/(hi[a]-lo[a])*c.grid[a]));}
        if(outside){++s.outside;s.outside_mass[g]+=m;continue;}
        auto& cell=s.cells[static_cast<std::size_t>(ix[0]+c.grid[0]*(ix[1]+c.grid[1]*ix[2]))];++cell.count[g];cell.mass[g]+=m;
    }
    const double total=s.mass[0]+s.mass[1];double variance=0;const double fraction=total>0?s.mass[0]/total:0;
    for(const auto& cell:s.cells){const double m=cell.mass[0]+cell.mass[1];if(m<=0)continue;++s.occupied;if(cell.count[0]+cell.count[1]<c.minimum_bodies)++s.undersampled;const double delta=cell.mass[0]/m-fraction;variance+=m/total*delta*delta;}
    s.status=s.outside?"outside_region":s.mass[0]<=0||s.mass[1]<=0?"missing_group":s.occupied<2?"insufficient_occupied_cells":s.undersampled?"undersampled_cells":"valid";
    s.valid=s.status=="valid";if(s.valid)s.index=std::clamp(1-variance/(fraction*(1-fraction)),0.0,1.0);return s;
}
class MixingMonitor {
public:
    MixingConfig config;std::map<ParticleId,unsigned> groups;std::vector<MixingSample> samples;
    explicit MixingMonitor(const MixingConfig& c,const std::vector<ClumpBody>& bodies):config(c){config.validate();for(const auto& b:bodies){if(b.type>1||!groups.emplace(b.id,b.type).second)throw std::invalid_argument("invalid initial mixing groups");}record(bodies,0);if(samples[0].count[0]!=c.count_a||samples[0].count[1]!=c.count_b)throw std::invalid_argument("charge does not match requested counts");}
    bool finished(std::uint64_t step)const{return step>=config.steps(config.duration_s);}
    MixingSample current(const std::vector<ClumpBody>& bodies,std::uint64_t step)const {
        auto s=measure_mixing(config,bodies,groups,step);
        if(!samples.empty())for(int g=0;g<2;++g)if(s.count[g]!=samples[0].count[g]||std::abs(s.mass[g]-samples[0].mass[g])>1e-12*std::max(1.0,samples[0].mass[g]))throw std::runtime_error("mixing group mass/count conservation failed");
        return s;
    }
    void record(const std::vector<ClumpBody>& bodies,std::uint64_t step,bool force=false){if(!samples.empty()&&step<=samples.back().step)return;if(force||step==0||finished(step)||step%config.steps(config.sample_s)==0)samples.push_back(current(bodies,step));}
    std::map<std::string,std::string> files(const std::vector<ClumpBody>& bodies,std::uint64_t step)const {
        auto copy=*this;copy.record(bodies,step,true);std::ostringstream meta,series,cells,labels;meta<<std::setprecision(17);series<<std::setprecision(17);cells<<std::setprecision(17);
        meta<<"{\"schema\":1,\"metric\":\"mass_weighted_cell_uniformity_v1\",\"assignment\":\"body_COM\",\"formula\":\"1-sum((m_i/M)*(c_i-c)^2)/(c*(1-c))\",\"grid\":["<<config.grid[0]<<','<<config.grid[1]<<','<<config.grid[2]<<"],\"lower_m\":["<<config.lower.x<<','<<config.lower.y<<','<<config.lower.z<<"],\"upper_m\":["<<config.upper.x<<','<<config.upper.y<<','<<config.upper.z<<"],\"minimum_bodies_per_occupied_cell\":"<<config.minimum_bodies<<",\"count_a\":"<<config.count_a<<",\"count_b\":"<<config.count_b<<",\"density_a_kg_m3\":"<<config.density_a<<",\"density_b_kg_m3\":"<<config.density_b<<",\"diameter_m\":0.04,\"premixed\":"<<(config.premixed?"true":"false")<<",\"rpm\":"<<config.rpm<<",\"dt_s\":"<<config.dt<<",\"duration_s\":"<<config.duration_s<<",\"sample_s\":"<<config.sample_s<<",\"duration_steps\":"<<config.steps(config.duration_s)<<",\"sample_steps\":"<<config.steps(config.sample_s)<<",\"run_finished\":"<<(finished(step)?"true":"false")<<"}\n";
        series<<"step,time_s,valid,status,uniformity,mass_a_kg,mass_b_kg,count_a,count_b,occupied,undersampled,outside,outside_mass_a_kg,outside_mass_b_kg\n";
        cells<<"step,time_s,ix,iy,iz,count_a,count_b,mass_a_kg,mass_b_kg,fraction_a\n";
        for(const auto& s:copy.samples){series<<s.step<<','<<s.time_s<<','<<s.valid<<','<<s.status<<',';if(s.valid)series<<s.index;series<<','<<s.mass[0]<<','<<s.mass[1]<<','<<s.count[0]<<','<<s.count[1]<<','<<s.occupied<<','<<s.undersampled<<','<<s.outside<<','<<s.outside_mass[0]<<','<<s.outside_mass[1]<<'\n';
            for(std::size_t i=0;i<s.cells.size();++i){const auto& cell=s.cells[i];cells<<s.step<<','<<s.time_s<<','<<i%config.grid[0]<<','<<(i/config.grid[0])%config.grid[1]<<','<<i/(config.grid[0]*config.grid[1])<<','<<cell.count[0]<<','<<cell.count[1]<<','<<cell.mass[0]<<','<<cell.mass[1]<<',';if(cell.mass[0]+cell.mass[1]>0)cells<<cell.mass[0]/(cell.mass[0]+cell.mass[1]);cells<<'\n';}}
        labels<<"body_id,group\n";for(const auto& e:groups)labels<<e.first<<','<<e.second<<'\n';
        return {{"mixing_metadata.json",meta.str()},{"mixing_timeseries.csv",series.str()},{"mixing_cells.csv",cells.str()},{"mixing_groups.csv",labels.str()}};
    }
};
}
