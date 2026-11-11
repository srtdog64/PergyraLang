# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-11 21:40 KST.

| Item | Observed value |
|---|---|
| Published main | `5c72599e2`; CI run 43018533732 running |
| Work branch | `ci/all-green`, worktree `C:\Users\user\pgy-wt\ci-win`, fast-forwarded into main; head `c7a67bd3f` |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-tip` at the branch head; its `bin/pgy-self-driver` is a local build from 2026-11-11 07:12 |
| Required CI at `51fbdca00` | RED: build-linux core 6 of 55 (5, 6, 24, 32, 34, 43), contracts 2 of 11 (3, 4), backend compare shards 4, 7, 14, 16, fixed point cancelled at 60 minutes; codegen bootstrap, Windows, macOS, formal proofs, tsan, sanitizers PASS |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
