# Gates

Owner: Claude root lane. Checked 2026-11-14. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Required CI at the published SHA | GREEN at `edbb4575b`, the folder reorganization (run 43583259892): all 33 jobs succeeded, no step skipped in the bootstrap, oracle, join or contracts jobs; before it GREEN at `cdb65d798` (run 43554588799) |
| Folder reorganization, branch CI | `d29fda1bb` (run 43578035229): 32 of 33 jobs green; contracts failed on two line caps that `360ff8cf0` fits |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `cdb65d798`, with the bootstrap policy corpus in the join job |
| Codegen bootstrap | PASS on CI at `cdb65d798` |
| Bootstrap installed-driver jobs, CLI mode | PASS on CI at `cdb65d798` |
| build-linux core shard | PASS on CI at `cdb65d798` |
| Contracts shard (identity probe in the shared probe build) | PASS on CI at `cdb65d798`; the shard step took 15.4 min of the 30 min job limit (at `ac0205b71` the job was cancelled at 30 min) |
| Backend compare (20 shards), sanitizers, native oracle | PASS on CI at `cdb65d798` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `cdb65d798` |

Job times at `cdb65d798`: seed part 30.0 min (limit 60), native oracle
17.6 min, build-linux 21.0 min, join 8.1 min, codegen bootstrap 18.3 min.
