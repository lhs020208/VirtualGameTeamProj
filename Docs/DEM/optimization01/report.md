# Optimization 01 baseline

Status: PASS

This validates instrumentation/reference fixtures, NOT sustained 0.1x or large-scale support. 30 RPM mixing is retained as a reference-rejected diagnostic; do not use it as a validated accurate baseline.

| Case | Status | Unprofiled loop rate | Minimum short window rate | Largest measured phase |
|---|---|---:|---:|---|
| freefall_32 | passed | 0.026260406897651377 | 0.022195932373436193 | validation_prepare |
| packing_32 | passed | 0.015215342740342181 | 0.011263151136847654 | validation_prepare |
| discharge_32 | passed | 0.0390203456724062 | 0.017894337515838864 | validation_prepare |
| mixing_32 | reference_rejected | 0.01312439664008963 | 0.010515722582618727 | validation_prepare |
| mixing_gentle_32 | passed | 0.013285691514635746 | 0.008582902686192193 | validation_prepare |
| packing_128 | passed | 0.001985148694954196 | 0.0018359959865127964 | validation_prepare |
| packing_256 | passed | 0.0006919664454499833 | 0.0006779311710040869 | validation_prepare |
| mm_packing_32 | passed | 0.00014659767696656282 | 0.00012550988390337743 | validation_prepare |
| mm_packing_128 | passed | 2.108185121711393e-05 | 1.9863636137913233e-05 | validation_prepare |
| mm_packing_256 | passed | 6.969027223220412e-06 | 6.8210730229971835e-06 | validation_prepare |
| capacity_1000 | unsupported | n/a | n/a | n/a |
| capacity_10000 | unsupported | n/a | n/a | n/a |
| capacity_100000 | unsupported | n/a | n/a | n/a |
| target_10kg | unsupported | n/a | n/a | n/a |

All large requests are unsupported (300-body limit unchanged). Phase clocks are exclusive broad groups; query/force internals are not separately timed. CPU display synchronization and GPU drawing are outside this standalone runner. Source hash: 18bcb3aa68207a18241e41d3406aa1d689d2f497e634e0bbfe38d3d4051532d4
