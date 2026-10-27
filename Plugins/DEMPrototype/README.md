# DEMPrototype — independent CPU sphere DEM

This is a numerical/visual prototype, not an industrially validated solver and not CFD–DEM.
All new assets and code are under this plugin; the project's existing .uproject change and startup map are preserved.

## Run and build

Windows PowerShell, from the project root:

```powershell
$tool = '.\Plugins\DEMPrototype\Tools\Run.ps1'
& $tool BuildTests
& $tool Basic
& $tool Candidate   # deliberately records the original coefficient failure
& $tool Deposit     # 5 simulated seconds at dt and dt/2
& $tool Bench       # physics only, 200/1000/5000, three repeats
& $tool Build
& $tool Map         # first creation only; refuses to overwrite existing assets
& $tool Run
& $tool Run -Extra '-DEMAuto -DEMTag=trial200'
```

The plugin enables itself by default. Restart the editor after building to discover a newly added plugin. Open `/DEMPrototype/DEM_Sandbox` (enable Show Plugin Content in the Content Browser), then Play, or use the standalone launch above. No project default map is changed.

Space / Start-Pause toggles running, N / Step executes exactly one fixed dt, R / Reset reapplies the actor settings, S / Save writes buffered samples under `Saved/DEMPrototype`. HUD buttons also change speed and 10/60 Hz display frequency. Actor Details exposes count, seed, radius, density, stiffness, damping, friction, dt, display Hz, physics-only mode and high-resolution mesh. Configuration is copied to the solver only on Reset. Optional red direction lines show orientation for ten particles.

Scripts currently contain this machine's UE/MSVC/project paths. Edit those paths for another machine. No external package installation is required. UE 5.8 rejects installed MSVC 14.43; builds use already installed 14.51, which UBT accepts with a newer-than-preferred warning. The standalone final tests use the same 14.51 toolchain with `/O2 /fp:precise`.

## Architecture and allocations

`DEMSolver.h` is standalone C++17, with no Unreal dependency, rendering geometry or Chaos access. `DEMSimulationActor` handles fixed substeps and reset configuration. ISM transformation preparation/upload is separate from stepping. `ADEMHUD` and `ADEMGameMode` implement controls/camera. `DEMSandboxCommandlet` creates new independent assets. `Tests/validate.cpp` runs without an engine or renderer.

A contiguous vector of particle structs keeps per-particle integration data together; contacts touch two structs. Particle radii, mass, inertia and reciprocals are cached. Stable IDs are separate from render instance order (reset creates ascending immutable IDs, instance index = ID). No per-particle Actor, UObject, tick or physics body exists.

The spatial hash uses a power-of-two table at <=25% particle occupancy, linear probing, full integer cell equality (hash collisions do not imply spatial equality), generation markers and preallocated linked particle indices. Cell width is the maximum diameter; floor division handles negative positions. The 27 neighboring cells are visited in a fixed order and only index j>i is considered. Chains preserve ascending particle order. A retained sorted-grid implementation and brute force contact query serve as regression references. The hash changed search cost only, not contact or summation order.

Candidate arrays reserve 64 entries/particle; two contact-history vectors reserve 16 entries/particle each. Histories have sorted stable pair keys (walls use a disjoint high-bit namespace). Next-step history contains only active geometric contacts, then buffers swap. Binary lookup and sort are deliberate simple deterministic choices. All buffers retain capacity on reset. Capacity expansion counts and peak history/candidate counts are measured; contact growth may still cause rare allocations. Diagnostic strings and file writes are outside contact/substep loops, sampled and buffered. A numerical error stops immediately and writes the error at the host boundary.

## Units, axes, forces

Solver units: m, kg, s, N, rad/s, double precision. Radius r and density rho imply `m = rho * 4*pi*r^3/3`, `I = 2*m*r^2/5`.

The solver is right handed, +Z up. At the display boundary, positions map to Unreal `(100*x, -100*y, 100*z)`. Polar vectors reflect Y; axial angular velocity/torque map `(-wx, wy, -wz)`. Orientation maps to quaternion `(-qx, qy, -qz, qw)`. The reflection identity for angular cross products is tested. No centimetre values enter contact calculations. Instance scale is `100*r / source_mesh_radius`; LOD or mesh tessellation cannot change physical r.

For pair A,B, `n = (xA-xB)/distance` points B -> A. Arms are `ra=-rA*n`, `rb=rB*n`. Contact relative velocity:

