# Powder free-fall test

Open `L_Main` and press Play. The default `PowderDropGameMode` spawns the test at runtime; the level asset itself does not contain 100 saved particle actors.

- 100 spheres, 5 × 5 × 4 non-overlapping initial arrangement.
- Radius 3 cm, mass 0.01 kg per proxy; deliberately enlarged for visibility, not measured powder properties.
- Bottom layer centers start 200 cm above the test platform; initial velocity is zero.
- After 1.5 seconds, Chaos gravity and sphere/sphere and sphere/platform contacts are enabled.
- Gravity: project default −980 cm/s². Linear drag: zero. Friction: 0.65. Restitution: 0.15.
- Angular damping: 3.0, an illustrative rotational dissipation to reduce prolonged rolling after impact.
- CCD is enabled to reduce tunneling. Physics results can differ slightly across frame rates/machines.
- R: reset the same seeded initial arrangement. Space: pause/resume. Escape: stop PIE.
- Click the play viewport first if keys do not respond.

This is a gravity/contact demonstration, not molecular dynamics, calibrated powder DEM, or a process/mixing simulation. No machinery or original Blender assets are imported. There are no molecular forces, air flow, cohesion, or quality predictions.

The HUD shows particle count, moving count and time since release. `PowderDrop CHECK` entries in Output Log report counts, platform escape checks, mean bottom clearance, minimum center height, and maximum speed every two seconds.

Implementation: `Source/VirtualGameTeamProj/PowderDropTest.h/.cpp`. Build target: `VirtualGameTeamProjEditor`, Development, Win64. Teammates need the same UE 5.8 version and C++ toolchain. Build output is ignored by Git.

To return to the normal game mode later, change Project Settings → Maps & Modes → Default GameMode (currently PowderDropGameMode), or remove `GlobalDefaultGameMode` from `Config/DefaultEngine.ini`.
