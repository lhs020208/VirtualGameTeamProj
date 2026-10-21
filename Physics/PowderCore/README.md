# PowderCore 0.14.0 — tasks 01 through 14

Standalone C++17 translational dynamics with gravity and constant external forces. Optional horizontal-plane normal contact; no Unreal dependencies. The default Unreal GameMode now uses the task 10 background DEM bridge with 300 particles; historical sections below describe their original scope.

## Build and verify

From a terminal with CMake and Visual Studio 2022 C++ tools:

```powershell
cmake -S Physics/PowderCore -B Intermediate/PowderCore -G "Visual Studio 17 2022" -A x64
cmake --build Intermediate/PowderCore --config Release
ctest --test-dir Intermediate/PowderCore -C Release --output-on-failure
python Physics/PowderCore/tests/cli_tests.py Intermediate/PowderCore/Release/powder_run.exe
Intermediate/PowderCore/Release/powder_run.exe --diameter 0.002 --density 2500 --dt 0.0001 --steps 10000 --seed 42 --output Saved/PowderCore_task01.json
```

If CMake is not in PATH, Visual Studio Community bundles it under
`C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`.
Generated files go into the project's ignored Intermediate/Saved directories.

## Contracts

- SI units: metres, kilograms, seconds, radians; explicit m/cm conversion at the future Unreal boundary.
- Double precision state; uint64 particle IDs must be unique in a collection.
- Homogeneous solid sphere: mass = density * pi/6 * diameter^3;
  isotropic centre inertia = 2/5 * mass * radius^2. No independently editable mass.
- Position, linear/angular velocity and unit quaternion are stored and validated.
- Material currently contains density only. Elastic/friction properties will be added with their contact models.
- FixedClock advances integer ticks without wall time; time = tick * dt. Zero steps is a valid empty run.
- Positive finite diameter, density and dt required. Derived underflow/overflow, nonfinite state, duplicate IDs and invalid orientation are rejected with diagnostic exceptions.
- Core tick range is <=2^53, plus finite/resolvable end-time checks. CLI is capped at 10 million steps to prevent accidental huge long loops.
- Seed is recorded but unused: there is no random initialization yet. Do not claim this validates a random generator.
- JSON records schema/core/model versions, SI units, seed, effective settings,
  completed steps, final time and initial and final particle state. Same inputs produce identical JSON on this build.
- JSON is output only in task 01; config import and configurable initialization belong to task 12.
- CLI validates before opening output; an explicitly supplied existing output path is overwritten after successful computation.

## Predeclared acceptance criteria

Relative tolerance 1e-12 for conversion and independent sphere reference values;
exact equality for IDs, tick counts and same-build deterministic records.
Reject zero/negative/NaN/infinite physical inputs. Validate scaling with diameter
(mass x8, inertia x32 when doubled) and density. Check invalid CLI syntax,
JSON parsing, output failure and existing-file preservation on validation failure.

## Task 02: translational dynamics

Integrator: symplectic (semi-implicit) Euler, v_next = v + (F/m)*dt,
x_next = x + v_next*dt. First-order position accuracy; constant acceleration
velocity is exact up to roundoff. Forces must be reapplied each step; successful
integration clears the accumulator. Failed integration preserves state and loads.
Gravity defaults to (0,0,-9.81) m/s2. No floor: particles can pass below Z=0.

CLI options: --px/py/pz initial position in m; --vx/vy/vz initial velocity in m/s;
--gx/gy/gz gravity in m/s2; --fx/fy/fz constant force in N. All initial/load values
are recorded in JSON schema 2 alongside the final computed state.
Rotation integration is not implemented; nonzero angular velocity is rejected.

```powershell
python Physics/PowderCore/tests/motion_tests.py Intermediate/PowderCore/Release/powder_run.exe
Intermediate/PowderCore/Release/powder_run.exe --pz 10 --vz 2 --dt 0.001 --steps 1000 --output Saved/PowderCore_task02.json
```

