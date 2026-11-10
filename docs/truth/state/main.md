# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-10 23:30 KST.

| Item | Observed value |
|---|---|
| Published main | `e875e9e90` |
| Work branch | `ci/all-green`, worktree `C:\Users\user\pgy-wt\ci-win`; ahead of main by the commits after `e875e9e90` |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-all` (bootstrap), `/root/cc-all-b` (shards), `/root/cc-dbg-1110` (scratch, never committed) |
| Required CI at `e6ca0e3f3` | RED: build-linux, contracts shard, 14 backend-compare shards, fixed point (60-minute limit) |
| Required CI at `e875e9e90` | running when this was written (run 42832502605) |

Green at `e6ca0e3f3`: classify, tsan, macOS, Windows, formal proofs, codegen
bootstrap, backend-compare toolchain, sanitizers, 6 backend-compare shards.

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
