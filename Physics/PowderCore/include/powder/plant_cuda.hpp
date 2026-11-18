#pragma once
#include "granular.hpp"
namespace powder {
// Native CUDA implementation of the current plant position-contact model.
// This port is an equivalence baseline; it is not a calibrated force-based DEM.
class PlantCuda {
public:
 PlantCuda(GranularConfig,const std::vector<Grain>&,const std::vector<Grain>&);
 ~PlantCuda();
 void advance(unsigned,float,bool);
 void set(const std::vector<Grain>&,const std::vector<Grain>&);
 void update(const std::vector<Grain>&);
 std::vector<Grain> snapshot(bool velocity=false);
 std::vector<GranularBoundarySample> inspect(const std::vector<Grain>&,float,bool,bool);
 GranularTiming timing()const;
 double time()const;float angle()const;std::string device()const;
 std::array<std::uint64_t,3> memory()const;
 std::array<std::uint64_t,4> stats();
private:struct Impl;std::unique_ptr<Impl> p;
};
}
