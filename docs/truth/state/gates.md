# Gates

Owner: Claude root lane. Checked 2026-11-11 06:00 KST. Local results are from
the WSL trees named in [main](main.md), on the driver built at the commit a
line names; a run without `wslpath` on `PATH` stands for CI's Linux runner.

| Gate | Last observed result |
|---|---|
| Native compile of the canonical driver | PASS at `e6ca0e3f3`: `0 error(s), 26 warning(s)` |
| Codegen bootstrap (gen2 == gen3, 119,545 lines) | PASS on CI at `e6ca0e3f3` |
| Driver fixed point (gen2 == gen3, 237,109 lines) | PASS on CI at `e875e9e90` at minute 59; the job then failed at legacy intent LLVM |
| build-linux core shard, local | RED at `7d9f224c8` driver: 6 of the first 42 steps (5, 6, 24, 32, 34, 39); CI at `e875e9e90`: 18 of 55 |
| build-linux contracts shard, local | RED at `7d9f224c8` driver: 5 of 11 (3, 4, 7, 10, 11); CI at `e875e9e90`: 8 of 11 |
| Zone sync, identity cell probe compile, enum inout negatives, struct arity, inout subject payload | PASS locally after the branch fixes |
| reflect_type_name, native C and LLVM | PASS at `6c3a01c91`, C and LLVM print the same six lines |
| Required CI at the published SHA | RED at `e875e9e90` |

A local PASS on a scratch tree is not CI evidence. The finish line is every
required job green on one published SHA.