```
vrel = vA + omegaA cross ra - vB - omegaB cross rb
vn = dot(vrel,n); vt = vrel - vn*n
meff = 1/(1/mA + 1/mB)       # a fixed wall uses mA
cn = 2*zn*sqrt(kn*meff); ct = 2*zt*sqrt(kt*meff)
Fn = max(0, kn*overlap - cn*vn)
xi = transported_previous_xi + vt*dt
Ft_trial = -kt*xi - ct*vt
|Ft| <= mu*Fn
xi = -(Ft + ct*vt)/kt        # after Coulomb clipping
FA += Fn*n + Ft; FB -= Fn*n + Ft
tauA += ra cross Ft; tauB += rb cross (-Ft)
```

History is parallel transported by the shortest rotation between contact normals, then reprojected. Antipodal degenerate normals discard history. Coincident centres choose a deterministic +X normal. The normal force clamp prevents attraction, but ends the dissipative impulse earlier than an unrestricted damped oscillator. Therefore damping ratio is not an exact restitution specification. This is a **linear spring–dashpot with tangential displacement history**, not Hertz–Mindlin. No rolling resistance, torsion resistance or ad hoc particle sleep is included.

Five analytical faces represent the floor and finite-height walls. Side faces act only when centre z is in [0,H] and the centre projects onto the face's horizontal span. Floor acts only within its rectangle. Top lips, rounded edges, and contacts whose sphere overlaps an edge while its centre projects outside the face are **not modeled**. Above H, there are no invisible side walls. Open-top excursions are allowed; side escape below H and centre crossing below z=0 are reported, never repaired/deleted. This simplification is unsuitable for edge-dominated flow.

## Integration and time

Semi-implicit Euler updates velocity/angular velocity, then position/quaternion. Quaternion is normalized after each step. A frame accumulator adds `frame_dt * requested_speed`; it consumes only executed fixed steps. The default 500-step/frame cap never enlarges dt. If backlog exceeds 2 s the actor pauses, preserving the backlog; no pending time is counted as simulated. Reset intentionally clears time. Step while paused does one dt independent of backlog. Simulation time is integer step count * dt; host elapsed time comes from a monotonic clock. Auto benchmarks deliberately run a fixed step count per frame and do not claim real-time scheduling.

A diagnostic scale `sqrt((min_mass/2)/max(kn,kt))` is reported by the solver, not a stability proof; frictional stiffness, rotational modes, contact networks and Euler error also matter. dt/2 tests are mandatory. The complete initial-condition fixture uses a fixed LCG seed and a 2.3r lattice with <=0.04r random offsets, avoiding overlaps without post-insertion correction.

## Settings

Container: 0.30 x 0.30 x 0.25 m, floor z=0, open top. 200 spheres r=.010 m, rho=1000 kg/m3, g=(0,0,-9.81), zero initial velocity/spin, seed=20261021. Each particle is about .00418879 kg; total .83775804 kg, weight 8.21840638 N.

Original requested kn=1000, kt=500 N/m, dt=.0001 s produced 20.26% peak overlap. Final test candidate uses kn=500000, kt=250000 N/m, dt=.00001 s, zn=zt=.2, both friction coefficients .4. These are assumed prototype values, not calibrated powder properties. Stiffness increased to meet the overlap target, not to claim more realistic material behaviour.

## Rendering

A generated radius-1 uu sphere has 122 render vertices/168 triangles. Comparison `/Engine/BasicShapes/Sphere` has 559/960, radius 50 uu. Opaque basic material, no per-particle dynamic shadows, collision or navigation. ISM is chosen because nearly every particle moves; no HISM tree rebuild overhead. Single batch upload uses the local UE 5.8 header's `BatchUpdateInstancesTransforms(..., false, true, true)` once per display interval, never once per substep. Instances are created only on Reset. Rendering frequency and mesh resolution do not enter solver state. Nanite is not used as a solution.

## Evidence and limitations

Actual CSV/JSON, mesh counts, machine metadata, logs and screenshots are in `Results`. `candidate_*` intentionally preserves the initial overlap failure. `unoptimized_partial_*` is an interrupted development run, not a completed validation. `basic_*` contains analytic/freefall, collision, torque/friction, neighbor, coordinate and reset regressions. `deposit_*` contains the complete dt sensitivity test. Profiling runs performed during builds are not clean absolute performance evidence; final benchmarking must be interpreted separately.

Bottom support can differ from total weight because sidewall friction carries vertical load; inspect settling energy and side conditions before asserting force balance. Without rolling resistance, residual rolling energy may persist. Grid checks and stability observations do not establish industrial validation.

## Benchmark fixtures

Standalone solver benchmarks use a 2.05r non-overlapping floor-adjacent lattice, the same r/material/dt, expanded X/Y by sqrt(N/200), and a .10 s physics warmup. They measure 1000 steps three times from copies of the same warm state. Grid, candidate search, forces/history, integration and statistics are separately timed with a monotonic clock; initialization/warmup/CSV output are outside the measurement. Capacity peaks include warmup.

