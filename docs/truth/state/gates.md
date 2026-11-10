# Gates

Owner: Claude root lane. Checked 2026-11-11 09:00 KST. Local results are from
the WSL tree `/root/cc-tip` on the driver built at the commit a line names; a
run without `wslpath` on `PATH` stands for CI's Linux runner.

| Gate | Last observed result |
|---|---|
| Driver fixed point (gen2 == gen3, 237,170 lines) | PASS on CI at `1acc31c5b` at minute 56; legacy, composite and nested intent, source-C execution and the public diagnostic gates also PASS; the job stopped at `self-host-array-index-type` |
| Codegen bootstrap | PASS on CI at `1acc31c5b` (failed at `558f5a9cd`, fixed) |
| build-linux core shard | CI at `1acc31c5b`: 6 of 55 (5, 6, 24, 32, 34, 39); 39 passes locally on the branch |
| build-linux contracts shard | CI at `1acc31c5b`: 4 of 11 (3, 4, 10, 11); step 11's sub-gates pass locally on the branch |
| Backend compare | CI at `1acc31c5b`: 16 of 20 shards green |
| macOS, Windows, formal proofs, tsan, sanitizers | PASS on CI at `1acc31c5b` |
| Required CI at the published SHA | RED at `1acc31c5b` |

A local PASS on a scratch tree is not CI evidence. The finish line is every
required job green on one published SHA.