Acceptance specified before execution: absolute error <=1e-10 SI for constant
velocity, velocity under gravity, and predicted discrete position error. Halving
dt must halve constant-gravity position error, ratio tolerance 1e-8. Tests cover
3D loads, mass independence of gravity, external force plus gravity, empty runs,
invalid inputs and prior task regressions. These are verification tests, not
calibration against real powder. No contact, energy-conserving integrator claim,
or complete DEM solver is implied.
## Task 03: horizontal plane normal contact

Enable with --plane-z (metres), --stiffness (N/m, positive), --damping (Ns/m,
nonnegative). Without --plane-z, free motion remains unchanged. Friction and
rotation are absent; the plane is infinite, fixed, horizontal, with outward +Z.
It is a one-sided half-space, not a finite mesh or a tunnelling-proof CCD surface.

For overlap delta = planeZ + radius - centreZ > 0:
Fn = k*delta + c*max(-vz,0). Otherwise Fn=0.
The dashpot acts ONLY during compression. Unloading is elastic, ensuring no
adhesive normal force. This differs from the full-cycle spring-dashpot formula:
do not use exp(-pi*zeta/sqrt(1-zeta^2)) to interpret this model's restitution.
Stored elastic energy = k*delta^2/2; dissipated power = c*max(-vz,0)^2.

For gravity-free underdamped impact, omega=sqrt(k/m), alpha=c/(2m),
wd=sqrt(omega^2-alpha^2), t_peak=atan(wd/alpha)/wd;
e=exp(-alpha*t_peak), delta_max=v_impact*e/omega,
t_contact=t_peak+pi/(2*omega). For c=0: e=1 and t_contact=pi/omega.
These formulas describe this specific compression-only damping model.

Conservative enforced step cap: dt <= 0.02/(sqrt(k/m)+c/m).
This is an accuracy-oriented cap, not a universal guarantee of stability or
small relative overlap. Verify dt convergence and maximum overlap for every
new stiffness/mass/impact-speed regime. It is enforced even while airborne.
Contact failure leaves state and the supplied force accumulator unchanged.

```powershell
python Physics/PowderCore/tests/contact_cli_tests.py Intermediate/PowderCore/Release/powder_run.exe
Intermediate/PowderCore/Release/powder_run.exe --plane-z 0 --stiffness 100 --damping 0.001 --pz 0.01 --dt 0.000001 --steps 100000
```

Tests: elastic and damped restitution/contact time/peak overlap; bounded elastic
energy error, dissipation, dt refinement; exact static force equilibrium and
5-second drop-to-rest; invalid stiffness/damping/time-step input and atomic
failure. Numerical acceptance thresholds are specified in the test source.
Task 04 will add sphere-sphere contact. Unreal remains on the previous Chaos test.
## Task 04: sphere-sphere normal contact

API: sphere_contact, pair_step_limit_s, step_pair; model PairContact(k,c).
The normal points from A to B. Fn=k*overlap+c*max(-dot(vB-vA,n),0),
with compression-only damping as in task 03. The effective mass is
min(mA,mB)/(1+min(mA,mB)/max(mA,mB)), avoiding the product mA*mB overflow.
Forces on A and B are exact negatives. No tangential forces or torque yet.

step_pair computes one pair once using both OLD states and commits both new
states only after both integrations succeed. Loads clear only on success.
Rejects duplicate IDs, coincident centres (undefined normal), aliased bodies
or load accumulators, nonfinite values and dt above
0.02/(sqrt(k/m_eff)+c/m_eff). This cap is not a universal stability guarantee.

The pair step is for an isolated pair only. Do not loop step_pair over many
pairs: future multi-particle integration must accumulate ALL pair forces before
updating ANY particle. Pair uniqueness across a collection belongs to task 07.
The single-particle powder_run CLI remains unchanged in scope; pair_tests is the
standalone two-body experiment runner, registered in CTest as sphere_pair.

Acceptance before execution: momentum absolute error <=1e-10 kg*m/s, final
velocities <=3e-4 m/s and energy loss <=3e-4 J against analytic central-impact
results; damped restitution error must decrease on timestep refinement.
Tested equal and unequal masses/sizes, elastic and damped collisions, all-axis
force symmetry, swapped pair order, force applied once, separated bodies,
invalid identities/aliases/time steps and atomic failure.

Run: ctest --test-dir Intermediate/PowderCore -C Release -V
Next: task 05 rotational dynamics and tangential contact history.
## Task 05: rotation and friction (new API)

