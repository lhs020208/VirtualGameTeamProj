# Full-process hybrid collision test

Level: `/Game/ProcessPlant_20261029/L_ProcessPlant_HybridCollisionTest`

Created as an additional copy of FlowTest. Existing levels and default startup map remain intact. Restart the editor to load the newly built module (1071), then open this level. Actor `Plant_Hybrid_Pressure_Grains_Test` uses `SourceAssets/ProcessPlantHybrid`. There are 10 particles of each of three species (30 total), radius 2 mm. The existing manual supplier, weighing hopper, receiver outlet, and mixer controls remain available. Valves start closed; mixer starts stopped. Simulation pacing is uncapped; display limit remains 30 FPS.

## Collision representation

- Narrow/bent pipes, necks, valves and connection sleeves retain original triangle geometry.
- Supplier hoppers, weighing hopper, receiver walls, mixer vessel and rotor use connected coplanar source-aligned wall patches where possible. These are thin solid wall slabs, not a union of hollow interior boxes.
- Rectangular patches use oriented boxes. Nonrectangular patches use an OBB candidate bound plus original polygon-edge clipping, including holes. This avoids closing openings or extending cone facets into the passage.
- Curved surfaces retain the original faceting. Maximum accepted plane fitting error is 1 micrometre; wall half-thickness is 2.5 micrometres plus fitting error. This geometric tolerance does not imply equivalent contact trajectories.
- Unconvertible patches retain triangles. Shaft/end-wall interfaces near mixer x=2.25 m and x=3.75 m deliberately use original mesh queries to preserve overlapping connection geometry.
- Per-step static/rotor stackless AABB BVHs reject distant candidates using particle position and radius. Rotor queries use rotor-local coordinates. Leaves contain at most eight shapes. No capped candidate list or stale cached list is used.
- Rendering, pressure network and particle-particle physics are unchanged by the hybrid mode. Hiding equipment still changes rendering only.

## Generated counts

| Geometry | Original triangles | Hybrid shapes | Wall patches |
|---|---:|---:|---:|
| Static | 211124 | 185682 | 10470 |
| Rotor | 8956 | 2726 | 2685 |

WH body, cone and top cover convert completely. Supplier A/C each retain four triangles; B converts completely. Mixer vessel converts 4711/5156 source triangles (about 89.9% of surface area); rotor converts 8915/8956 (about 99.996% of area). The remaining detailed pipe geometry still dominates static shape count. Original reference geometry also remains in the packed file for interface fallback, so these counts do not imply a matching memory reduction.

## Validation and limits

- Editor Development build succeeded, suffix 1071.
- Original and hybrid manual-valve tests passed (independent supplier gates, weighing/receiver gates, pending closure and pressure-field refresh, mixer start/stop).
- 7176 surface probes across three rotor orientations compared against the source model. Maximum position difference after contact projection: 5.623 mm; maximum source re-projection correction: 1.001 mm. Aggregating triangles changes sequential contact resolution. This is not contact-equivalence certification.
- Sampled hybrid BVH results matched the same geometry queried without BVH culling (maximum position difference zero). This is a sampled audit, not a proof for every possible trajectory.
- 1656 route-centre samples matched reference boundary projection exactly. This checks comparison at sampled points, not that all routes or passage widths are certified unobstructed.
- Mixer fixture: 30 particles, 15 RPM, 10000 steps at 10 microseconds (0.1 simulated seconds), finite states and species counts preserved. This short test does not establish long-term containment or mixing accuracy.
- Initial 30-particle benchmark: 5000 steps / 0.05 simulated seconds in 5.731 wall seconds, approximately 0.00872x. Not a controlled before/after speed comparison; no speedup guarantee.

See `boundary.json`, `initial.json`, `level.json`, and `smoke.json` for results. Full transport, long-duration mixer behavior and experimental mixing fidelity remain unvalidated. This is an additional comparison level, not a replacement for the reference simulation.

## Reproduction

Generate collision data with `python Scripts/build_plant_hybrid.py` (NumPy/SciPy required). `Scripts/setup_plant_hybrid.py` is a one-time Unreal Python level creator and refuses to overwrite an existing target. Build/run `plant_hybrid_tests` with reference directory, hybrid directory and report path. A changed machine model requires regeneration and renewed boundary/route tests.

## 2026-11-02 optimization update

Module 1072 adds exact 2D edge-BVH queries for clipped polygons. Shape order and geometry remain unchanged. See [optimization results](optimization/RESULTS.md) for paired pre/post kernel measurements and visual validation. This supersedes the build-version note above; original creation tests remain historical evidence.
