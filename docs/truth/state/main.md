# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-11 10:30 KST.

| Item | Observed value |
|---|---|
| Published main | `ec109c765`; CI run 42928023799 |
| Work branch | `ci/all-green`, worktree `C:\Users\user\pgy-wt\ci-win`, fast-forwarded into main |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-tip` (branch head; its `bin/pgy-self-driver` is an earlier local build), `/root/cc-all` (older driver, reference timing) |
| Required CI at `ec109c765` | macOS and Windows RED on one smoke (`mir_lower_stmt_render_fail_closed_smoke.sh`, fixed in `62d939e38`); codegen bootstrap, formal proofs, tsan PASS; Linux jobs had not started when this was checked |
| Required CI at `1acc31c5b` | RED: build-linux core 6 of 55, contracts 4 of 11, 4 backend-compare shards, fixed point (after gen2 == gen3) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
