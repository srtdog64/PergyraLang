# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-17.

| Item | Observed value |
|---|---|
| Published main | fast-forwarded to the commit that carries this file; previous published head `a1cb26a93` (CI green) |
| Work branch | `integrate/track-a`, worktree `C:\Users\user\pgy-wt\ci-win`; parallel tracks land on it as squash commits, then main fast-forwards |
| Open tracks | W1 (weekly Linux gates), S3 (self-host alias closure), DM2 (direct MIR builtin calls and the role receiver ABI); each in its own worktree and WSL tree with its own `TMPDIR` |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main` builds the integration head |
| Last CI with Linux jobs executed | `a1cb26a93` green |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
