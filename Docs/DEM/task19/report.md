# DEM baseline validation report

Status: **PASS**

UTC: 2026-10-21T13:31:12.643435+00:00; repeats: 2; core version: 0.19.0.
Source SHA-256: `cd079474357e3ca01e0ae868e389785474e9a445f2a7939d7703ecfeebcc8ef3`

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
| dense_n30_k400000_dt2e-7 | 100 | 0.0168276 [0.0165806, 0.0170745] | 5943.92 | 841.378 | 0.0005 |
| rig_initial_n30_k400000_dt2e-7 | 100 | 0.0545947 [0.0540378, 0.0551517] | 1831.87 | 2729.74 | 0 |
| dense_n30_k400000_dt1e-7 | 200 | 0.034693 [0.0339217, 0.0354642] | 5767.71 | 1734.65 | 0.0005 |
| rig_initial_n30_k400000_dt1e-7 | 200 | 0.107893 [0.107667, 0.108119] | 1853.69 | 5394.67 | 0 |
| dense_n30_k4000000_dt2e-7 | 100 | 0.0166797 [0.0164503, 0.016909] | 5996.46 | 833.983 | 0.0005 |
| rig_initial_n30_k4000000_dt2e-7 | 100 | 0.0536496 [0.0535338, 0.0537654] | 1863.96 | 2682.48 | 0 |
| dense_n30_k4000000_dt1e-7 | 200 | 0.0331249 [0.0328968, 0.033353] | 6038.04 | 1656.24 | 0.0005 |
| rig_initial_n30_k4000000_dt1e-7 | 200 | 0.107219 [0.105321, 0.109117] | 1865.92 | 5360.97 | 0 |
| dense_n100_k400000_dt2e-7 | 100 | 0.0846598 [0.0830265, 0.0862931] | 1181.64 | 4232.99 | 0.0005 |
| rig_initial_n100_k400000_dt2e-7 | 100 | 0.199045 [0.195644, 0.202445] | 502.547 | 9952.23 | 0 |
| dense_n100_k400000_dt1e-7 | 200 | 0.166749 [0.163023, 0.170475] | 1200.01 | 8337.45 | 0.0005 |
| rig_initial_n100_k400000_dt1e-7 | 200 | 0.465277 [0.456704, 0.47385] | 429.997 | 23263.9 | 0 |
| dense_n100_k4000000_dt2e-7 | 100 | 0.0823366 [0.0810964, 0.0835767] | 1214.8 | 4116.83 | 0.0005 |
| rig_initial_n100_k4000000_dt2e-7 | 100 | 0.193493 [0.192785, 0.1942] | 516.822 | 9674.64 | 0 |
| dense_n100_k4000000_dt1e-7 | 200 | 0.161478 [0.161263, 0.161694] | 1238.56 | 8073.92 | 0.0005 |
| rig_initial_n100_k4000000_dt1e-7 | 200 | 0.391829 [0.390161, 0.393498] | 510.436 | 19591.5 | 0 |
| dense_n300_k400000_dt2e-7 | 100 | 0.320518 [0.31914, 0.321895] | 312.001 | 16025.9 | 0.0005 |
| rig_initial_n300_k400000_dt2e-7 | 100 | 0.638869 [0.630858, 0.64688] | 156.551 | 31943.5 | 0 |
| dense_n300_k400000_dt1e-7 | 200 | 0.629473 [0.627215, 0.631731] | 317.73 | 31473.6 | 0.0005 |
| rig_initial_n300_k400000_dt1e-7 | 200 | 1.31827 [1.31436, 1.32219] | 151.715 | 65913.7 | 0 |
| dense_n300_k4000000_dt2e-7 | 100 | 0.310633 [0.308917, 0.31235] | 321.933 | 15531.7 | 0.0005 |
| rig_initial_n300_k4000000_dt2e-7 | 100 | 0.639884 [0.63468, 0.645089] | 156.289 | 31994.2 | 0 |
| dense_n300_k4000000_dt1e-7 | 200 | 0.62275 [0.616959, 0.62854] | 321.184 | 31137.5 | 0.0005 |
| rig_initial_n300_k4000000_dt1e-7 | 200 | 1.2738 [1.26783, 1.27976] | 157.015 | 63689.8 | 0 |
| rig_drop_1p_1.5s | 750000 | 14.0054 [13.9803, 14.0305] | 53550.9 | 9.33695 | 0.0349938 |

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
