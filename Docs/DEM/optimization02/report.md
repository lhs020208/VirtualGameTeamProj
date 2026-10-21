# Optimization 02 comparison

Same inputs, Release CPU solver, 3 alternating unprofiled repeats per path. Timings include solver checks and fixture measurement. NullRHI/renderer performance is separate.

| Case | Status | Reference rate | Cached rate | Speedup |
|---|---|---:|---:|---:|
| freefall_32 | passed | 0.026545956891491484 | 0.06446879645783082 | 2.4285730863404806 |
| packing_32 | passed | 0.015161379236734746 | 0.02351291952632913 | 1.5508430439731566 |
| discharge_32 | passed | 0.03855956874977136 | 0.07540007278618063 | 1.9554179476301254 |
| mixing_32 | reference_rejected | 0.01360458795682299 | 0.019931934437088992 | 1.4650891670035993 |
| mixing_gentle_32 | passed | 0.013605481975218166 | 0.019920707615405847 | 1.4641677267803233 |
| packing_128 | passed | 0.0020072433383106505 | 0.0037237679169093746 | 1.8551651640021867 |
| packing_256 | passed | 0.0006934847867121737 | 0.0015778261153574097 | 2.2752137402147166 |
| mm_packing_32 | passed | 0.00014588807175359373 | 0.0002108177991359698 | 1.4450653614233997 |
| mm_packing_128 | passed | 2.1121913679174213e-05 | 3.8995697019812014e-05 | 1.8462198838669148 |
| mm_packing_256 | passed | 6.988034249194367e-06 | 1.593830473496739e-05 | 2.2807994589901837 |
| capacity_1000 | unsupported | n/a | n/a | n/a |
| capacity_10000 | unsupported | n/a | n/a | n/a |
| capacity_100000 | unsupported | n/a | n/a | n/a |
| target_10kg | unsupported | n/a | n/a | n/a |

Large-scale target NOT achieved; unchanged 300-body cap. 30 RPM reference_rejected case is diagnostic only.
PASS: True
Source SHA256: 39870fec39ee66c9c235ea964ba1aacd8364850045c487021dcec596ec2cfeab