Include powder/friction.hpp. integrate_rigid accepts world-space torque and uses
solid-sphere isotropic inertia, semi-implicit angular velocity, and a normalized
quaternion exponential update. Legacy translation-only functions intentionally
keep rejecting spin. Existing CLI is still the normal-contact-only runner.

step_friction_plane supports unit-normal infinite fixed planes dot(n,x)=offset.
step_friction_pair handles one isolated pair. FrictionModel specifies kn,cn,kt,ct
in SI, and one Coulomb coefficient mu shared by sticking and sliding. The contact
spring records tangential displacement and returns Ft=-kt*xi-ct*slip, capped at
mu*Fn. After slip, xi is corrected to agree with the capped force. Separation
clears history; active history rejects another particle identity or pair order.
Use one history per plane contact and reset it when changing the plane geometry
or material. No multi-contact history map exists yet.

History is rotated by the shortest normal-to-normal rotation and half the sum
of body spin along the normal (the static plane contributes zero spin). Nearly
opposite normals are rejected instead of picking an arbitrary rotation axis.
Tangential damping and stored spring energy are distinct: a spring can return
energy, so instantaneous tangential force is not always opposite slip.

A common contact point (half overlap per radius for a pair) ensures consistent
lever arms and total torque balance. Plane arm reaches the actual plane. Deep
penetration past a sphere centre is rejected. Hence finite-overlap rolling
results differ slightly from ideal rigid-sphere reference values.

Conservative dt limit includes both normal reduced mass and tangential effective
mass including rotational inertia. Steps commit particles, loads and history
only after validation succeeds. Forces/torques are computed from old states.
No rolling-resistance torque, torsional friction, separate static/dynamic mu,
non-spherical inertia, CCD, multi-contact integration or Unreal rendering yet.

Validation runner: Intermediate/PowderCore/Release/friction_tests.exe
CTest name: rotation_friction. Tests include free spin/quaternion, torque/inertia,
sticking/sliding cap, history rotation/reset, horizontal slide-to-roll,
frictionless and frictional inclines, oblique pair linear/angular momentum,
energy reduction, and rejection without partial mutation.
Tolerances are declared in test source: free spin 1e-10, horizontal rolling speed
0.003 m/s, inclined rolling speed 0.004 m/s, angular momentum 1e-9 kg*m2/s.

Reference for tangential-history/Coulomb model concepts and rotating contact
frames: https://docs.lammps.org/pair_granular.html . No LAMMPS executable or code
is used; this implementation is our own restricted linear-history model.
## Task 06: material-based Hertz contact

Include powder/elastic.hpp. MaterialContact selects Linear (delegates unchanged
FrictionModel) or HertzMindlin. ElasticMaterial is separate from density material:
Young modulus E [Pa]>0, -1<Poisson ratio nu<0.5; both finite. Plane material b is
a finite-compliance material on a stationary wall, not an infinite E shortcut.

1/Eeff=(1-nua^2)/Ea+(1-nub^2)/Eb;
Gi=Ei/(2*(1+nui)); 1/Geff=(2-nua)/Ga+(2-nub)/Gb.
Reff=Ra*Rb/(Ra+Rb) for spheres, Ra for a plane.
F_el=(4/3)*Eeff*sqrt(Reff)*delta^(3/2);
dF_el/ddelta=2*Eeff*sqrt(Reff*delta);
U_normal=(2/5)*F_el*delta;
kt=8*Geff*sqrt(Reff*delta).

The tangential model uses the existing displacement history with this instantaneous
Mindlin no-slip stiffness and Coulomb cap. This is a simplified history law,
NOT full Mindlin-Deresiewicz partial slip/unloading memory. Contact-area changes
change stored tangential spring energy; full thermodynamic validation for arbitrary
combined normal/tangential cycles remains future work. Reset history when switching
model/material. No claim that adding Hertz automatically calibrates real powder.

Normal damping remains cn*compression_speed ONLY during compression; tangent
viscous damping is ct*slip. cn and ct are constant Ns/m coefficients, not restitution
inputs or Hertz viscoelastic damping parameters. kn/kt linear settings are ignored
for Hertz; elasticity sets stiffness. Linear selection ignores Hertz properties.

