#pragma once
#include "granular.hpp"
#include "coupled.hpp"
namespace powder {
// Experimental rectangular/voxel gas domain; caller must supply compatible physical boundaries.
// Uses configured approximate contact attraction; no non-spherical particles or full Hertz-DMT.
class GranularGasCoupler {
public:
 GasGrid gas;
 GranularGasCoupler(const GranularConfig&,const std::vector<Grain>& initial,GasGrid,Vec3 origin);
 void advance(GranularGPU&,unsigned steps,float rpm,bool gate);
 double gas_mass_error()const;double volume_error()const{return volume_error_;}
 double max_gas_speed()const{return max_speed_;}double elapsed_s()const{return elapsed_;}
private:
 GranularConfig config_;GranularGasField field_;double initial_mass_=0,volume_=0,volume_error_=0,max_speed_=0,elapsed_=0;bool failed_=false;
 void step(GranularGPU&,float rpm,bool gate);
};
}
