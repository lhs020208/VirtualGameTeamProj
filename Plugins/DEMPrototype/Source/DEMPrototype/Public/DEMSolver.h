#pragma once
// Standalone C++17, SI metres/kg/seconds, right-handed axes and cross products.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
namespace dem
{
struct V
{
    double x = 0, y = 0, z = 0;
    V operator+(V b) const
    {
        return {x + b.x, y + b.y, z + b.z};
    }
    V operator-(V b) const
    {
        return {x - b.x, y - b.y, z - b.z};
    }
    V operator-() const
    {
        return {-x, -y, -z};
    }
    V operator*(double s) const
    {
        return {x * s, y * s, z * s};
    }
    V operator/(double s) const
    {
        return *this * (1 / s);
    }
    V &operator+=(V b)
    {
        *this = *this + b;
        return *this;
    }
    V &operator-=(V b)
    {
        *this = *this - b;
        return *this;
    }
    double dot(V b) const
    {
        return x * b.x + y * b.y + z * b.z;
    }
    V cross(V b) const
    {
        return {y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x};
    }
    double norm2() const
    {
        return dot(*this);
    }
    double norm() const
    {
        return std::sqrt(norm2());
    }
    bool finite() const
    {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};
struct Q
{
    double x = 0, y = 0, z = 0, w = 1;
    void advance(V o, double dt)
    {
        Q q = *this;
        x += .5 * dt * (o.x * q.w + o.y * q.z - o.z * q.y);
        y += .5 * dt * (-o.x * q.z + o.y * q.w + o.z * q.x);
        z += .5 * dt * (o.x * q.y - o.y * q.x + o.z * q.w);
        w += .5 * dt * (-o.x * q.x - o.y * q.y - o.z * q.z);
        double n = std::sqrt(x * x + y * y + z * z + w * w);
        x /= n;
        y /= n;
        z /= n;
        w /= n;
    }
    bool finite() const
    {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }
};
struct Particle
{
    uint32_t id = 0;
    V p, v, w, f, t;
    Q q;
    double r = .01, m = 0, I = 0, invM = 0, invI = 0;
};
struct Config
{
    int count = 200;
    uint32_t seed = 20261021;
    double r = .01, rho = 1000, kn = 1000, kt = 500, zn = .2, zt = .2, mu = .4, muWall = .4, dt = .0001;
    V gravity{0, 0, -9.81};
    double width = .30, depth = .30, height = .25;
    bool walls = true;
};
// Fixed, zero-thickness, two-sided rectangle. u/v are orthonormal SI world axes.
struct Plane
{
    uint32_t id = 0;
    V center, u{1,0,0}, v{0,1,0};
    double halfU = .15, halfV = .15;
    bool measureSupport = false;
    V normal() const { return u.cross(v); }
    V closest(V point) const
    {
        V d = point-center;
        return center + u*std::clamp(d.dot(u),-halfU,halfU) + v*std::clamp(d.dot(v),-halfV,halfV);
    }
};
struct Load { V force, torque; };
struct Stats
{
    double time = 0, mass = 0, ke = 0, rotKE = 0, maxSpeed = 0, maxOverlap = 0, support = 0, height = 0,
           maxCoulombRatio = 0;
    uint64_t steps = 0, candidates = 0, contacts = 0, errors = 0, escapes = 0, floorCrossings = 0,
             growths = 0;
    size_t historyPeak = 0, candidatePeak = 0, historyCapacity = 0, candidateCapacity = 0;
    double gridMs = 0, searchMs = 0, contactMs = 0, integrateMs = 0, statsMs = 0, stepMs = 0;
};
struct Cell
{
    int64_t x, y, z;
    bool operator<(const Cell &b) const
    {
        return x != b.x ? x < b.x : y != b.y ? y < b.y : z < b.z;
    }
    bool operator==(const Cell &b) const
    {
        return x == b.x && y == b.y && z == b.z;
    }
};
struct Entry
{
    Cell c;
    uint32_t i;
};
struct Bucket
{
    Cell c{};
    int head = -1;
    uint64_t generation = 0;
};
struct Pair
{
    uint32_t a, b;
};
struct History
{
    uint64_t key;
    V xi, n;
};
class Solver
{
  public:
    Config c;
    std::vector<Particle> p;
    Stats s;
    bool failed = false;
    const char *error = "";
    std::vector<Entry> grid;
    std::vector<Pair> pairs;
    std::vector<History> history, nextHistory;
    double cellSize = 0;
    bool referenceGrid = false;
    bool externalMode = false;
    std::vector<Plane> planes;
    std::vector<Load> loads; // Persistent per global ID until replaced/reset; no step allocations.
    uint64_t boundaryCrossings = 0;
    std::vector<Bucket> buckets;
    std::vector<int> links;
    uint64_t generation = 0;
    using Clock = std::chrono::steady_clock;
    static double ms(Clock::time_point a, Clock::time_point b)
    {
        return std::chrono::duration<double, std::milli>(b - a).count();
    }
    void fail(const char *why)
    {
        if (!failed)
        {
            failed = true;
            error = why;
            ++s.errors;
        }
    }
    bool validate(bool external = false)
    {
        const double values[] = {c.r,  c.rho,    c.kn, c.kt,    c.zn,    c.zt,
                                 c.mu, c.muWall, c.dt, c.width, c.depth, c.height};
        for (double v : values)
            if (!std::isfinite(v) || v < 0)
            {
                fail("invalid nonfinite/negative configuration");
                return false;
            }
        if (c.count < 1 || c.count > 100000 || c.r <= 0 || c.rho <= 0 || c.kn <= 0 || c.kt <= 0 ||
            c.dt <= 0 || (!external && (c.width < 2.3 * c.r || c.depth < 2.3 * c.r || c.height <= 0)) || !c.gravity.finite())
        {
            fail("invalid configuration dimensions/mass/stiffness/dt");
            return false;
        }
        return true;
    }
    void setMass(Particle &a)
    {
        a.m = c.rho * (4.0 / 3.0) * 3.14159265358979323846 * a.r * a.r * a.r;
        a.I = .4 * a.m * a.r * a.r;
        a.invM = 1 / a.m;
        a.invI = 1 / a.I;
    }
    void reset(Config cfg)
    {
        externalMode = false;
        planes.clear(); loads.clear(); boundaryCrossings = 0;
        c = cfg;
        s = {};
        failed = false;
        error = "";
        p.clear();
        grid.clear();
        pairs.clear();
        history.clear();
        nextHistory.clear();
        if (!validate())
            return;
        size_t slots = 1;
        while (slots < size_t(c.count) * 4)
            slots *= 2;
        buckets.assign(slots, {});
        links.resize(c.count);
        generation = 0;
        p.reserve(c.count);
        grid.reserve(c.count);
        pairs.reserve(size_t(c.count) * 64);
        history.reserve(size_t(c.count) * 16);
        nextHistory.reserve(size_t(c.count) * 16);
        cellSize = 2 * c.r;
        uint32_t rng = c.seed;
        auto random = [&]()
        {
            rng = 1664525u * rng + 1013904223u;
            return double(rng) / 4294967296.0 - .5;
        };
        int nx = int(c.width / (2.3 * c.r)), ny = int(c.depth / (2.3 * c.r));
        for (int i = 0; i < c.count; ++i)
        {
            Particle a;
            a.id = uint32_t(i);
            a.r = c.r;
            setMass(a);
            a.p = {((i % nx) - .5 * (nx - 1)) * 2.3 * c.r + random() * .08 * c.r,
                   (((i / nx) % ny) - .5 * (ny - 1)) * 2.3 * c.r + random() * .08 * c.r,
                   c.height + 3 * c.r + (i / (nx * ny)) * 2.3 * c.r + random() * .08 * c.r};
            p.push_back(a);
        }
        statistics();
    }
    // External data path: no internal packing/container assumptions. IDs must be dense 0..N-1.
    bool resetExternal(Config cfg, const std::vector<Particle>& initial, const std::vector<Plane>& faces)
    {
        *this = Solver{};
        externalMode = true;
        c = cfg; c.count = int(initial.size()); c.walls = false;
        if (!validate(true) || initial.size()>10000 || faces.size()>256) { fail("external count/face limit"); return false; }
        p = initial; planes = faces;
        for (size_t i=0;i<p.size();++i)
        {
            const auto& a=p[i];
            const double q2=a.q.x*a.q.x+a.q.y*a.q.y+a.q.z*a.q.z+a.q.w*a.q.w;
            if (a.id!=i || !a.p.finite() || !a.v.finite() || !a.w.finite() || !a.q.finite() ||
                std::abs(q2-1)>1e-8 || !std::isnormal(a.r) || !std::isnormal(a.m) || !std::isnormal(a.I) ||
                !std::isnormal(a.invM) || !std::isnormal(a.invI) || a.r<=0 || a.m<=0 || a.I<=0 ||
                a.invM<=0 || a.invI<=0) { fail("invalid external particle/id/quaternion/mass"); return false; }
        }
        std::sort(planes.begin(),planes.end(),[](const Plane&a,const Plane&b){return a.id<b.id;});
        for(size_t i=0;i<planes.size();++i)
        {
            const auto& f=planes[i];
            if ((i && f.id==planes[i-1].id) || f.id>=0x80000000u || !f.center.finite() || !f.u.finite() || !f.v.finite() ||
                std::abs(f.u.norm2()-1)>1e-8 || std::abs(f.v.norm2()-1)>1e-8 || std::abs(f.u.dot(f.v))>1e-8 ||
                !std::isnormal(f.halfU) || !std::isnormal(f.halfV) || f.halfU<=0 || f.halfV<=0)
                {fail("invalid or duplicate finite boundary");return false;}
        }
        size_t slots=1; while(slots<p.size()*4) slots*=2;
        buckets.assign(slots,{}); links.resize(p.size()); loads.resize(p.size());
        grid.reserve(p.size()); pairs.reserve(p.size()*128);
        history.reserve(p.size()*(64+planes.size())); nextHistory.reserve(history.capacity());
        cellSize=2*c.r; statistics(); return !failed;
    }
    bool setExternalLoad(uint32_t id, V force, V torque)
    {
        if(!externalMode || id>=loads.size() || !force.finite() || !torque.finite())
        {fail("external load invalid ID or nonfinite value"); return false;}
        loads[id]={force,torque}; return true;
    }
    void clearExternalLoads() { for(auto& f:loads) f={}; }
    template <class T> void push(std::vector<T> &v, const T &item)
    {
        if (v.size() == v.capacity())
        {
            if (externalMode) { fail("preallocated contact/candidate capacity exceeded; Reset required"); return; }
            ++s.growths;
        }
        v.push_back(item);
    }
    Cell cell(V a) const
    {
        return {int64_t(std::floor(a.x / cellSize)), int64_t(std::floor(a.y / cellSize)),
                int64_t(std::floor(a.z / cellSize))};
    }
    static uint64_t key(uint32_t a, uint32_t b)
    {
        return (uint64_t(a) << 32) | b;
    }
    size_t bucketIndex(Cell c0) const
    {
        uint64_t h = uint64_t(c0.x) * 0x9e3779b185ebca87ULL;
        h ^= uint64_t(c0.y) * 0xc2b2ae3d27d4eb4fULL;
        h ^= uint64_t(c0.z) * 0x165667b19e3779f9ULL;
        h ^= h >> 32;
        return size_t(h) & (buckets.size() - 1);
    }
    void makeGrid()
    {
        grid.clear();
        cellSize = 0;
        for (const auto &a : p)
            cellSize = std::max(cellSize, 2 * a.r);
        if (referenceGrid)
        {
            for (uint32_t i = 0; i < p.size(); ++i)
                push(grid, Entry{cell(p[i].p), i});
            std::sort(grid.begin(), grid.end(),
                      [](const Entry &a, const Entry &b) { return a.c == b.c ? a.i < b.i : a.c < b.c; });
            return;
        }
        ++generation;
        if (generation == 0)
        {
            for (auto &b : buckets)
                b.generation = 0;
            ++generation;
        }
        for (int i = int(p.size()) - 1; i >= 0; --i)
        {
            Cell target = cell(p[i].p);
            size_t slot = bucketIndex(target);
            while (buckets[slot].generation == generation && !(buckets[slot].c == target))
                slot = (slot + 1) & (buckets.size() - 1);
            auto &b = buckets[slot];
            if (b.generation != generation)
            {
                b.c = target;
                b.head = -1;
                b.generation = generation;
            }
            links[i] = b.head;
            b.head = i;
        }
    }
    void findPairs()
    {
        pairs.clear();
        for (uint32_t i = 0; i < p.size(); ++i)
        {
            Cell base = cell(p[i].p);
            for (int x = -1; x <= 1; ++x)
                for (int y = -1; y <= 1; ++y)
                    for (int z = -1; z <= 1; ++z)
                    {
                        Cell target{base.x + x, base.y + y, base.z + z};
                        if (referenceGrid)
                        {
                            auto it = std::lower_bound(grid.begin(), grid.end(), target,
                                                       [](const Entry &e, const Cell &v) { return e.c < v; });
                            for (; it != grid.end() && it->c == target; ++it)
                                if (it->i > i)
                                    push(pairs, Pair{i, it->i});
                        }
                        else
                        {
                            size_t slot = bucketIndex(target);
                            while (buckets[slot].generation == generation && !(buckets[slot].c == target))
                                slot = (slot + 1) & (buckets.size() - 1);
                            if (buckets[slot].generation == generation)
                                for (int j = buckets[slot].head; j >= 0; j = links[j])
                                    if (uint32_t(j) > i)
                                        push(pairs, Pair{i, uint32_t(j)});
                        }
                    }
        }
        s.candidates = pairs.size();
        s.candidatePeak = std::max(s.candidatePeak, pairs.size());
    }
    // n points B -> A (wall -> particle). vrel = contact velocity A - B.
    // A gets Fn*n + Ft, B its negative. Arms are -rA*n and +rB*n.
    void contact(Particle &a, Particle *b, V n, double overlap, int wall, uint64_t boundaryKey = 0)
    {
        if (!(overlap>0) || failed) return;
        if (!std::isfinite(overlap) || !n.finite()) { fail("invalid contact geometry"); return; }
        const double effective = b ? 1 / (a.invM + b->invM) : a.m;
        const double cn = 2 * c.zn * std::sqrt(c.kn * effective), ct = 2 * c.zt * std::sqrt(c.kt * effective);
        V ra = n * (-a.r), rb = b ? n * b->r : V{};
        V rel = a.v + a.w.cross(ra) - (b ? b->v + b->w.cross(rb) : V{});
        double vn = rel.dot(n);
        V vt = rel - n * vn;
        double fn = std::max(0.0, c.kn * overlap - cn * vn), mu = b ? c.mu : c.muWall;
        uint64_t k = b ? key(a.id, b->id) : (uint64_t(1) << 63) | (uint64_t(a.id) << 3) | uint64_t(wall);
        if (boundaryKey) k = boundaryKey;
        V xi;
        auto it = std::lower_bound(history.begin(), history.end(), k,
                                   [](const History &h, uint64_t v) { return h.key < v; });
        if (it != history.end() && it->key == k)
        {
            V axis = it->n.cross(n);
            double cosTheta = std::clamp(it->n.dot(n), -1.0, 1.0);
            // Shortest rotation parallel transport. Antipodal normal is degenerate: discard history.
            if (cosTheta > -.999999)
            {
                xi = it->xi + axis.cross(it->xi) + axis.cross(axis.cross(it->xi)) / (1 + cosTheta);
                xi -= n * xi.dot(n);
            }
        }
        xi += vt * c.dt;
        V ft = xi * (-c.kt) - vt * ct;
        double ftLen = ft.norm(), limit = mu * fn;
        if (ftLen > limit)
        {
            ft = ft * (limit / ftLen);
            xi = -(ft + vt * ct) / c.kt;
        }
        push(nextHistory, History{k, xi, n});
        if (!std::isfinite(fn) || !ft.finite() || !xi.finite() || !std::isfinite(cn) || !std::isfinite(ct))
        { fail("nonfinite contact force/history/damping"); return; }
        V force = n * fn + ft;
        a.f += force;
        a.t += ra.cross(ft);
        if (b)
        {
            b->f -= force;
            b->t += rb.cross(-ft);
        }
        else if (wall == 0)
            s.support += fn;
        ++s.contacts;
        s.maxOverlap = std::max(s.maxOverlap, overlap / (2 * (b ? std::min(a.r, b->r) : a.r)));
        if (limit > 0)
            s.maxCoulombRatio = std::max(s.maxCoulombRatio, ft.norm() / limit);
    }
    void forces()
    {
        s.contacts = 0;
        s.maxOverlap = 0;
        s.support = 0;
        s.maxCoulombRatio = 0;
        nextHistory.clear();
        for (auto &a : p)
        {
            a.f = c.gravity * a.m;
            a.t = {};
            if(externalMode) { a.f += loads[a.id].force; a.t += loads[a.id].torque; }
        }
        for (auto pair : pairs)
        {
            auto &a = p[pair.a];
            auto &b = p[pair.b];
            V d = a.p - b.p;
            double d2 = d.norm2(), r = a.r + b.r;
            if (d2 >= r * r)
                continue;
            double distance = std::sqrt(d2);
            V n = distance > 1e-14 ? d / distance : V{1, 0, 0};
            contact(a, &b, n, r - distance, -1);
        }
        if (c.walls)
            for (auto &a : p)
            {
                double hx = c.width * .5, hy = c.depth * .5;
                // Finite rectangular faces only; rounded top/vertical edges are deliberately absent.
                if (std::abs(a.p.x) <= hx && std::abs(a.p.y) <= hy && a.p.z < a.r)
                    contact(a, nullptr, {0, 0, 1}, a.r - a.p.z, 0);
                if (a.p.z >= 0 && a.p.z <= c.height)
                {
                    if (std::abs(a.p.y) <= hy)
                    {
                        if (a.p.x + hx < a.r)
                            contact(a, nullptr, {1, 0, 0}, a.r - a.p.x - hx, 1);
                        if (hx - a.p.x < a.r)
                            contact(a, nullptr, {-1, 0, 0}, a.r + a.p.x - hx, 2);
                    }
                    if (std::abs(a.p.x) <= hx)
                    {
                        if (a.p.y + hy < a.r)
                            contact(a, nullptr, {0, 1, 0}, a.r - a.p.y - hy, 3);
                        if (hy - a.p.y < a.r)
                            contact(a, nullptr, {0, -1, 0}, a.r + a.p.y - hy, 4);
                    }
                }
            }
        if (externalMode)
            for (auto& a:p) for (const auto& face:planes)
            {
                V d=a.p-face.closest(a.p); double distance=d.norm();
                if(distance>=a.r) continue;
                V normal=face.normal();
                if((a.p-face.center).dot(normal)<0) normal=-normal;
                if(distance>1e-14) normal=d/distance;
                V before=a.f;
                contact(a,nullptr,normal,a.r-distance,-2,
                    (uint64_t(1)<<63)|(uint64_t(a.id)<<32)|face.id);
                if(face.measureSupport) s.support += (a.f-before).z;
            }
        std::sort(nextHistory.begin(), nextHistory.end(),
                  [](const History &a, const History &b) { return a.key < b.key; });
        history.swap(nextHistory);
        s.historyPeak = std::max(s.historyPeak, history.size());
        s.historyCapacity = history.capacity() + nextHistory.capacity();
        s.candidateCapacity = pairs.capacity();
    }
    void statistics()
    {
        s.mass = s.ke = s.rotKE = s.maxSpeed = s.height = 0;
        s.escapes = s.floorCrossings = 0;
        for (const auto &a : p)
        {
            s.mass += a.m;
            s.ke += .5 * a.m * a.v.norm2();
            s.rotKE += .5 * a.I * a.w.norm2();
            s.maxSpeed = std::max(s.maxSpeed, a.v.norm());
            s.height = std::max(s.height, a.p.z + a.r);
            if (c.walls)
            {
                if (a.p.z < 0)
                    ++s.floorCrossings;
                if (a.p.z < c.height &&
                    (std::abs(a.p.x) > c.width * .5 + a.r || std::abs(a.p.y) > c.depth * .5 + a.r))
                    ++s.escapes;
            }
            if (!a.p.finite() || !a.v.finite() || !a.w.finite() || !a.f.finite() || !a.t.finite() ||
                !a.q.finite())
                fail("NaN/Inf state");
        }
        if(!std::isfinite(s.mass)||!std::isfinite(s.ke)||!std::isfinite(s.rotKE)||!std::isfinite(s.maxSpeed)||!std::isfinite(s.height))
            fail("nonfinite aggregate statistics");
    }
    bool step()
    {
        if (failed)
            return false;
        auto start = Clock::now();
        for (const auto &a : p)
            if (!a.p.finite() || !a.v.finite() || !a.w.finite() || !std::isfinite(a.m) || a.m <= 0 ||
                !std::isfinite(a.I) || a.I <= 0 || a.invM<=0 || a.invI<=0 || !a.q.finite() || !std::isfinite(a.r) || !std::isfinite(a.invM) || !std::isfinite(a.invI) || a.r <= 0 ||
                std::abs(a.p.x) > 1e8 || std::abs(a.p.y) > 1e8 || std::abs(a.p.z) > 1e8)
            {
                fail("invalid particle state/mass or domain range");
                return false;
            }
        double maxRadius=0; for(const auto& a:p) maxRadius=std::max(maxRadius,a.r);
        if(!(maxRadius>0) || !std::isfinite(maxRadius)) {fail("invalid spatial grid cell size");return false;}
        for(const auto& a:p)
            if(std::max({std::abs(a.p.x),std::abs(a.p.y),std::abs(a.p.z)})/(2*maxRadius)>1e15)
            {fail("spatial grid integer range");return false;}
        makeGrid();
        auto t1 = Clock::now();
        findPairs();
        auto t2 = Clock::now();
        forces();
        auto t3 = Clock::now();
        if(failed) return false;
        for (auto &a : p)
        {
            const V beforePosition=a.p;
            a.v += a.f * (c.dt * a.invM);
            a.w += a.t * (c.dt * a.invI);
            a.p += a.v * c.dt;
            a.q.advance(a.w, c.dt);
            if(externalMode) for(const auto& face:planes)
            {
                const V n=face.normal(); const double d0=(beforePosition-face.center).dot(n), d1=(a.p-face.center).dot(n);
                if(d0*d1<0)
                {
                    const V at=beforePosition+(a.p-beforePosition)*(d0/(d0-d1));
                    if((at-face.closest(at)).norm()<1e-10) ++boundaryCrossings;
                }
            }
        }
        auto t4 = Clock::now();
        ++s.steps;
        s.time = double(s.steps) * c.dt;
        statistics();
        auto t5 = Clock::now();
        s.gridMs = ms(start, t1);
        s.searchMs = ms(t1, t2);
        s.contactMs = ms(t2, t3);
        s.integrateMs = ms(t3, t4);
        s.statsMs = ms(t4, t5);
        s.stepMs = ms(start, t5);
        return !failed;
    }
    double conservativeScale() const
    {
        double mass = std::numeric_limits<double>::max();
        for (const auto &a : p)
            mass = std::min(mass, a.m);
        return std::sqrt(.5 * mass / std::max(c.kn, c.kt));
    }
    std::vector<uint64_t> overlaps(bool brute)
    {
        std::vector<uint64_t> out;
        if (!brute)
        {
            makeGrid();
            findPairs();
        }
        auto add = [&](uint32_t i, uint32_t j)
        {
            if ((p[i].p - p[j].p).norm2() < std::pow(p[i].r + p[j].r, 2))
                out.push_back(key(p[i].id, p[j].id));
        };
        if (brute)
        {
            for (uint32_t i = 0; i < p.size(); ++i)
                for (uint32_t j = i + 1; j < p.size(); ++j)
                    add(i, j);
        }
        else
            for (auto pair : pairs)
                add(pair.a, pair.b);
        std::sort(out.begin(), out.end());
        return out;
    }
};
} // namespace dem