Small-overlap cap defaults to 1% of the smaller radius (plane: particle radius),
configurable up to 5%. This is a numerical/model-scope guard, not proof Hertz is
accurate up to that value. Both input and predicted next overlap are checked;
failure preserves particles, forces and history. dt limit uses the MAXIMUM
normal derivative and tangential stiffness at this cap, including rotational
inertia. Refine dt for each new loading condition; no automatic dt adaptation.

Validation: hertz_tests / CTest hertz_mindlin. Independently checks modulus,
force exponent, energy derivative, Mindlin stiffness, undamped maximum overlap
from energy and contact duration from a beta-function integral, damping refinement,
plane/pair force integration, linear selection and invalid material/overlap guards.
The original CLI remains linear-only; these new models are selected through the
C++ API and demonstrated in hertz_tests. Existing regression tests remain enabled.

Reference: https://docs.lammps.org/stable/pair_granular.html
No external solver is executed. Multi-particle integration is described below.
## Task 07: many-body search and deposition

Include powder/world.hpp and create ParticleSystem with particles, planes, model
and gravity_m_s2. Call step(dt) for each fixed physical step; step(dt, true) uses
all-pairs contact search for reference. This API computes states, not video playback.
The existing single-particle CLI and Unreal Chaos scene are unchanged.

The grid cell width is the largest diameter; 27 neighbouring cells cover every
overlapping sphere pair, including mixed sizes and negative coordinates. Contacts
are canonical ID pairs, sorted and evaluated once. Extremely broad size distributions
can reduce efficiency. Particle IDs and wall IDs must each be unique.

All contact forces and torques are evaluated at the same old state; gravity is
added once and each particle is integrated once. Tangential history is stored per
pair or particle/wall, and pruned at the next evaluation after separation. Summation
order is deterministic by ID and does not depend on the caller's particle order.
Call reset_history() after material/geometry changes or reusing IDs.

Linear and simplified Hertz/Mindlin models share this pipeline. All contacts use
the shared MaterialContact settings. Walls are fixed infinite planes with unit
normals; valid centres lie on the positive side. Finite boxes/pipes are task 08.
The step commits states and histories together only after successful validation.

The dt guard accounts for current contact degree, minimum mass, stiffness and
damping; Hertz uses stiffness at the overlap cap. It is conservative but not a
general stability proof. Refine dt for each new loading case. Step travel is limited
to 10% of the smallest radius; this discrete guard is NOT continuous collision
detection. Deep overlap/invalid geometry and Hertz cap violations are rejected.
StepReport contacts/overlap refer to pre-integration geometry; speed is post-step.
distance_checks counts the principal search, not additional pre/post geometry checks.

CTest many_body verifies grid/all-pairs contact equality, all state components
under particle reordering, simultaneous force balance, history cleanup, error
atomicity and linear/Hertz paths. A three-sphere vertical stack settles over 5s
to analytic weight/compression equilibrium, and halving dt agrees within 1e-6m.
These are synthetic numerical cases, not calibrated powder validation.

Scale checks cover 100/300/1000 particle searches and 27/125/300 particle integration
over only 100 steps; they do not establish large-pile equilibrium. See Docs/DEM
for the detailed Korean results, timing, limitations and task 01–07 records.
Run all CTest targets plus tests/cli_tests.py, tests/motion_tests.py and
tests/contact_cli_tests.py (each accepts the powder_run executable path).

## Task 08: finite oriented boxes and Unreal geometry import

ParticleSystem.boxes accepts FixedBox (SI centre, positive half extents, orthonormal
world axes, stable ID and optional weld_group). Sphere contact uses the closest
point on the OBB, including faces, edges and corners. Each contact contributes
forces/torques in the existing simultaneous integration step. Centres on/inside
solid boxes are rejected. Static boxes only; arbitrary mesh/shear/moving walls
and continuous collision detection are not implemented.

