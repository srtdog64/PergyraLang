# Canonical driver landing rounds, measured on 2026-11-11

Measured by the Claude root lane in WSL `Ubuntu-C-WSL`, at `f1d4a0232` with
no source changes (`bin-dbg/pgy`, sha256 prefix `e165208d25a3`), with
`PGY_DEBUG_LANDING_ROUNDS`, `PGY_DEBUG_SEMANTIC_TIMING` and
`PGY_DEBUG_PIPELINE_TIMING` set and two other jobs running alongside. This
record is not edited after it is written.

Input: `src/self_hosted/compiler/driver_bootstrap_main.pgy --native-pipeline
--emit-c`, the fixed point job's native oracle emission.

## Result

- Exit 0, 1,963 s wall, 2.69 GB peak resident memory, 826,843 lines of C.
- Pipeline: semantic 1,844 s; HIR 16 s; MIR lowering 27 s; MIR validation
  8 s; backend 55 s.
- 67 analyses; type checking 1,626 s in total, 9.4 s minimum, 48.9 s
  maximum, 24.3 s on average.
- Final plan: 688 stores, 3,925 copies.

## Rounds

Stores grew in rounds 0 to 5, 7, 9, 11, 13, 15, 17, 19, 22, 27, 33, 39, 41
and 49 (19 store waves). Each store wave empties the copy plan; the copies
then come back over the following rounds in nearly the same steps each
time: 864, then about 600, 1, 94, 740 and about 1,000 more. Rounds 50 to
57 rebuilt 3,893 copies after the last store wave. Rounds 58 to 65 each
added one copy (one added 25), and round 66 added nothing.

## Comparison

CI at `51fbdca00` ran the same emission in 28 minutes inside the fixed
point job, which was cancelled at its 60 minute limit while compiling gen2.
At `1acc31c5b` the emission took 24 minutes and the job reached gen2 ==
gen3 at minute 56. The copy retention trial on the same input is in
[its record](2026-11-11_landing_copy_retention_rejected.md).
