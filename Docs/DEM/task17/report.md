# DEM baseline validation report

Status: **PASS**

UTC: 2026-10-21T12:50:49.758405+00:00; repeats: 2; core version: 0.17.0.
Source SHA-256: `d6c6c5a6aff1201fb34b307082035575abc39bf67f3a69b98a222b2393115ea2`

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
| dense_n30_k400000_dt2e-7 | 100 | 0.0165457 [0.016543, 0.0165484] | 6043.87 | 827.285 | 0.0005 |
| rig_initial_n30_k400000_dt2e-7 | 100 | 0.0526209 [0.0526075, 0.0526344] | 1900.38 | 2631.05 | 0 |
| dense_n30_k400000_dt1e-7 | 200 | 0.0333547 [0.0327469, 0.0339625] | 5998.15 | 1667.74 | 0.0005 |
| rig_initial_n30_k400000_dt1e-7 | 200 | 0.108767 [0.106881, 0.110653] | 1839.35 | 5438.34 | 0 |
| dense_n30_k4000000_dt2e-7 | 100 | 0.0171689 [0.0162868, 0.018051] | 5839.9 | 858.445 | 0.0005 |
| rig_initial_n30_k4000000_dt2e-7 | 100 | 0.0537408 [0.0535469, 0.0539348] | 1860.81 | 2687.04 | 0 |
| dense_n30_k4000000_dt1e-7 | 200 | 0.0337808 [0.0333543, 0.0342073] | 5921.47 | 1689.04 | 0.0005 |
| rig_initial_n30_k4000000_dt1e-7 | 200 | 0.11193 [0.108298, 0.115562] | 1788.71 | 5596.51 | 0 |
| dense_n100_k400000_dt2e-7 | 100 | 0.0840362 [0.0839742, 0.0840981] | 1189.96 | 4201.81 | 0.0005 |
| rig_initial_n100_k400000_dt2e-7 | 100 | 0.205936 [0.205921, 0.205951] | 485.588 | 10296.8 | 0 |
| dense_n100_k400000_dt1e-7 | 200 | 0.167231 [0.166266, 0.168196] | 1195.99 | 8361.56 | 0.0005 |
| rig_initial_n100_k400000_dt1e-7 | 200 | 0.397763 [0.393406, 0.402121] | 502.872 | 19888.2 | 0 |
| dense_n100_k4000000_dt2e-7 | 100 | 0.0824841 [0.0818259, 0.0831423] | 1212.43 | 4124.2 | 0.0005 |
| rig_initial_n100_k4000000_dt2e-7 | 100 | 0.197877 [0.194501, 0.201253] | 505.511 | 9893.85 | 0 |
| dense_n100_k4000000_dt1e-7 | 200 | 0.168389 [0.166681, 0.170096] | 1187.85 | 8419.43 | 0.0005 |
| rig_initial_n100_k4000000_dt1e-7 | 200 | 0.400323 [0.390722, 0.409924] | 499.884 | 20016.1 | 0 |
| dense_n300_k400000_dt2e-7 | 100 | 0.310592 [0.30805, 0.313134] | 321.988 | 15529.6 | 0.0005 |
| rig_initial_n300_k400000_dt2e-7 | 100 | 0.646289 [0.630347, 0.662231] | 154.824 | 32314.5 | 0 |
| dense_n300_k400000_dt1e-7 | 200 | 0.617793 [0.607015, 0.62857] | 323.832 | 30889.6 | 0.0005 |
| rig_initial_n300_k400000_dt1e-7 | 200 | 1.2803 [1.24766, 1.31293] | 156.315 | 64014.8 | 0 |
| dense_n300_k4000000_dt2e-7 | 100 | 0.308532 [0.305232, 0.311833] | 324.152 | 15426.6 | 0.0005 |
| rig_initial_n300_k4000000_dt2e-7 | 100 | 0.634432 [0.624031, 0.644833] | 157.664 | 31721.6 | 0 |
| dense_n300_k4000000_dt1e-7 | 200 | 0.614339 [0.607392, 0.621286] | 325.595 | 30716.9 | 0.0005 |
| rig_initial_n300_k4000000_dt1e-7 | 200 | 1.27054 [1.25575, 1.28533] | 157.435 | 63527 | 0 |
| rig_drop_1p_1.5s | 750000 | 14.0619 [13.6564, 14.4674] | 53380 | 9.3746 | 0.0349938 |

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
