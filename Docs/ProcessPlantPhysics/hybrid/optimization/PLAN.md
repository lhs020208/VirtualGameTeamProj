# Bounded hybrid optimization plan

Target: /Game/ProcessPlant_20261029/L_ProcessPlant_HybridCollisionTest. Branch PowderSimulationOptimization. Preserve all existing changes. 30 particles, radius 2 mm, dt 10 us, eight iterations; no rate cap; no wall deletion; no commits.

1. Exact polygon-edge BVH: preserve shape, holes, nearest-edge tie order and parity. Acceptance: <=1 um position difference, <=1e-4 normal/velocity difference on static/rotated/shell probes; performance comparison at cover and actual positions.
2. Spatial median shape BVH experiment: compare same probes and checkpoint trajectories. Reject if contact order changes exceed thresholds; never silently replace physical behavior for speed.
3. Evaluate conservative wall candidate reuse against accepted version. Require displacement and rotor/gate invalidation, no missed contacts and bounded fallback. If those conditions cannot be established in this bounded task, record deferral rather than adopt unsafe cache. Prefer exact query-local reuse if measured and validated.
4. Final: repeated fused and split GPU benchmarks, short 30-particle trajectories, valve and mixer regressions, Unreal build and visual smoke in existing editor (no second interactive editor). Report throughput separately from GPU percentages and distinguish numerical equivalence from real-world accuracy.

Baseline packed data and code preserved under Intermediate/HybridOptimization. Reports in this directory. Geometry remains provisional.
