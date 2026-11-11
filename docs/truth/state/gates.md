# Gates

Owner: Claude root lane. Checked 2026-11-11 21:40 KST. Local results are from
the WSL tree `/root/cc-tip` on the driver built at the commit a line names; a
run without `wslpath` on `PATH` stands for CI's Linux runner.

| Gate | Last observed result |
|---|---|
| Driver fixed point (gen2 == gen3, 237,170 lines) | PASS on CI at `1acc31c5b` at minute 56; at `51fbdca00` the job was cancelled at 60 minutes while compiling gen2 (oracle emit 28 minutes) |
| Codegen bootstrap | PASS on CI at `51fbdca00` (cancelled at 30 minutes at `e4a7ca842`) |
| build-linux core shard | CI at `51fbdca00`: 6 of 55 (5, 6, 24, 32, 34, 43); 32, 34 and 43 pass locally on the branch |
| build-linux contracts shard | CI at `51fbdca00`: 2 of 11 (3, 4), both probe builds over 120 s |
| Backend compare | CI at `51fbdca00`: 16 of 20 shards green; 14 and 16 pass locally at `c7a67bd3f` |
| macOS, Windows | PASS on CI at `51fbdca00` |
| Formal proofs, tsan, sanitizers | PASS on CI at `51fbdca00` |
| Required CI at the published SHA | RED at `51fbdca00` |

A local PASS on a scratch tree is not CI evidence. The finish line is every
required job green on one published SHA.
