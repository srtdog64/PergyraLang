# Gates

Owner: Claude root lane. Checked 2026-11-11 10:30 KST. Local results are from
the WSL tree `/root/cc-tip` on the driver built at the commit a line names; a
run without `wslpath` on `PATH` stands for CI's Linux runner.

| Gate | Last observed result |
|---|---|
| Driver fixed point (gen2 == gen3, 237,170 lines) | PASS on CI at `1acc31c5b` at minute 56; legacy, composite and nested intent, source-C execution and the public diagnostic gates also PASS; the job stopped at `self-host-array-index-type` |
| Codegen bootstrap | PASS on CI at `ec109c765` and `1acc31c5b` |
| build-linux core shard | CI at `1acc31c5b`: 6 of 55 (5, 6, 24, 32, 34, 39); 39 passes locally on the branch |
| build-linux contracts shard | CI at `1acc31c5b`: 4 of 11 (3, 4, 10, 11). On the branch, locally: 4's nominal gate, 10 and 11 pass; 3 and the arity-probe gates behind 4 exceed their 120 s probe builds |
| Backend compare | CI at `1acc31c5b`: 16 of 20 shards green |
| macOS, Windows | RED on CI at `ec109c765` (one smoke); the smoke passes locally at `62d939e38` |
| Formal proofs, tsan | PASS on CI at `ec109c765` |
| Sanitizers | PASS on CI at `1acc31c5b` |
| Required CI at the published SHA | RED at `ec109c765` and `1acc31c5b` |

A local PASS on a scratch tree is not CI evidence. The finish line is every
required job green on one published SHA.