Nonzero weld_group marks pieces of one surface assembly. Coplanar face histories
share a key, duplicate support faces and artificial seam edge contacts are removed,
and points strictly buried inside another piece are discarded. Genuine perpendicular
constraints remain distinct. This is a restricted welded-box manifold, not a general
Boolean-union collision solver. Coplanar pieces in a group are assumed continuous;
contact gaps prune histories on the next evaluation. Explicitly reset history after
geometry changes or a re-export, since exported IDs can change after actor edits.

Edge/corner forces use the closest-point normal and an equivalent tangent plane.
The Hertz path therefore uses the sphere/plane effective radius at sharp edges too:
this is a force approximation, not the exact sharp-edge Hertz stress solution.
Rounded edges and experimental calibration are future work.

Scripts/export_dem_geometry.py reads PowderRig-tagged BasicShapes/Cube components
from L_Main, preserving component world transforms (including nonuniform/negative
scale). cm positions become metres; the 100cm cube becomes half_extent=abs(scale)*0.5m.
No Euler reinterpretation is used. Other meshes fail explicitly. All current rig
pieces use weld_group=1 and include cutaway walls, regardless of rendering visibility.
The script writes fixtures/L_Main.boxes and the actor-ID manifest. read_boxes(stream)
loads this versioned SI snapshot. The exported fixture contains 53 saved level boxes.

In the editor Python console with L_Main open:
```python
exec(open('D:/GitHub/VirtualGameTeamProj/Scripts/export_dem_geometry.py', encoding='utf-8').read())
```
For saved-level commandlet export/transform verification:
```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$PWD/VirtualGameTeamProj.uproject" -run=pythonscript "-script=$PWD/Scripts/test_dem_geometry_export.py" -unattended -nullrhi -nosound
```
The exporter does not save or mutate the level. Commandlet export reads disk state;
unsaved editor transforms require the in-editor invocation. Runtime particle display
and simulation controls remain task 09, not part of this geometry snapshot bridge.

CTest box_geometry checks independent face/edge/corner distances and edge force,
rotation/reflection, welded seam trajectories against a plane with friction/spin,
Hertz forces, invalid input atomicity, open tube endpoints, 360 wall samples and a
complete feeder-to-collector drop using the exported 53-box fixture. The synthetic
20cm/1kg particle's maximum overlap is 3.50mm (<5% radius) with kn=4e6, dt=2e-6s;
it rebounds after impact. This is not a calibrated powder or static-settling result.
Unreal's own Transform API is separately tested for combined rotations and scale.
Box enumeration is currently brute force; seam welding adds box comparisons when
touching. See Docs/DEM task 08 report for timings, limitations and full results.

## Task 09: Unreal single-particle bridge and controls

The Unreal module compiles these same solver sources via DEMCore_*.cpp wrappers,
with separate translation units and exceptions enabled. Session is a render-neutral
fixed-step controller: frame(work_budget) executes steps only when running;
single_step pauses and executes exactly one; reset restores the initial state and
clears contact histories, clocks and faults. A failed step stops and does not advance
time. Render DeltaSeconds never enters the solver.

APowderDEMTest replaces the default 300-particle Chaos test at Play. It reads current
PowderRig cube component transforms on reset, displays one solver-controlled sphere,
and disables Chaos simulation, gravity and collision on that sphere. Static box
geometry is frozen until reset. No file replay is involved. The actor uses the linear
contact model with the synthetic 20cm/1kg task 08 parameters; this is not calibrated
powder. Parameters are editable in the actor's DEM properties and apply on R.

SPACE starts/pauses; N pauses and advances one dt; R resets and rereads the rig;
F focuses the particle, Home resets view and T focuses the tube. Existing flight
camera input stays available while the solver is paused. HUD separately displays
physical simulation time, wall time since reset (including paused time), and elapsed
time inside compute calls. Default dt=2e-6s and budget=500 steps/render callback.
Rendering FPS affects throughput, not states at equal completed physical steps.
No catch-up steps are dropped. This synchronous game-thread bridge can stall under
heavy work; background computation and multiple particles are task 10.

CTest session_controls verifies deterministic frame partitioning, pause/single-step,
reset history and fault stopping. Unreal automation Powder.DEM.SingleParticleBridge
verifies actual actor mesh/control/geometry integration, comparing 30 callbacks of
400 steps and 120 callbacks of 100 steps (DeltaSeconds=1/30 vs 1/120). At 12000 steps
both solver and displayed positions match exactly on this build. Ordinary component
movement suppressed tiny changes, so visual updates use the NoPhysics transform path
and explicit propagation; no relaxed assertion was used to hide that discrepancy.

