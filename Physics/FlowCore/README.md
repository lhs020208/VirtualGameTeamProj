# FlowCore air 0.2.0 / one-way 0.3.0 / dilute two-way 0.4.0

Standalone C++17 in-house air-flow solver. The supported verification domain is near-atmospheric, isothermal incompressible laminar air in a 2D parallel-plate channel. Conservative Reynolds bound ≤100; SI input, fixed explicit time steps. The air-only solver has no particle feedback. The separate one-way adapter below adds isolated particle motion. No turbulence, strong vacuum, circular/curved pipes or Unreal visualization.

The solver uses MAC finite volumes, donor-cell advection, central viscosity, explicit Euler and a non-incremental pressure projection with matrix-free Jacobi-preconditioned CG. A step commits only after pressure, continuity and field-envelope checks pass. No clipping or skipped physical time.

From the Unreal project root:

```powershell
python Scripts/run_flow_solver_validation.py --output Saved/Flow_solver_new_check
& ./Intermediate/FlowCore/Release/flow_run.exe Physics/FlowCore/fixtures/pressure_channel.flow Saved/MyAirRun
& ./Intermediate/FlowCore/Release/flow_contract.exe Physics/FlowCore/fixtures/pressure_channel.flow
```

The runner builds using the installed Visual Studio 2022 Community CMake. Output directories must be new; their parent must exist. `flow_run` writes normalized input, contract estimates, every accepted step's diagnostics, the last accepted MAC field and a success/failure summary. Code 0 means completion, 2 input/I/O rejection, 3 numerical failure. Failed numerical steps preserve the previous state and clock. Wall-clock duration is separate from physical time.

`flow_contract` only validates input and reports analytical estimates (`flow_evolved:false`). It does not run the solver. `unsupported_vacuum.flow` must be rejected by both tools.

See [task 23 implementation and verification](../../Docs/DEM/작업23_공기유동계산_완료기록.md) and [task 22 contract](../../Docs/DEM/작업22_유동모델_격자_경계조건_완료기록.md). Repeated numerical evidence is in `Docs/DEM/task23/`. These numerical verification cases are not experimental validation of real powder transport.

## Task 24: air-to-particle coupling

`flow_one_way` reuses the existing PowderCore Particle, ForceAccumulator and translation integrator. It bilinearly samples MAC velocities, linearly interpolates consecutive computed air states, and substeps particle gravity + Stokes drag. Both phases commit together or roll back. Air never reads particle state; this is not two-way CFD-DEM.

Restricted to isolated nonrotating heavy spheres: Re_p<=0.1, estimated Kn(d)<=0.01, density ratio>=100, diameter<=0.1 cell, wall distance>=50 diameters, separated test particles and a small global slab mass loading. Buoyancy, history/lift/pressure-gradient forces, slip correction, torque and particle/wall contacts are not implemented in this adapter. Passing these guards is not experimental validation.

```powershell
python Scripts/run_one_way_validation.py --output Saved/Flow_one_way_new_check
& ./Intermediate/FlowCore/Release/flow_particle_run.exe Physics/FlowCore/fixtures/one_way_channel.flow Saved/MyParticleRun
```

The CLI initializes one 30 micrometre, 1000 kg/m^3 sphere at the channel centre with gravity (0,-9.81,0), 20 particle substeps per air step. Optional arguments after the output directory: `diameter_m density_kg_m3 integer_substeps`. Use a new output directory. The short fixture runs 0.05 s; longer runs can correctly fail upon approaching an unsupported wall/open boundary.

Outputs: normalized `input.flow`, `particle.json`, initial and accepted-air-step `particles.csv`, and `result.json` with explicit one-way/no-reaction flags. Numerical failure preserves the last accepted state (exit 3); input/I/O error is exit 2. See [task 24 completion record](../../Docs/DEM/작업24_공기입자_단방향연성_완료기록.md) and `Docs/DEM/task24/` for assumptions, analytical checks, exact-repeat evidence and remaining integration work.

