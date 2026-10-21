# Task 25 dilute two-way verification

Status: **PASS**

Nine CTest suites twice: conservative volume assignment, equal/opposite actual impulse injection, adjoint power identity, exchange decay reference, fixed-support grid and coupling-time refinement, off-node and overlapping stencils, weak one-way limit, dense/input rejection, atomic rollback and previous solver regressions. Two-way and matched one-way 0.01 s runs repeated byte-for-byte.

Volume fractions are diagnostics/guards (alpha<=0.001). Finite-porosity continuity, dense drag, self-disturbance correction, contacts and Unreal integration are not implemented here.

Source SHA-256: `ba6ef1b8c75a2418dae460029bbeb90536149ec29ad4369e3c55c4db67d90800`
