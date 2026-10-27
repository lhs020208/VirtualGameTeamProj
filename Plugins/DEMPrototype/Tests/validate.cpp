#include "DEMSolver.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <numeric>
using namespace dem;
int failures = 0;
std::ofstream checks, trace, bench;
void check(const char *name, bool ok, double value, double limit)
{
    checks << name << "," << (ok ? "PASS" : "FAIL") << "," << std::setprecision(16) << value << "," << limit
           << "\n";
    if (!ok)
        ++failures;
}
void run(Solver &s, double time)
{
    uint64_t n = uint64_t(std::llround(time / s.c.dt));
    for (uint64_t i = 0; i < n && !s.failed; ++i)
        s.step();
}
void basic()
{
    {
        auto collision = [](double dt)
        {
            Config c;
            c.count = 2;
            c.walls = false;
            c.gravity = {};
            c.mu = 0;
            c.dt = dt;
            Solver s;
            s.reset(c);
            s.p[0].p = {-.011, 0, 0};
            s.p[1].p = {.011, 0, 0};
            s.p[0].v = {1, 0, 0};
            s.p[1].v = {-1, 0, 0};
            run(s, .03);
            return s.p[0].v.x;
        };
        double ref = collision(1e-6), a = collision(1e-4), b = collision(5e-5);
        check("B_refinement", std::abs(b - ref) < std::abs(a - ref), std::abs(b - ref), std::abs(a - ref));
    }
    {
        Config c;
        c.count = 1;
        c.gravity = {};
        c.muWall = 10;
        Solver s;
        s.reset(c);
        auto &a = s.p[0];
        a.p = {0, 0, .009};
        s.history.push_back({uint64_t(1) << 63, {.00001, 0, 0}, {0, 0, 1}});
        s.contact(a, nullptr, {0, 1, 0}, .001, 0);
        check("history_transport_tangent", std::abs(s.nextHistory.back().xi.y) < 1e-15,
              s.nextHistory.back().xi.y, 1e-15);
        check("history_transport_norm", std::abs(s.nextHistory.back().xi.norm() - .00001) < 1e-15,
              s.nextHistory.back().xi.norm(), .00001);
        a.p = {0, 0, 1};
        s.step();
        check("history_expiry", s.history.empty(), double(s.history.size()), 0);
    }
    {
        Config c;
        c.count = 1;
        c.walls = false;
        c.gravity = {};
        Solver s;
        s.reset(c);
        s.p[0].w = {1, 2, 3};
        run(s, .1);
        const auto &q = s.p[0].q;
        check("quaternion_normalized", std::abs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1) < 1e-14,
              q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w, 1);
    }
    {
        Config c;
        Solver a, b;
        a.reset(c);
        b.reset(c);
        b.referenceGrid = true;
        run(a, .30);
        run(b, .30);
        double error = 0;
        for (size_t i = 0; i < a.p.size(); ++i)
            error = std::max(error, (a.p[i].p - b.p[i].p).norm());
        check("optimized_reference_exact", error == 0, error, 0);
    }
    {
        V omega{.2, .3, .4}, arm{.5, .6, .7};
        V v = omega.cross(arm);
        V ueOmega{-omega.x, omega.y, -omega.z}, ueArm{arm.x, -arm.y, arm.z};
        V ueV = ueOmega.cross(ueArm);
        check("rotation_reflection_cross", (ueV - V{v.x, -v.y, v.z}).norm() < 1e-15,
              (ueV - V{v.x, -v.y, v.z}).norm(), 1e-15);
    }

    for (double dt : {1e-4, 5e-5})
    {
        Config c;
        c.count = 1;
        c.walls = false;
        c.dt = dt;
        Solver s;
        s.reset(c);
        s.p[0].p = {0, 0, 1};
        run(s, .1);
        double ez = std::abs(s.p[0].p.z - (1 - .5 * 9.81 * .01)), ev = std::abs(s.p[0].v.z + .981);
        check(dt == 1e-4 ? "A_position_dt" : "A_position_half", std::abs(ez - .5 * 9.81 * .1 * dt) < 1e-12,
              ez, .5 * 9.81 * .1 * dt);
        check("A_velocity", ev < 1e-12, ev, 1e-12);
    }
    for (double dt : {1e-4, 5e-5})
    {
        Config c;
        c.count = 2;
        c.walls = false;
        c.gravity = {};
        c.mu = 0;
        c.dt = dt;
        Solver s;
        s.reset(c);
        s.p[0].p = {-.011, 0, 0};
        s.p[1].p = {.011, 0, 0};
        s.p[0].v = {1, 0, 0};
        s.p[1].v = {-1, 0, 0};
        double maxOverlap = 0, initialKE = s.p[0].m;
        for (int i = 0; i < int(.03 / dt); ++i)
        {
            s.step();
            maxOverlap = std::max(maxOverlap, s.s.maxOverlap);
        }
        V momentum = s.p[0].v * s.p[0].m + s.p[1].v * s.p[1].m;
        check("B_momentum", momentum.norm() < 1e-14, momentum.norm(), 1e-14);
        check("B_energy_dissipation", s.s.ke < initialKE, s.s.ke, initialKE);
        trace << "collision," << dt << "," << s.p[0].v.x << "," << s.p[1].v.x << "," << s.s.ke << ","
              << maxOverlap << ",0,0,0,0\n";
    }
    {
        Config c;
        c.count = 1;
        c.gravity = {};
        Solver s;
        s.reset(c);
        s.p[0].p = {0, 0, c.r - .0001};
        s.p[0].v = {.1, 0, 0};
        s.step();
        check("C_friction_sign", s.p[0].f.x < 0, s.p[0].f.x, 0);
        check("C_torque_sign", s.p[0].t.y > 0, s.p[0].t.y, 0);
        check("C_slip_reduction", s.p[0].v.x - c.r * s.p[0].w.y < .1, s.p[0].v.x - c.r * s.p[0].w.y, .1);
        check("C_Coulomb", s.s.maxCoulombRatio <= 1 + 1e-12, s.s.maxCoulombRatio, 1);
    }
    {
        Config c;
        c.count = 64;
        Solver s;
        s.reset(c);
        for (int i = 0; i < 64; ++i)
            s.p[i].p = {-.0400000001 + (i % 4) * .01999999999, -.0400000001 + ((i / 4) % 4) * .0199,
                        -.04 + (i / 16) * .018};
        auto a = s.overlaps(true), b = s.overlaps(false);
        check("D_grid_brute", a == b, double(a.size()), double(b.size()));
        check("D_no_duplicate", std::adjacent_find(b.begin(), b.end()) == b.end(), double(b.size()),
              double(a.size()));
    }
    {
        Config c;
        c.count = 2;
        Solver s;
        s.reset(c);
        s.p[1].p = s.p[0].p;
        s.step();
        check("coincident_centres_finite", !s.failed, s.s.errors, 0);
        s.p[0].m = 0;
        s.step();
        check("invalid_mass_stops", s.failed, s.s.errors, 1);
    }
    {
        Config c;
        Solver a, b;
        a.reset(c);
        b.reset(c);
        run(a, .005);
        a.reset(c);
        run(a, .005);
        run(b, .005);
        bool equal = true;
        for (size_t i = 0; i < a.p.size(); ++i)
            equal &= (a.p[i].p - b.p[i].p).norm2() == 0;
        check("reset_determinism", equal, equal, 1);
        check("reset_history", a.history.size() == b.history.size(), double(a.history.size()),
              double(b.history.size()));
    }
    {
        Config c;
        c.count = 1;
        Solver s;
        s.reset(c);
        s.p[0].p = {c.width * .5 + .001, 0, c.height + .02};
        s.step();
        check("finite_wall_above_top", s.s.contacts == 0, s.s.contacts, 0);
    }
}
struct Deposit
{
    double ke = 0, height = 0, support = 0, maxOverlap = 0;
};
Deposit deposit(Config c, const std::string &label, double seconds)
{
    Solver s;
    s.reset(c);
    Deposit d;
    double support = 0, ke = 0;
    int samples = 0;
    uint64_t escaped = 0, floor = 0;
    double mass = s.s.mass;
    auto start = Solver::Clock::now();
    uint64_t steps = uint64_t(std::llround(seconds / c.dt)), stride = uint64_t(std::llround(.01 / c.dt));
    for (uint64_t i = 0; i < steps && !s.failed; ++i)
    {
        s.step();
        d.maxOverlap = std::max(d.maxOverlap, s.s.maxOverlap);
        escaped = std::max(escaped, s.s.escapes);
        floor = std::max(floor, s.s.floorCrossings);
        if (s.s.time > seconds - 1)
        {
            support += s.s.support;
            ke += s.s.ke + s.s.rotKE;
            ++samples;
        }
        if (i % stride == 0)
            trace << label << "," << s.s.time << "," << s.s.ke << "," << s.s.rotKE << "," << s.s.height << ","
                  << s.s.maxOverlap << "," << s.s.support << "," << s.s.contacts << "," << s.history.size()
                  << "," << s.s.growths << "\n";
    }
    d.ke = ke / std::max(1, samples);
    d.height = s.s.height;
    d.support = support / std::max(1, samples);
    double weight = mass * 9.81;
    check((label + "_mass").c_str(), s.s.mass == mass && s.p.size() == size_t(c.count), s.s.mass, mass);
    check((label + "_finite").c_str(), !s.failed, s.s.errors, 0);
    check((label + "_floor").c_str(), floor == 0, double(floor), 0);
    check((label + "_escape").c_str(), escaped == 0, double(escaped), 0);
    check((label + "_overlap").c_str(), d.maxOverlap <= .01, d.maxOverlap, .01);
    check((label + "_settledKE").c_str(), d.ke < 1e-5, d.ke, 1e-5);
    // Sidewall friction may support weight: report the bottom/weight ratio rather than assert equality.
    check((label + "_support_vs_weight_10pct").c_str(), std::abs(d.support - weight) / weight < .1,
          std::abs(d.support - weight) / weight, .1);
    std::ofstream snapshot("C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/Results/" + label +
                           "_snapshot.csv");
    snapshot << std::setprecision(17);
    for (const auto &a : s.p)
        snapshot << a.p.x << "," << a.p.y << "," << a.p.z << "," << a.q.x << "," << a.q.y << "," << a.q.z
                 << "," << a.q.w << "\n";
    std::cout << label << " max_overlap=" << d.maxOverlap << " mean_ke=" << d.ke << " height=" << d.height
              << " bottom=" << d.support << " weight=" << weight
              << " seconds=" << Solver::ms(start, Solver::Clock::now()) / 1000 << std::endl;
    trace << label + "_summary" << "," << c.dt << "," << d.ke << "," << d.height << "," << d.support << ","
          << d.maxOverlap << "," << weight << "," << s.s.historyPeak << "," << s.s.historyCapacity << ","
          << s.s.growths << "\n";
    return d;
}
void benchmark(Config c, int n)
{
    c.count = n;
    double scale = std::sqrt(double(n) / 200);
    c.width *= scale;
    c.depth *= scale;
    Solver initial;
    initial.reset(c);
    // Dense non-overlapping multilayer packing just above the floor; same radii/material/dt.
    int nx = int(c.width / (2.05 * c.r)), ny = int(c.depth / (2.05 * c.r));
    for (int i = 0; i < n; ++i)
        initial.p[i].p = {((i % nx) - .5 * (nx - 1)) * 2.05 * c.r,
                          (((i / nx) % ny) - .5 * (ny - 1)) * 2.05 * c.r,
                          c.r + .0001 + (i / (nx * ny)) * 2.05 * c.r};
    run(initial, .10);
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        Solver s = initial;
        std::vector<double> times;
        times.reserve(1000);
        double grid = 0, search = 0, contact = 0, integ = 0, stats = 0, contacts = 0, candidates = 0;
        auto start = Solver::Clock::now();
        for (int i = 0; i < 1000; ++i)
        {
            s.step();
            times.push_back(s.s.stepMs);
            grid += s.s.gridMs;
            search += s.s.searchMs;
            contact += s.s.contactMs;
            integ += s.s.integrateMs;
            stats += s.s.statsMs;
            contacts += s.s.contacts;
            candidates += s.s.candidates;
        }
        double elapsed = Solver::ms(start, Solver::Clock::now());
        std::sort(times.begin(), times.end());
        bench << "solver," << n << "," << repeat << "," << c.width << "," << c.depth << "," << c.dt
              << ",1000," << elapsed / 1000 << "," << times[949] << "," << grid / 1000 << "," << search / 1000
              << "," << contact / 1000 << "," << integ / 1000 << "," << stats / 1000 << "," << contacts / 1000
              << "," << candidates / 1000 << "," << (1000 * c.dt) / (elapsed / 1000) << "," << s.s.growths
              << "," << s.s.historyPeak << "," << s.s.historyCapacity << "\n";
        check("benchmark_finite", !s.failed, s.s.errors, 0);
    }
    std::cout << "bench " << n << " completed" << std::endl;
}
int main(int argc, char **argv)
{
    std::string mode = argc > 1 ? argv[1] : "basic", dir = argc > 2 ? argv[2] : ".";
    checks.open(dir + "/" + mode + "_checks.csv");
    trace.open(dir + "/" + mode + "_trace.csv");
    bench.open(dir + "/" + mode + "_performance.csv");
    checks << "test,status,value,criterion\n";
    trace
        << std::setprecision(16)
        << "case,time_or_dt,ke_or_vA,rotKE_or_vB,height_or_energy,overlap,support,contacts,history,growths\n";
    bench << std::setprecision(12)
          << "mode,particles,repeat,width_m,depth_m,dt,steps,mean_ms,p95_ms,grid_ms,search_ms,contact_ms,"
             "integration_ms,stats_ms,contacts,candidates,sim_wall_ratio,growths,history_peak,history_"
             "capacity\n";
    Config c;
    if (mode == "basic")
        basic();
    else if (mode == "candidate")
        deposit(c, "candidate", 5);
    else
    {
        c.kn = 500000;
        c.kt = 250000;
        c.dt = .00001;
        if (mode == "deposit")
        {
            auto a = deposit(c, "dt", 5);
            c.dt *= .5;
            auto b = deposit(c, "half", 5);
            check("F_height_5pct", std::abs(a.height - b.height) / b.height < .05,
                  std::abs(a.height - b.height) / b.height, .05);
            check("F_support_5pct", std::abs(a.support - b.support) / b.support < .05,
                  std::abs(a.support - b.support) / b.support, .05);
        }
        else if (mode == "bench")
        {
            for (int n : {200, 1000, 5000})
                benchmark(c, n);
        }
    }
    std::ofstream json(dir + "/" + mode + "_summary.json");
    json << "{\"mode\":\"" << mode << "\",\"failures\":" << failures
         << ",\"compiler\":\"MSVC /O2 /fp:precise\",\"precision\":\"double\"}\n";
    return failures ? 1 : 0;
}
