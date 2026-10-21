# DEM baseline validation report

Status: **PASS**

UTC: 2026-10-21T14:24:27.006948+00:00; repeats: 2; core version: 0.21.0.
Source SHA-256: `b1968094773d8c3903bc5572a1fa0dc13c11968ba20d6b0ff36a73ed79efbda4`

## Verification

- Unit suites: 2; all passed: True.
- Unreal suites: 2; all passed: True.
- Matrix: 42 cases per repeat; deterministic metrics and final state identical: True.
- All analytic convergence groups passed: True.

## Time-step convergence

| Case | Metric | Error, 200 steps | Error, 400 steps | Error, 800 steps | Halving ratios |
|---|---|---:|---:|---:|---|
| pair_k1000 | velocity_error_m_s | 0.000694024 | 0.000347056 | 0.000173539 | 1.9997, 1.9999 |
| pair_k10000 | velocity_error_m_s | 0.000694024 | 0.000347056 | 0.000173539 | 1.9997, 1.9999 |
| pair_k100000 | velocity_error_m_s | 0.000694024 | 0.000347056 | 0.000173539 | 1.9997, 1.9999 |
| rig_floor_k400000 | position_error_m | 1.38876e-08 | 6.9429e-09 | 3.47123e-09 | 2.0003, 2.0001 |
| rig_floor_k4000000 | position_error_m | 1.38876e-08 | 6.9429e-09 | 3.47123e-09 | 2.0003, 2.0001 |

## Instrumented core cost

The short matrices cover 2e-5 physical seconds, NOT complete deposition. Wall time includes stepping and metric collection. No rendering, initialization or file I/O is timed. Min/max describe repeat variation, not confidence intervals.

| Case | Steps | Wall seconds median [min, max] | Steps/s | Wall s / physical s | Peak overlap/radius |
|---|---:|---:|---:|---:|---:|
| dense_n30_k400000_dt2e-7 | 100 | 0.0181174 [0.0180873, 0.0181474] | 5519.59 | 905.868 | 0.0005 |
| rig_initial_n30_k400000_dt2e-7 | 100 | 0.0552311 [0.0548063, 0.0556558] | 1810.68 | 2761.55 | 0 |
| dense_n30_k400000_dt1e-7 | 200 | 0.0353237 [0.0346875, 0.0359599] | 5663.76 | 1766.19 | 0.0005 |
| rig_initial_n30_k400000_dt1e-7 | 200 | 0.11323 [0.110701, 0.11576] | 1767.19 | 5661.52 | 0 |
| dense_n30_k4000000_dt2e-7 | 100 | 0.0183851 [0.0178652, 0.018905] | 5443.54 | 919.255 | 0.0005 |
| rig_initial_n30_k4000000_dt2e-7 | 100 | 0.0581978 [0.0571078, 0.0592879] | 1718.88 | 2909.89 | 0 |
| dense_n30_k4000000_dt1e-7 | 200 | 0.0353225 [0.0342353, 0.0364096] | 5667.49 | 1766.12 | 0.0005 |
| rig_initial_n30_k4000000_dt1e-7 | 200 | 0.112058 [0.111376, 0.11274] | 1784.86 | 5602.9 | 0 |
| dense_n100_k400000_dt2e-7 | 100 | 0.0900968 [0.089416, 0.0907775] | 1109.98 | 4504.84 | 0.0005 |
| rig_initial_n100_k400000_dt2e-7 | 100 | 0.217015 [0.216791, 0.217238] | 460.799 | 10850.7 | 0 |
| dense_n100_k400000_dt1e-7 | 200 | 0.183222 [0.182636, 0.183808] | 1091.58 | 9161.1 | 0.0005 |
| rig_initial_n100_k400000_dt1e-7 | 200 | 0.524785 [0.523432, 0.526138] | 381.111 | 26239.2 | 0 |
| dense_n100_k4000000_dt2e-7 | 100 | 0.10774 [0.107646, 0.107835] | 928.159 | 5387.01 | 0.0005 |
| rig_initial_n100_k4000000_dt2e-7 | 100 | 0.237484 [0.235329, 0.23964] | 421.115 | 11874.2 | 0 |
| dense_n100_k4000000_dt1e-7 | 200 | 0.175906 [0.171626, 0.180185] | 1137.65 | 8795.28 | 0.0005 |
| rig_initial_n100_k4000000_dt1e-7 | 200 | 0.420981 [0.417717, 0.424244] | 475.11 | 21049 | 0 |
| dense_n300_k400000_dt2e-7 | 100 | 0.329456 [0.326777, 0.332136] | 303.55 | 16472.8 | 0.0005 |
| rig_initial_n300_k400000_dt2e-7 | 100 | 0.712346 [0.708486, 0.716206] | 140.385 | 35617.3 | 0 |
| dense_n300_k400000_dt1e-7 | 200 | 0.675427 [0.673288, 0.677565] | 296.112 | 33771.3 | 0.0005 |
| rig_initial_n300_k400000_dt1e-7 | 200 | 1.38153 [1.35349, 1.40956] | 144.827 | 69076.3 | 0 |
| dense_n300_k4000000_dt2e-7 | 100 | 0.333454 [0.329825, 0.337084] | 299.927 | 16672.7 | 0.0005 |
| rig_initial_n300_k4000000_dt2e-7 | 100 | 0.679006 [0.672803, 0.68521] | 147.286 | 33950.3 | 0 |
| dense_n300_k4000000_dt1e-7 | 200 | 0.658492 [0.650982, 0.666002] | 303.764 | 32924.6 | 0.0005 |
| rig_initial_n300_k4000000_dt1e-7 | 200 | 1.36553 [1.35696, 1.3741] | 146.469 | 68276.5 | 0 |
| rig_drop_1p_1.5s | 750000 | 15.0811 [15.0218, 15.1403] | 49732 | 10.054 | 0.0349938 |

