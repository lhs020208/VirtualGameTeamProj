# DEM task 15 validation report

Status: **PASS**

UTC: 2026-10-21T12:25:55.474256+00:00; repeats: 2; core version: 0.14.0.
Source SHA-256: `373296f982a3949704588ab88f2044f01bb9f72413c6bfbd599670a9de711a32`

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
| dense_n30_k400000_dt2e-7 | 100 | 0.017195 [0.0168162, 0.0175737] | 5818.48 | 859.748 | 0.0005 |
| rig_initial_n30_k400000_dt2e-7 | 100 | 0.0570223 [0.0548975, 0.0591472] | 1756.14 | 2851.12 | 0 |
| dense_n30_k400000_dt1e-7 | 200 | 0.0341694 [0.0336475, 0.0346913] | 5854.56 | 1708.47 | 0.0005 |
| rig_initial_n30_k400000_dt1e-7 | 200 | 0.111115 [0.110212, 0.112018] | 1800.06 | 5555.74 | 0 |
| dense_n30_k4000000_dt2e-7 | 100 | 0.0172245 [0.0171015, 0.0173475] | 5805.98 | 861.225 | 0.0005 |
| rig_initial_n30_k4000000_dt2e-7 | 100 | 0.058408 [0.0573263, 0.0594896] | 1712.68 | 2920.4 | 0 |
| dense_n30_k4000000_dt1e-7 | 200 | 0.0361883 [0.0349924, 0.0373842] | 5532.69 | 1809.41 | 0.0005 |
| rig_initial_n30_k4000000_dt1e-7 | 200 | 0.117133 [0.115187, 0.119078] | 1707.94 | 5856.63 | 0 |
| dense_n100_k400000_dt2e-7 | 100 | 0.0914747 [0.0911828, 0.0917667] | 1093.21 | 4573.74 | 0.0005 |
| rig_initial_n100_k400000_dt2e-7 | 100 | 0.22891 [0.213939, 0.24388] | 438.73 | 11445.5 | 0 |
| dense_n100_k400000_dt1e-7 | 200 | 0.208506 [0.207934, 0.209079] | 959.211 | 10425.3 | 0.0005 |
| rig_initial_n100_k400000_dt1e-7 | 200 | 0.472187 [0.457505, 0.486868] | 423.971 | 23609.3 | 0 |
| dense_n100_k4000000_dt2e-7 | 100 | 0.0885404 [0.0874011, 0.0896798] | 1129.61 | 4427.02 | 0.0005 |
| rig_initial_n100_k4000000_dt2e-7 | 100 | 0.208929 [0.205934, 0.211923] | 478.73 | 10446.4 | 0 |
| dense_n100_k4000000_dt1e-7 | 200 | 0.176235 [0.174624, 0.177845] | 1134.95 | 8811.74 | 0.0005 |
| rig_initial_n100_k4000000_dt1e-7 | 200 | 0.415494 [0.410033, 0.420955] | 481.438 | 20774.7 | 0 |
| dense_n300_k400000_dt2e-7 | 100 | 0.328069 [0.324622, 0.331516] | 304.847 | 16403.5 | 0.0005 |
| rig_initial_n300_k400000_dt2e-7 | 100 | 0.674875 [0.674441, 0.675309] | 148.176 | 33743.8 | 0 |
| dense_n300_k400000_dt1e-7 | 200 | 0.679509 [0.672582, 0.686436] | 294.361 | 33975.5 | 0.0005 |
| rig_initial_n300_k400000_dt1e-7 | 200 | 1.3523 [1.34388, 1.36072] | 147.902 | 67615.1 | 0 |
| dense_n300_k4000000_dt2e-7 | 100 | 0.331309 [0.330882, 0.331736] | 301.834 | 16565.4 | 0.0005 |
| rig_initial_n300_k4000000_dt2e-7 | 100 | 0.712821 [0.672245, 0.753396] | 140.744 | 35641 | 0 |
| dense_n300_k4000000_dt1e-7 | 200 | 0.66574 [0.659255, 0.672225] | 300.446 | 33287 | 0.0005 |
| rig_initial_n300_k4000000_dt1e-7 | 200 | 1.34497 [1.33868, 1.35125] | 148.706 | 67248.3 | 0 |
| rig_drop_1p_1.5s | 750000 | 14.9907 [14.8301, 15.1512] | 50036.9 | 9.99378 | 0.0349938 |

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
- Existing unit/Unreal tests cover additional Hertz, friction, tracking, supply and output cases; the new stiffness/performance matrix is linear-contact only.
- No calibration to physical powder measurements, no CFD, and no universal realtime guarantee.
