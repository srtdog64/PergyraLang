# Copy retention across store growth, rejected on 2026-11-11

Measured by the Claude root lane in WSL `Ubuntu-C-WSL`, both runs at once
with other jobs alongside. Input: the canonical driver,
`src/self_hosted/compiler/driver_bootstrap_main.pgy --native-pipeline
--emit-c`, source at `f3dd62ca1`. This record is not edited after it is
written. The arity probe measurement it follows is in
[the landing cost record](2026-11-11_landing_fixed_point_cost.md).

| Rule | Analyses | Semantic | Total | Stores | Copies |
|---|---|---|---|---|---|
| Reference (copy plan reset on every store wave) | 67 | 2,283.6 s | 2,395 s | 688 | 3,923 |
| Copy retention, then one derivation from no copies | 44 | 1,937.5 s | 2,068 s | 683 | 3,920 |

Peak resident memory was 2.69 GB for both. The emitted C differs (826,734
lines against 826,708), so copy retention selects a different landing
plan on this input although it matched on the arity probe. It is rejected:
it saves 14% and does not reproduce the reference plan. The experiment
source is not committed.

The same emission took 26 minutes on CI at `e875e9e90` and 40 minutes
here under load, so this machine ran about 1.5 times slower than the CI
runner during these measurements.