## Task 25: conservative volume diagnostics and dilute two-way forces

`flow_two_way` adds normalized fixed-physical-support tent kernels, conservative cell solid-fraction mapping, adjoint velocity gathering/reaction spreading, and actual momentum-source injection before air pressure projection. Both phases use the same substep and commit atomically. Particle ID sorting makes accumulation order deterministic.

This is a **dilute regularized point-force approximation**, restricted to mapped alpha<=0.001 and isolated Stokes spheres. Solid/void fractions are diagnostics and scope guards; finite-porosity continuity/momentum, dense drag, self-disturbance correction and contacts are NOT implemented. The physical slab depth (`reporting_depth_m`) determines gas control-volume mass and therefore feedback strength. Kernel support must remain interior; no truncated boundary forcing.

```powershell
python Scripts/run_two_way_validation.py --output Saved/Flow_two_way_new_check
& ./Intermediate/FlowCore/Release/flow_two_way_run.exe Physics/FlowCore/fixtures/two_way_channel.flow Saved/MyTwoWayRun
& ./Intermediate/FlowCore/Release/flow_two_way_run.exe Physics/FlowCore/fixtures/two_way_channel.flow Saved/MyMatchedControl 20 0
```

Optional arguments are coupling substeps and reaction flag 0/1. The fixture uses a 30 micrometre, 1000 kg/m^3 sphere, zero gravity, fixed 2.5 mm kernel, a 1 mm representative depth and 0.01 s physical duration. API settings allow other validated cases. Outputs include `exchange.csv`, `volume_pressure.csv`, `air_velocity.csv`, settings and explicit success/failure metadata. Force-exchange balance excludes pressure, viscous and open-boundary momentum changes. The control uses the same kernel and time integration as the two-way model.

See [task 25 model and verification](../../Docs/DEM/작업25_체적분율_양방향힘교환_완료기록.md) and `Docs/DEM/task25/`. Fixed-kernel numerical convergence does not establish real-powder or 3D particle-scale accuracy. Unreal integration remains separate.

## Task 26: buffered channel supply and collection

`flow_channel` adds scheduled FIFO particle injection and virtual collection planes **inside** a padded gas domain. Full volume/force kernels stay interior; particles do not cross the external CFD pressure faces. The dilute isolated-sphere limits of task 25 remain in force. Injection waits for 55-diameter clearance, while the 50-diameter model guard remains active throughout integration.

```powershell
python Scripts/run_channel_validation.py --output Saved/Flow_channel_new_check
& ./Intermediate/FlowCore/Release/flow_channel_run.exe Physics/FlowCore/fixtures/buffered_channel.flow Physics/FlowCore/fixtures/buffered.channel Saved/MyChannelRun
```

The `.channel` file specifies ports, kernel radius, coupling substeps, reaction flag, gravity and scheduled releases (`ID requested_time_s diameter_m density_kg_m3 y_m velocity_x_m_s`). The fixture calculates 0.3 physical seconds with three 30-micrometre virtual spheres. Input and model errors are rejected; output directories must be new.

`history.csv` records pressure, supply/collection masses, gas balance and test/buffer kernel partitions. `transits.csv` records requested and actual entry times, interpolated crossing times and end-of-substep removal times. Residence time excludes supply waiting. Centre-count mass and filtered test-region volume are distinct measurements. Reaction-source balance is not whole-system momentum conservation. Exit code 0 means the requested duration completed; `all_arrived` separately reports whether every scheduled particle arrived. A numerical failure returns 3 and preserves the last accepted state including queues/events; input/I/O rejection returns 2.

Eleven CTest suites run twice, including independent filtered Poiseuille transit, grid/time refinement, pressure and arrival-mass convergence, source delay, partial completion, atomic failure and prior regressions. See [task 26 completion record](../../Docs/DEM/작업26_관로공급배출_기준시험_완료기록.md) and `Docs/DEM/task26/`. This remains a standalone dilute planar numerical baseline; dense powder, actual vacuum/3D equipment and Unreal integration need further work.
