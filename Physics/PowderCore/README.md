# PowderCore 0.7.0 — tasks 01 through 07

Standalone C++17 translational dynamics with gravity and constant external forces. Optional horizontal-plane normal contact; no Unreal dependencies. Existing Unreal Chaos simulation is unchanged.

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
