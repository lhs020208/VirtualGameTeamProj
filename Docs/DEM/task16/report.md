# DEM baseline validation report

Status: **PASS**

UTC: 2026-10-21T12:40:15.391935+00:00; repeats: 2; core version: 0.16.0.
Source SHA-256: `c6898cf00bb4b17fc60b7eb1329695ff7bfb5fabea231e666e4ede19387d3843`

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
| dense_n30_k400000_dt2e-7 | 100 | 0.0162711 [0.0162155, 0.0163267] | 6145.94 | 813.555 | 0.0005 |
| rig_initial_n30_k400000_dt2e-7 | 100 | 0.0550159 [0.053651, 0.0563809] | 1818.77 | 2750.8 | 0 |
| dense_n30_k400000_dt1e-7 | 200 | 0.03357 [0.0333552, 0.0337847] | 5957.95 | 1678.5 | 0.0005 |
| rig_initial_n30_k400000_dt1e-7 | 200 | 0.10979 [0.109639, 0.109941] | 1821.66 | 5489.5 | 0 |
| dense_n30_k4000000_dt2e-7 | 100 | 0.0166287 [0.0163763, 0.0168811] | 6015.09 | 831.435 | 0.0005 |
| rig_initial_n30_k4000000_dt2e-7 | 100 | 0.0540349 [0.0535535, 0.0545164] | 1850.8 | 2701.75 | 0 |
| dense_n30_k4000000_dt1e-7 | 200 | 0.0331264 [0.0330116, 0.0332412] | 6037.55 | 1656.32 | 0.0005 |
| rig_initial_n30_k4000000_dt1e-7 | 200 | 0.108236 [0.107823, 0.10865] | 1847.84 | 5411.82 | 0 |
| dense_n100_k400000_dt2e-7 | 100 | 0.0838567 [0.0827796, 0.0849338] | 1192.71 | 4192.84 | 0.0005 |
| rig_initial_n100_k400000_dt2e-7 | 100 | 0.201189 [0.199669, 0.202708] | 497.074 | 10059.4 | 0 |
| dense_n100_k400000_dt1e-7 | 200 | 0.167376 [0.166839, 0.167914] | 1194.92 | 8368.83 | 0.0005 |
| rig_initial_n100_k400000_dt1e-7 | 200 | 0.404445 [0.402895, 0.405995] | 494.512 | 20222.3 | 0 |
| dense_n100_k4000000_dt2e-7 | 100 | 0.0842501 [0.0829705, 0.0855297] | 1187.22 | 4212.51 | 0.0005 |
| rig_initial_n100_k4000000_dt2e-7 | 100 | 0.197616 [0.196691, 0.198541] | 506.043 | 9880.8 | 0 |
| dense_n100_k4000000_dt1e-7 | 200 | 0.165504 [0.162206, 0.168802] | 1208.91 | 8275.2 | 0.0005 |
| rig_initial_n100_k4000000_dt1e-7 | 200 | 0.403064 [0.399219, 0.406909] | 496.244 | 20153.2 | 0 |
| dense_n300_k400000_dt2e-7 | 100 | 0.309876 [0.308038, 0.311714] | 322.721 | 15493.8 | 0.0005 |
| rig_initial_n300_k400000_dt2e-7 | 100 | 0.643143 [0.639486, 0.646801] | 155.491 | 32157.2 | 0 |
| dense_n300_k400000_dt1e-7 | 200 | 0.619126 [0.616524, 0.621729] | 323.041 | 30956.3 | 0.0005 |
| rig_initial_n300_k400000_dt1e-7 | 200 | 1.27578 [1.27241, 1.27914] | 156.769 | 63788.8 | 0 |
| dense_n300_k4000000_dt2e-7 | 100 | 0.308507 [0.307016, 0.309998] | 324.149 | 15425.4 | 0.0005 |
| rig_initial_n300_k4000000_dt2e-7 | 100 | 0.636721 [0.636582, 0.636861] | 157.055 | 31836.1 | 0 |
| dense_n300_k4000000_dt1e-7 | 200 | 0.61727 [0.614526, 0.620015] | 324.014 | 30863.5 | 0.0005 |
| rig_initial_n300_k4000000_dt1e-7 | 200 | 1.27656 [1.26731, 1.2858] | 156.68 | 63827.8 | 0 |
| rig_drop_1p_1.5s | 750000 | 14.0727 [13.9106, 14.2348] | 53301.8 | 9.38179 | 0.0349938 |

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