## Conservation / energy

| Case | Max mass error kg | Max momentum error kg m/s | Max relative energy error |
|---|---:|---:|---:|
| pair_k1000_s200 | 2.22045e-16 | 0 | 0.0019673545264167203 |
| pair_k1000_s400 | 2.22045e-16 | 0 | 0.0009827120071395079 |
| pair_k1000_s800 | 2.22045e-16 | 0 | 0.00049111486852337904 |
| pair_k10000_s200 | 2.22045e-16 | 0 | 0.0019673545264287107 |
| pair_k10000_s400 | 2.22045e-16 | 0 | 0.00098271200698230032 |
| pair_k10000_s800 | 2.22045e-16 | 0 | 0.00049111486846298291 |
| pair_k100000_s200 | 2.22045e-16 | 0 | 0.0019673545265086467 |
| pair_k100000_s400 | 2.22045e-16 | 0 | 0.00098271200736732567 |
| pair_k100000_s800 | 2.22045e-16 | 0 | 0.00049111486849273689 |
| rig_floor_k400000_s200 | 2.77556e-17 | N/A | 0.0019596437991070354 |
| rig_floor_k400000_s400 | 2.77556e-17 | N/A | 0.00098078453941708821 |
| rig_floor_k400000_s800 | 2.77556e-17 | N/A | 0.00049063292406582924 |
| rig_floor_k4000000_s200 | 2.77556e-17 | N/A | 0.0019596437991074756 |
| rig_floor_k4000000_s400 | 2.77556e-17 | N/A | 0.0009807845394171899 |
| rig_floor_k4000000_s800 | 2.77556e-17 | N/A | 0.00049063292406552436 |
| dense_n30_k400000_dt2e-7 | 0 | 6.3523631045321505e-19 | N/A |
| rig_initial_n30_k400000_dt2e-7 | 0 | 5.4210108624275222e-19 | N/A |
| dense_n30_k400000_dt1e-7 | 0 | 5.944602503245698e-19 | N/A |
| rig_initial_n30_k400000_dt1e-7 | 0 | 1.3010426069826053e-18 | N/A |
| dense_n30_k4000000_dt2e-7 | 0 | 5.5283680341832774e-18 | N/A |
| rig_initial_n30_k4000000_dt2e-7 | 0 | 5.4210108624275222e-19 | N/A |
| dense_n30_k4000000_dt1e-7 | 0 | 5.1141714192815136e-18 | N/A |
| rig_initial_n30_k4000000_dt1e-7 | 0 | 1.3010426069826053e-18 | N/A |
| dense_n100_k400000_dt2e-7 | 0 | 2.6775654174679124e-18 | N/A |
| rig_initial_n100_k400000_dt2e-7 | 0 | 4.7704895589362195e-18 | N/A |
| dense_n100_k400000_dt1e-7 | 0 | 3.0214448028883825e-18 | N/A |
| rig_initial_n100_k400000_dt1e-7 | 0 | 8.4567769453869346e-18 | N/A |
| dense_n100_k4000000_dt2e-7 | 0 | 2.229422266387156e-17 | N/A |
| rig_initial_n100_k4000000_dt2e-7 | 0 | 4.7704895589362195e-18 | N/A |
| dense_n100_k4000000_dt1e-7 | 0 | 2.551764779199052e-17 | N/A |
| rig_initial_n100_k4000000_dt1e-7 | 0 | 8.4567769453869346e-18 | N/A |
| dense_n300_k400000_dt2e-7 | 0 | 2.5168530801045204e-17 | N/A |
| rig_initial_n300_k400000_dt2e-7 | 0 | 5.7245874707234634e-17 | N/A |
| dense_n300_k400000_dt1e-7 | 0 | 2.928747717560372e-17 | N/A |
| rig_initial_n300_k400000_dt1e-7 | 0 | 5.6378512969246231e-17 | N/A |
| dense_n300_k4000000_dt2e-7 | 0 | 4.2498987199775785e-16 | N/A |
| rig_initial_n300_k4000000_dt2e-7 | 0 | 5.7245874707234634e-17 | N/A |
| dense_n300_k4000000_dt1e-7 | 0 | 3.6666616882520088e-16 | N/A |
| rig_initial_n300_k4000000_dt1e-7 | 0 | 5.6378512969246231e-17 | N/A |
| rig_drop_1p_1.5s | 1.11022e-16 | N/A | N/A |

