#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
namespace powder {
struct GranularTiming {std::array<double,5> gpu_seconds{};double advance_wall=0,snapshot_wall=0;};
struct Grain {float x,y,z,w;};
struct GranularConfig {unsigned count=30000,iterations=16;bool fast_bounds=true,profile=false,test_mixer=false;float radius=0,dt=.00025f,friction=.4f;};
void validate_granular_initial(GranularConfig config,const std::vector<Grain>& positions);
// Explicit approximate position-contact solver; persistent IDs = vector indices.
// SI units, equal-mass spheres, no spin/adhesion or contact-force certification.
class GranularGPU {
public:
 explicit GranularGPU(GranularConfig config);
 GranularGPU(GranularConfig config,const std::vector<Grain>& positions,const std::vector<Grain>& velocities);~GranularGPU();
 GranularGPU(const GranularGPU&)=delete;GranularGPU& operator=(const GranularGPU&)=delete;
 void advance(unsigned steps,float rpm,bool gate_open);
 std::vector<Grain> snapshot();
 void set_state(const std::vector<Grain>& positions,const std::vector<Grain>& velocities);
 GranularTiming timing()const;double time()const;float radius()const;float angle()const;std::string device()const;
private:struct Impl;std::unique_ptr<Impl> p;
};
}