The actual L_Main Play UI was also checked for N, SPACE, R and F. See the task 09
report in Docs/DEM for build commands, test logs, default parameters and limitations.

## Task 10: background worker and 300-instance display

Worker owns a single std::thread and its Session. It never accesses engine objects.
Commands and immutable shared_ptr<const Snapshot> pointers cross a short mutex;
physics is computed outside that mutex. Running publishes every 8 completed steps;
pause/single-step/reset/fault publishes immediately. The renderer consumes the latest
snapshot only: intermediate visual frames can be omitted, physical steps cannot.

reset(world,dt) advances a generation, invalidates the visible snapshot and asynchronously
validates the replacement on the worker. Older completed computations cannot publish
into a newer generation. Pausing stops at a step boundary. N queues one additional step
and pauses; an already in-flight step may finish first. Toggle/pause cancels unexecuted
N requests. Destruction signals stop and joins; there are no detached threads or UObject
callbacks. Shutdown may wait for the in-flight step or initial validation to finish.

APowderDEMTest now polls snapshots and displays InstancedStaticMesh spheres on the game
thread. It reads rig geometry on reset only. Default: 300 synthetic 4cm particles,
density 2500kg/m3, 5x5x12 grid at 1.25 diameters spacing starting z=4.5m, dt=2e-7s,
linear kn=4e6, cn=40, kt=1e6, ct=10, mu=.3. Count remains editable from 1 to 300.
These are numerical test settings, not calibrated powder. The old StepsPerFrame
property is removed; rendering no longer controls solver throughput.

Controls remain SPACE/N/R/F/Home/T. HUD includes initialization state, generation,
particle count and separate physical/wall/compute clocks. Simulation time is shown
with 9 decimal places. Async controls become visible when the worker acknowledges.
Chaos physics/gravity/collision are disabled on all displayed instances. Render
instance matrices have finite display precision; solver state remains double.

CTest background_worker compares 300 particles/53 actual fixture boxes against 100
sequential reference steps, tests immutable snapshots, pause acknowledgement, 20
reset-while-running cycles, invalid initialization and shutdown during initialization
or work. Engine automation is now Powder.DEM.BackgroundBridge, checking actual
instances, async controls and fresh snapshot consumption. In one run 93 fresh
snapshots were rendered with max measured actor Tick ~4.50ms (not a full FPS benchmark).

The 100-step test covers only 20 microseconds of physical time. Backgrounding improves
UI responsiveness, not physics throughput. The small dt and current geometry searches
make long physical runs slow. No long-duration 300-particle passage/settling result is
claimed. Task 11 adds tracking/passage accounting. See Docs/DEM task 10 for details.


## Task 11: ID tracking, passage events and conservation

Tracker runs after every successful solver step, on the same worker as the physics.
Snapshots carry immutable particle states and tracking records from the same step.
Reset clears both, and retired generations cannot publish into a new run.

Current locations are exclusive: Feed (including funnel), Tube, Collector, Outside.
The flight gap is Outside, not lost mass. Counts and mass sum over every ID, including
previously passed particles. IDs are stable even if the particle array is reordered;
changing IDs/count/mass within a run is rejected until supply is implemented in task 13.

Passage means the CENTRE crosses a finite opening, not whole-sphere clearance.
Entry-down, exit-down, exit-up and entry-up events have physical times interpolated
on each step's linear position segment. A single step may cross both gates. Entry
admission followed by exit-down completes a passage. Side escape cancels admission;
starting inside does not manufacture an entry. Unique IDs and repeated events are
separate. First/last exit timestamps describe exit-down events, even if unqualified:
use ever_passed/completed_passage to check a full traversal. -1 means no such event.
Current-region location_since_s is a step-end observation time, not an interpolated
region-transition time. Full event vectors remain in memory until reset/shutdown;
file export and residence-time analysis are deferred to task 14.