The Unreal `Suite` task runs 200, 1000 and 5000 sequentially in physics-only, low sphere ISM and high sphere ISM modes, three separate process runs each. `-DEMDense` uses the same dense fixture and 10,000 warmup steps. It then runs 130 frames x 10 steps, discards the first 30 frame samples and reports the next 100. All modes execute the same steps and initial conditions. `steps` in these CSVs denotes post-warmup steps (1300); `sim_time` includes the .1 s warmup. Physics-only inside Unreal still draws the container and HUD; the standalone executable is the completely renderer-free case. Frame time includes engine work, presentation and waiting; it is not a pure GPU measurement. ISM prepare and upload CPU time are separately reported. Low/high modes use the same display frequency. Game/render-thread-specific and GPU timings are not instrumented yet.

`Tools/summarize.py` aggregates actual files and compares byte hashes of the exported numerical state. The result explicitly lists missing data. Frame mean/p95 values are averaged over repeats, not pooled percentiles. `-noraytracing` is used for final performance runs; the first exploratory screenshot run paid shader/path-tracing initialization costs and is not the final performance benchmark.

The default requested speed is 0.05x because the validated stiff contact model is not real time on this machine. It is adjustable; raising it beyond available CPU throughput grows the visible backlog and eventually pauses. dt remains fixed. The contact scale is about 64.7 microseconds, so dt/scale is about 0.155 (dt/2 gives 0.0773); this is a diagnostic only.

`-DEMSnapshot -DEMAuto -DEMTag=settled200` visualizes positions/orientations exported by the completed 5 s test. The HUD explicitly labels it a geometry snapshot: velocities, forces and contact histories are not replayed in that view. Use `deposit_metrics.json` for the actual physical measurements, rather than the snapshot HUD. Dense performance runs disable screenshots during measurement. They export all particle positions, velocities, angular velocities, quaternions, forces and torques, plus tangential history/normals, for invariance comparison.

## Validation tolerances and scope

- A: semi-implicit Euler freefall at .1 s; expected z error = .5*g*T*dt and velocity near roundoff. Halving dt halves position error (first order).
- B: equal spheres at +/-1 m/s, g=mu=0; linear momentum tolerance 1e-14 kg m/s, dissipative final energy below initial. dt/2 collision speed is compared with a 1 microsecond reference; it must be closer than dt. Clipped normal forces mean oscillator restitution formulas are not used as an exact oracle.
- C: positive x slip over +z floor must produce negative x force, positive y torque and reduced contact slip; Coulomb ratio <=1+1e-12. Separate history-transport/expiration and quaternion normalization tests are included.
- D: 64 particles spanning negative indices and near-exact cell boundaries; grid/brute contact-key sets equal and unique. Sorted-grid/hash trajectories are compared through an impact with identical summation order.
- E: 5 s, invariant count/mass, no numerical failure, no centre crossing below floor or side exit; peak overlap <=.01. Mean total kinetic energy over the last second <1e-5 J is the settling criterion. Bottom support is compared to weight within 10%; side friction can complicate this criterion.
- F: identical initialization at dt and dt/2; report kinetic energy, bed height, support and peak overlap. Height/support compare within 5%; individual final coordinates need not match. Residual kinetic energy is very small but its relative difference can still be significant.
- G: automated standalone control-handler calls check Step exactly once, Start, Pause, Reset, seed and empty histories. Separate engine launches verify plugin/map reload. Cross-mode CSV exports compare all state and history fields exactly. This does not constitute an automated mouse hit-test of HUD buttons or a packaged shipping-build test. Mesh resolution changes are tested; a multilevel distance LOD system is not implemented.

Long-run history inspection is bounded to the 5 s tests (500,000 and 1,000,000 steps). Reported peaks/capacities are stable and no growth occurred in those runs; this is not a multi-hour endurance test. All physics benchmarks remain single threaded. Profiling of the current implementation does not justify claiming parallel speedup or a real-time 5000-particle capacity.

The generated sphere uses Unreal front-face winding (clockwise viewed from outside) with outward vertex normals. A visual review caught reversed culling in the first development mesh; it was corrected without changing radius, vertex count, triangle count or solver data. `RebuildMesh` updates only this generated sphere after fully loading its package; it does not overwrite a map. `Map` still refuses to overwrite existing assets. Development render measurements from before this visual correction are archived in `Results/development_render_v1` and excluded from final aggregate results.

The plain C++ `dem::Config` defaults intentionally retain the original requested candidate coefficients for the Candidate/basic regression fixtures. The Unreal actor and Deposit/Bench test entry points explicitly select the validated final stiffness and dt. When embedding the standalone solver elsewhere, supply the settings in `Results/configuration.json`; do not assume a default-constructed Config meets the 1% drop-test overlap target.
