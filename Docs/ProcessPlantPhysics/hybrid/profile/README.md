# Hybrid collision profiling — 2026-11-02

Target: L_ProcessPlant_HybridCollisionTest data, RTX 4060 Ti, 30 particles (10/species), radius 2 mm, dt 10 us, 8 contact iterations. Standalone compute tests; no editor rendering. Supplier valves open, weighing outlet closed for trajectory tests. Existing level/assets/physics unchanged. Diagnostics only added.

## Stage measurements

Split profiling inserts separate wall kernels, so timings are diagnostic and slower than production fused kernels. Percentages below are fractions of summed GPU event durations, not percent of total application wall time or device compute occupancy.

| State | Wall projection + probe | Particle contact | Wall-clock throughput |
|---|---:|---:|---:|
| First 0.03 simulated seconds | 75.71% | 10.55% | 0.01337x (split) |
| 0.30–0.33 simulated seconds | 90.64% | 4.21% | 0.00488x (split) |

First-window fused/profiled run: 0.01876x. Unprofiled 0.30-second run: average 0.01401x. These are short compute-only tests, not 30-FPS sustained guarantees. CPU submission and synchronization costs overlap GPU work and must not be added as independent costs.

## Location probes

Each source mesh gets 30 area-quantile surface-centre probes, offset 1 mm toward its source normal. Full-scene boundary queries are timed at angles 0 and 0.35 rad. Five trials of 12 calls after warm-up; CSV stores median/min/max GPU kernel duration. Timings exclude diagnostic buffer allocation/upload/readback. These probes are synthetic contact workloads, not an assertion that real particles visit every surface. Name identifies probe location, not exclusive ownership of all measured work.

Representative angle-zero full-query times for 30 probes:

| Location | GPU us/query |
|---|---:|
| WH separate top cover | 1947.1 |
| WH inspection neck | 520.6 |
| P101B weighing flexible joint | 509.4 |
| P101C weighing flexible joint | 500.8 |
| WH inlet C / A | 447.8 / 447.2 |
| PL_C02 seal | 445.3 |
| Mixer vessel | 193.3 |
| WH cone | 190.6 |
| Mixer rotor | 142.0 |
| WH body ID700 | 108.1 |
| Supplier A wall | 93.8 |
| Actual initial positions | 39.4 |
| Empty space | 5.0 |

Top cover has 2898 wall patches; a clipped polygon has 2897 boundary edges. The kernel linearly scans all these edges whenever the patch OBB passes. This preserves holes but creates an expensive narrow phase. Diagnostic-only cover ablation (move shapes far away in private memory, preserve BVH) reduced cover-region query from 1947.1 to 444.3 us (77.2%), inspection-neck from 520.6 to 309.7 us, and B/C flexible-joint queries by about 45%. This supports a causal cover cost but is not a physically valid simulation or a forecast of total speedup.

After 0.30 seconds, the nearest source triangle centroids for all particles belonged to P101A/B/C_RigidRoute_ID44 (10 each). This nearest-centroid classification is approximate. Query at those actual positions took 162.8 us vs initial 39.4 us. Disabling only these pipes in private diagnostic memory reduced it to 126.7 us (22.2%). Thus original-pipe precise tests contribute, but cannot explain the whole boundary cost: BVH traversal, other candidates and repeated queries remain. The three original rigid routes contain 39168 triangles total.

## Recommendations

1. Give complex clipped patches an edge acceleration structure or decompose into hole-preserving smaller patches; prioritize WH top cover. Do not fill holes with a single box.
2. Measure and improve hybrid BVH locality/overlap, especially supply routes and crowded neck/seal areas. Current generator preserves old triangle order and splits by count; this may produce overlapping aggregate node bounds. Validate contact-order effects before spatial reordering.
3. Improve precise triangle-query cost on retained pipes and redundant boundary probes; preserve collision geometry and validate boundary cases.
4. Particle count reduction alone will not fix this workload. At only 30 particles, frequent kernel launches and eight boundary iterations per timestep also matter.

Build target: plant_wall_profile. Prepare workload with Scripts/prepare_plant_wall_profile.py. Run with data directory, probes text, output CSV; optional fourth argument is an ablation mask containing geometry-record index and shape type. Masked geometry is modified only in private process memory. Never use masks in production. later_probes.txt and pipe_mask.txt correspond to the saved evolve.bin and current packed geometry; regenerate after geometry changes.

GPU spot readings: 34–39 C, up to 93% activity in a sampled instant, about 38 W. These are spot readings, not continuous maximums; activity is not shader occupancy. No thermal issue observed in these short tests.