tracking_layout derives convex interior half-spaces and gate caps from the same
current wall boxes read on R. Tube walls must share an axis and end planes. Common
rigid transforms and dimension edits are supported; arbitrary bent tubes/meshes are
not. Unreal uses semantic PowderFeed/PowderFunnel/PowderTube/PowderCollector tags,
with editor-label fallback for the existing saved rig. Rebuild scripts now supply
tags, but the existing level was not resaved: packaged use requires persisting tags.
A rig without any groups explicitly shows tracking OFF; partial rigs fail validation.

HUD shows counts, unique passage history, total mass and balance error. InspectParticleId
selects the displayed ID; Blueprint ParticleTracking(Id) returns a readable record.
Default parameters and camera controls are unchanged. This is not a speed optimization.

CTest particle_tracking covers conservation (1e-10 kg), exact integer counts, linear
crossing times (1e-12 s), array reordering, invalid state changes, fast crossing,
reverse/reentry, side escape, starting inside, reset, snapshot immutability and the
actual 53-box fixture under a rigid transform (1e-10 SI). Kinematic diagnostic paths
are not claimed as physically validated trajectories. Engine TrackingBridge also
tests real actors, moved rig classification and initialized actor EndPlay cleanup.

The archived Chaos log really contains total=300 and entered=passed=298. A reproducible
diagnostic using two near-wall paths demonstrates that the old radius-shrunk predicate
can report 298 while centre crossings report 300. The original two trajectories were
not recorded, so the historical cause remains unresolved. Outside-aperture and side
bypass tests prevent forcing the count to 300. No long-duration 300-particle passage
experiment is claimed. See Docs/DEM/작업11_입자추적_통과보존_완료기록.md.

## Task 12: virtual powder settings and deterministic initialization

`PowderSettings` supports mono or number-uniform diameters, density, count (1–300)
or target mass with absolute tolerance, seed, independent particle/wall contact
parameters, spawn volume, initial velocity, gravity and fixed dt. Linear and
Hertz–Mindlin contact models remain selectable. Wall elasticity affects wall
contacts only; legacy callers without `wall_model` retain their shared model.

`serialize_settings` / `parse_settings` use strict versioned UTF-8 text with SI
units and 17-digit values. `make_initial` returns an all-or-nothing batch using
seeded `lattice_v1` candidates, checking particle/particle and particle/wall
clearance. Mass mode chooses the nearest nonempty seeded prefix around the
requested mass, without resizing particles or changing density. Unsatisfied
mass tolerance, capacity or placement budgets fail explicitly.

Worker configuration performs placement off the game thread. The same settings,
seed, geometry and build reproduce initial IDs, diameters and positions. This is
not dense random packing; failure does not prove all possible packings impossible.
Presets do not contain geometry or simulation checkpoints.

In PIE, edit PowderDEMTest Details and press R to apply. K saves settings and L
loads and resets; Space toggles running and N advances one physical step.
Default file: `Saved/DEM/PowderSettings.dem` (ignored by Git). Invalid loads retain
the current settings and run. Current defaults are synthetic, uncalibrated powder.

Validation: all 11 CTest targets, three CLI regressions and all three Unreal
`Powder.DEM.*` automation tests passed. The added settings tests cover deterministic
round trips, independent mass and overlap checks, separate wall properties,
invalid inputs and failed/cancelled initialization. Final K/L behavior was also
verified in the editor. See `Docs/DEM/작업12_가상분말설정_초기배치_완료기록.md`.
## Task 13: batch release and timed supply

`Supply` separates a seeded finite inventory from the active solver particles.
Batch mode validates the full initial layout and releases it once at `supply_start_s`.
Timed mode requests `supply_per_event` particles every `supply_interval_s`, starting
at `supply_start_s`. It retries occupied inlets at `retry_interval_s` or on a new
request. All times use the fixed-step physical clock; requests are never advanced
to match rendering speed. Events between steps are processed at the next boundary.

FIFO admission checks existing particles and static walls. The oldest blocked
particle and all later requests remain pending; candidates are never forced into
overlap. The same seeded lattice is searched again up to the candidate budget.
Backlog may be admitted together when space becomes available. Batch mode still
requires the entire initial batch to fit, while timed mode can reuse a small inlet.

