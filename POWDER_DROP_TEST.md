# Powder free-fall test

Open `L_Main` and press Play. The default `PowderDropGameMode` spawns the test at runtime; the level asset itself does not contain 300 saved particle actors.

- 300 spheres, 5 × 5 × 12 non-overlapping initial arrangement.
- Radius 3 cm, mass 0.01 kg per proxy; deliberately enlarged for visibility, not measured powder properties.
- Bottom layer centers start at Z=450 cm inside the feed chamber; initial velocity is zero.
- After 1.5 seconds, Chaos gravity and sphere/sphere and sphere/platform contacts are enabled.
- Gravity: project default −980 cm/s². Linear drag: zero. Friction: 0.65. Restitution: 0.15.
- Angular damping: 3.0, an illustrative rotational dissipation to reduce prolonged rolling after impact.
- CCD is enabled to reduce tunneling. Physics results can differ slightly across frame rates/machines.
- R: reset the same seeded initial arrangement. Space: pause/resume. Shift+Escape: stop PIE (current editor shortcut).
- Click the play viewport first if keys do not respond.

## Camera navigation during Play

- Hold right mouse: mouse look; W/S forward/back, A/D strafe, Q/E world down/up.
- Hold right mouse and scroll: change flight speed (10–4000 cm/s; default 200).
- Hold Shift while flying: temporary 3× speed.
- Middle mouse drag: pan. Wheel without right mouse: move forward/back.
- F: frame the powder at its current mean height. Home: restore the original overview.
- Space pauses powder physics while camera navigation remains available.
- R resets the powder without resetting the camera. Shift+Escape stops Play with the current editor shortcut; Shift+F1 releases the mouse to the editor.

This mirrors the editor's common flight controls, not every editor orbit/selection shortcut or the user's editor preference settings. Navigation has no collisions and does not push particles.

This is a gravity/contact demonstration, not molecular dynamics, calibrated powder DEM, or a process/mixing simulation. No machinery or original Blender assets are imported. There are no molecular forces, air flow, cohesion, or quality predictions.

The HUD shows particle count, moving count and time since release. `PowderDrop CHECK` entries in Output Log report counts, platform escape checks, mean bottom clearance, minimum center height, and maximum speed every two seconds.

Implementation: `Source/VirtualGameTeamProj/PowderDropTest.h/.cpp`. Build target: `VirtualGameTeamProjEditor`, Development, Win64. Teammates need the same UE 5.8 version and C++ toolchain. Build output is ignored by Git.

To return to the normal game mode later, change Project Settings → Maps & Modes → Default GameMode (currently PowderDropGameMode), or remove `GlobalDefaultGameMode` from `Config/DefaultEngine.ini`.

## Saved powder test rig (2026-10-21)

L_Main now contains 53 editable basic cube actors under PowderTest: 16 feed walls, 16 tapered funnel walls, 16 tube walls, and a five-piece collection bin. The former seven random collision obstacles are removed from runtime generation. Lighting and the original ground remain.

All coordinates are centimetres, centred at XY (0,0). Feed chamber inner diameter 60, Z 440..580; funnel tapers to inner diameter 24 over Z 360..440; open tube inner diameter 24, Z 240..360; bin floor top Z104, inner width152, rim Z210. The runtime safety platform remains underneath. Spawned particles start at Z450..549 in a 5x5x12 arrangement and are not saved to the level. This is a batch release, not a metered feeder or valve model.

Front-facing feed/funnel/tube walls and the front bin wall are hidden during Play for observation; all retain collision. Editor mode shows the complete fixture. Runtime assigns the contact material to PowderRig-tagged actors. T focuses the tube; Home restores the full rig view. R resets particles and counters.

The tube counter uses the fixed tube dimensions in PowderDropTest.h. Moving/resizing tube actors manually does NOT automatically update the measurement planes; update both until configurable sensors are implemented. Moving feed walls also does not move the particle spawn volume automatically.

Scripts/build_powder_rig.py rebuilds only PowderRig-tagged actors in L_Main using the Unreal Python commandlet with PythonScriptPlugin enabled; it overwrites manual edits to these tagged fixtures. No Python plugin is required for normal Play.

Counts are simulation diagnostics, not virtual sensor measurements. Passage is a unique admitted particle reaching the downstream exit. Return counts are historical and can overlap eventual passes. Contact callbacks are not unique physical impacts. This is an enlarged-particle Chaos contact experiment, not calibrated DEM or CFD-DEM.