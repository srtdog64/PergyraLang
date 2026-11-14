# Gates

Owner: Claude root lane. Checked 2026-11-14. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `ac0205b71` (run 43541429551), with the bootstrap policy corpus in the join job |
| Codegen bootstrap | PASS on CI at `ac0205b71` |
| Bootstrap installed-driver jobs, CLI mode | PASS on CI at `ac0205b71` |
| build-linux core shard | PASS on CI at `59e40856b` and `ac0205b71` |
| Backend compare (20 shards), sanitizers, native oracle | PASS on CI at `ac0205b71` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `ac0205b71` |
| Contracts step 3 (identity probe) | CI at `ac0205b71`: the probe build on the public route timed out at 120 s |
| Contracts step 4 | PASS locally at `ac0205b71`; on CI the job hit its 30 minute limit during step 11, after step 4 passed |
| Contracts job time | CI at `ac0205b71`: step 4 took 19.6 min, 572 s of it the shared semantic owner probe build (native, C and LLVM in parallel; 463 s at `d78654846`) |
| Required CI at the published SHA | RED: only build-linux-self-host-contracts |

The finish line is every required job green on one published SHA.
