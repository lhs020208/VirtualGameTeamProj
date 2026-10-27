# Particle authoring integration — step 01

Selected from origin/bbangkyu commit 32b83103c0f4c4def202a5b97bdee4fa62c892a4. Native module/class names are retained for asset compatibility.
Only DEMParticleSettings, DEMEmitter and their necessary assets are included. CPU DEMWorld/solver, maps, editor commandlets and machine-specific tools are not imported.

Emitter creates static particles and editor previews only. No GPU integration yet. Contact properties and angular velocity are stored data, not active simulation parameters. Existing GPU mixer levels are unchanged.

Create/duplicate a DEM Particle Settings asset, assign a sphere mesh, place BP_DEMEmitter, choose the settings asset and use Refresh Preview. Generate exports a value snapshot; Clear removes it. World scale must be one. Preview is limited to 2048, actual burst to 100000. Generate replaces a burst, not continuous emission.

The data coordinate convention is right-handed SI world space with Y reflection; it is not the GPU mixer's local frame. The adapter is a later task.

## Step 02: GPU settings contract
Explicitly select GPU Approximation before assigning an asset to PowderGranularMixer.ParticleSettings. Stiffness/damping are disabled and not consumed by GPU physics. Spin is unsupported. Particle/wall friction must match in [0,1]; float radius must be >1mm and <30mm. Density affects reported mass only. DA_DEMParticle_GPU_Default matches the existing 50000-grain charge. Null mixer settings preserve legacy behavior. Reset applies changes. Emitter positions are not yet connected (step 03).

## Steps 03–06 complete
The GPU adapter is now connected through APowderGranularMixer.UseEmitters and its explicit Emitters list. GPUGroup selects red/blue tracer identity. Editor previews are capped; runtime displays all particles using each emitter's mesh/material. An optional local position list preserves vessel-fitted layouts. Changes/removal/Generate/Clear while owned require mixer Reset. Initial vessel/rotor/particle overlaps are rejected. Full usage: Docs/DEM/PowderEmitterIntegration_사용법.md. Results: Docs/DEM/emitter_integration06/결과.md. Earlier step descriptions above document intermediate states, not the current integration limit.