## Deliberately rejected conditions

- reject_oversized_dt: dt exceeds multi-contact world limit; unchanged state: True.
- reject_hertz_deep_overlap: world Hertz wall overlap cap exceeded; unchanged state: True.

## Scope and limitations

- Pair tests: elastic frictionless 1 kg spheres, k=1e3..1e5 N/m, 200/400/800 steps over a fixed quarter-contact interval. Pair velocity is the first-order convergence metric; pair position has a special second-order cancellation in this initial condition.
- Collector-face tests: actual 53-box fixture, d=0.04 m, rho=2500 kg/m3, initial compression 1e-5 m, k=4e5/4e6 N/m; floor position is the first-order metric.
- Dense-contact matrix: 30/100/300 spheres with small initial overlaps, no gravity/damping/friction, k=4e5/4e6 N/m, dt=2e-7/1e-7 s. This isolates conservative pair-contact cost, not calibrated powder.
- Rig initial matrix: normal seeded default placements in the 53-box fixture; NO contact during this very short window. Stiffness dependence here is not a contact-response validation.
- Complete rig drop: one enlarged 0.2 m, 1 kg synthetic sphere for 1.5 physical seconds, including collector impact. This is not a 300-particle discharge experiment.
- Mass and momentum are independent conservation checks only where applicable. Wall impulses/gravity make isolated momentum conservation inapplicable; wall rows leave it blank. Energy is checked only in undamped analytic contact cases.
- Overlap is the maximum reported at solver force evaluations, not a continuous-time upper bound. Remaining untested dt/material/count combinations are not certified.
- Timing depends on machine/background load and includes benchmark instrumentation. Short-window wall/physical ratios are diagnostics, not guaranteed full-experiment completion times.
- Existing unit/Unreal tests cover additional Hertz, sliding/rolling friction, tracking, supply and output cases; the new stiffness/performance matrix is linear-contact only.
- No calibration to physical powder measurements, no CFD, and no universal realtime guarantee.
