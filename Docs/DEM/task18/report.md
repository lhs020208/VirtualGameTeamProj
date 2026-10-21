# DEM baseline validation report

Status: **PASS**

UTC: 2026-10-21T13:06:10.310314+00:00; repeats: 2; core version: 0.18.0.
Source SHA-256: `f7f29676a8c14e1e9a5cd8b99d8127245cfec353b377aec91b5ef97ce27dbf42`

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
| dense_n30_k400000_dt2e-7 | 100 | 0.0163449 [0.0162286, 0.0164612] | 6118.43 | 817.245 | 0.0005 |
| rig_initial_n30_k400000_dt2e-7 | 100 | 0.0540306 [0.0537809, 0.0542803] | 1850.84 | 2701.53 | 0 |
| dense_n30_k400000_dt1e-7 | 200 | 0.0332812 [0.03213, 0.0344324] | 6016.6 | 1664.06 | 0.0005 |
| rig_initial_n30_k400000_dt1e-7 | 200 | 0.10695 [0.106611, 0.107288] | 1870.06 | 5347.48 | 0 |
| dense_n30_k4000000_dt2e-7 | 100 | 0.0162181 [0.0159309, 0.0165052] | 6167.9 | 810.903 | 0.0005 |
| rig_initial_n30_k4000000_dt2e-7 | 100 | 0.054049 [0.0523973, 0.0557007] | 1851.9 | 2702.45 | 0 |
| dense_n30_k4000000_dt1e-7 | 200 | 0.0329973 [0.0328503, 0.0331444] | 6061.21 | 1649.87 | 0.0005 |
| rig_initial_n30_k4000000_dt1e-7 | 200 | 0.108272 [0.107107, 0.109437] | 1847.42 | 5413.59 | 0 |
| dense_n100_k400000_dt2e-7 | 100 | 0.0835528 [0.0826176, 0.0844879] | 1197 | 4177.64 | 0.0005 |
| rig_initial_n100_k400000_dt2e-7 | 100 | 0.213901 [0.204826, 0.222976] | 468.349 | 10695.1 | 0 |
| dense_n100_k400000_dt1e-7 | 200 | 0.187333 [0.173857, 0.20081] | 1073.17 | 9366.67 | 0.0005 |
| rig_initial_n100_k400000_dt1e-7 | 200 | 0.394608 [0.39351, 0.395706] | 506.836 | 19730.4 | 0 |
| dense_n100_k4000000_dt2e-7 | 100 | 0.0821508 [0.0820841, 0.0822176] | 1217.27 | 4107.54 | 0.0005 |
| rig_initial_n100_k4000000_dt2e-7 | 100 | 0.198218 [0.195164, 0.201273] | 504.614 | 9910.91 | 0 |
| dense_n100_k4000000_dt1e-7 | 200 | 0.163358 [0.162687, 0.164029] | 1224.33 | 8167.88 | 0.0005 |
| rig_initial_n100_k4000000_dt1e-7 | 200 | 0.39826 [0.391688, 0.404831] | 502.322 | 19913 | 0 |
| dense_n300_k400000_dt2e-7 | 100 | 0.305955 [0.304764, 0.307146] | 326.851 | 15297.7 | 0.0005 |
| rig_initial_n300_k400000_dt2e-7 | 100 | 0.628161 [0.620311, 0.636012] | 159.22 | 31408.1 | 0 |
| dense_n300_k400000_dt1e-7 | 200 | 0.604829 [0.603473, 0.606184] | 330.674 | 30241.4 | 0.0005 |
| rig_initial_n300_k400000_dt1e-7 | 200 | 1.26631 [1.24905, 1.28356] | 157.969 | 63315.4 | 0 |
| dense_n300_k4000000_dt2e-7 | 100 | 0.310781 [0.31035, 0.311212] | 321.771 | 15539.1 | 0.0005 |
| rig_initial_n300_k4000000_dt2e-7 | 100 | 0.636223 [0.63591, 0.636536] | 157.178 | 31811.1 | 0 |
| dense_n300_k4000000_dt1e-7 | 200 | 0.612059 [0.607035, 0.617084] | 326.788 | 30603 | 0.0005 |
| rig_initial_n300_k4000000_dt1e-7 | 200 | 1.27465 [1.27119, 1.2781] | 156.908 | 63732.3 | 0 |
| rig_drop_1p_1.5s | 750000 | 14.2009 [14.039, 14.3628] | 52820.4 | 9.46727 | 0.0349938 |

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
