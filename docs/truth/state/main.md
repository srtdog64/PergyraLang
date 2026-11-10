# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-11 06:00 KST.

| Item | Observed value |
|---|---|
| Published main | `1acc31c5b` (2026-11-11 06:40 KST); CI run 42905138426 |
| Work branch | `ci/all-green`, worktree `C:\Users\user\pgy-wt\ci-win`, fast-forwarded into main |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-all-b` (tip driver, shards), `/root/cc-all` (older driver, reference timing), `/root/cc-land` (landing experiment), `/root/cc-dbg-1110` (scratch, never committed) |
| Required CI at `e875e9e90` | RED: build-linux core 18 of 55, contracts 8 of 11, backend compare, fixed point (gen2 == gen3 at minute 59, then legacy intent LLVM) |
| Required CI at `558f5a9cd` | RED: codegen bootstrap (`readonly_call_bad` lost its refusal; fixed in `1acc31c5b`), so build-linux, backend compare and the fixed point were skipped |

Green at `e6ca0e3f3`: classify, tsan, macOS, Windows, formal proofs, codegen
bootstrap, backend-compare toolchain, sanitizers, 6 backend-compare shards.

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
