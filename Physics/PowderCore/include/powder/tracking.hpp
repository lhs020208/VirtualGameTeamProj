#pragma once
#include "powder/geometry.hpp"
#include <array>
#include <map>
namespace powder {
enum class Location : unsigned { Feed, Tube, Collector, Outside };
// Interior is dot(normal, position) <= offset. SI, unit outward normals.
struct Boundary { Vec3 normal; double offset=0; };
struct Volume { std::vector<Boundary> sides; bool contains(Vec3 p) const; };
struct TrackingLayout {
 bool enabled=false;
 Vec3 axis{0,0,1};
 double bottom=0,top=0;
 Volume bore,feed,funnel,collector;
};
// Current straight convex rig: tube local Z axis, local X wall thickness.
// Supports common rigid transforms and edited dimensions; rejects non-coaxial tube walls.
TrackingLayout tracking_layout(const std::vector<FixedBox>& feed,const std::vector<FixedBox>& funnel,
 const std::vector<FixedBox>& tube,const std::vector<FixedBox>& collector);
enum class Crossing : unsigned { EntryDown, ExitDown, ExitUp, EntryUp };
struct PassageEvent { Crossing kind; double time_s=0; bool completed_passage=false; };
struct ParticleTrack {
 ParticleId id=0; Location location=Location::Outside;
 double mass_kg=0,location_since_s=0,birth_time_s=0;
 bool admitted=false,ever_entered=false,ever_passed=false,ever_returned=false;
 double first_entry_s=-1,last_entry_s=-1,first_exit_s=-1,last_exit_s=-1;
 std::vector<PassageEvent> events;
 Vec3 previous;
};
struct TrackingSummary {
 bool enabled=false;
 std::array<std::size_t,4> counts{};
 std::array<double,4> mass_kg{};
 std::array<std::uint64_t,4> crossings{};
 std::size_t total_count=0,unique_entered=0,unique_passed=0,unique_returned=0;
 std::uint64_t passages=0,returns=0;
 double total_mass_kg=0,mass_error_kg=0,unique_passed_mass_kg=0;
 std::vector<ParticleTrack> records;
};
class Tracker {
public:
 void reset(TrackingLayout layout,const std::vector<Particle>& particles);
 // Every accepted solver step, never just rendered snapshots. Linear path within one step.
 void admit(const std::vector<Particle>& particles,double time_s);
 void advance(const std::vector<Particle>& particles,double time_s);
 const TrackingSummary& summary()const{return data_;}
private:
 Location classify(Vec3 p)const;
 void totals(TrackingSummary& data)const;
 TrackingLayout layout_;
 TrackingSummary data_;
 std::map<ParticleId,std::size_t> index_;
 double time_=0,initial_mass_=0;
};
}