`SupplyReport` partitions planned inventory into inserted + pending + future, with
requested = inserted + pending, for both counts and mass. Tracker admission starts
at each particle's birth position/time and retains existing trajectories and contact
history. Reset clears the schedule, backlog and tracking together. Maximum inventory
remains 300 particles; no screw, valve, unlimited emitter or constant kg/s controller
is modeled.

Unreal Details exposes DEM/Supply controls; R applies, K saves, L loads and resets.
Schema 2 stores supply settings. Schema 1 remains readable as immediate Batch mode.
Presets are not checkpoints. The default remains immediate release of 300 particles.

Validation: 12 core test targets passed, including scheduled admission, blocked-inlet
FIFO recovery, independent mass and free-flight checks, cancellation, birth tracking,
and worker reset. All four `Powder.DEM.*` Unreal automation tests passed with no
warnings. See `Docs/DEM/작업13_배치방출_시간별공급_완료기록.md` for assumptions and use.
## Task 14: experiment records and comparisons

ResultMonitor observes every accepted physical step. It records gross/net outlet
mass, unique completed-passage mass, spatial inventories, supply backlog and
low-speed/no-outflow duration. Defaults: sample every 0.01 physical seconds,
speed threshold 0.001 m/s, long-duration threshold 1 s, 10,000 scheduled samples.
These are explicit classification criteria, not a validated arching detector.
Supply completion and outlet stagnation are independent states.

Pause the Unreal run and press O (or call ExportResults). Each export writes a new
Saved/DEM/Results/<UTC>_<GUID> directory with metadata.json, summary.json,
timeseries.csv, particles.csv, events.csv, settings.dem and run.json. complete.json
is written last. Partial bundles are rejected by the comparison reader. A final
sample is appended if export occurs between scheduled samples. At the sample limit,
the worker pauses without dropping its current result; export and reset to continue.

Passage residence uses each completed entry-to-exit event, including repeated passes.
Unfinished particles are not zero-duration samples; empty statistics are null. Gross
outlet mass counts recrossings, net mass subtracts reverse crossings, and unique
passage mass counts IDs once. Positions/velocities are final-state SI values.
Code version/build stamp, dt, seed, active settings, measurement geometry and criteria
are recorded. Git source_revision is unavailable (null); equal versions do not prove
identical binaries. Criteria are recorded in metadata, not restored by the K/L preset.

From the project root:

```powershell
python Scripts/compare_dem_results.py "Saved/DEM/Results/<A>" "Saved/DEM/Results/<B>" --output "Saved/DEM/comparison.json"
```

Comparison requires equal metric definition, code version, measurement regions,
sampling/slow/stall criteria and exact physical observation time. It reports A, B and
B-A, and flags changed settings, geometry, dt or seed. Existing output files are never
overwritten. Runs without tracking cannot provide passage comparisons.

Validation: 13 core tests passed, independent Python JSON/CSV checks verified analytic
transit time and flux, and a second density condition doubled mass without changing
transit time. Five Unreal automation tests passed, including actual file publication,
unique destinations, running-export rejection and result-limit handling.
See Docs/DEM/작업14_실험결과저장_조건비교_완료기록.md for definitions and limitations.
## Task 15: repeatable baseline validation

The physical core remains version 0.14.0; DEM_validation_v1 adds verification tools.
From the project root, run:

```powershell
python -B Scripts/run_dem_validation.py --output Saved/DEM_validation_new --repeats 2 --unreal-editor "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
```

Use a new output directory. The optional Unreal check requires an up-to-date Editor
module. The runner builds the Release core, repeats 13 CTest tests, CLI/result checks,
5 Unreal tests when requested, and 42 benchmark cases. It records source and binary
hashes, fixed acceptance criteria, CSV final states, convergence, conservation,
overlap and instrumented performance. Existing output is never overwritten.

The 30/100/300-particle matrix covers only 20 microseconds; the complete 1.5-second
rig drop is a separate enlarged single sphere. This is not a full 300-particle
discharge validation or real-powder calibration. Timing includes metric collection,
and short-window cost ratios do not guarantee full-run completion times. See
Docs/DEM/task15/report.md and the task 15 completion record for measured limits.
