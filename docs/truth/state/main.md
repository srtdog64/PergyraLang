# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-17.

| Item | Observed value |
|---|---|
| Published main | fast-forwarded to the commit that carries this file; S4 `757f02f76` lands with it, on top of `ea0783975` |
| Work branch | `integrate/track-a`, worktree `C:\Users\user\pgy-wt\ci-win`; parallel tracks land on it as squash commits, then main fast-forwards |
| Open tracks | none running: paused 2026-11-17 by the user while the direction is reviewed; unlanded wip branches are listed in [blocker](blocker.md) |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main` builds the integration head |
| Last CI with Linux jobs executed | `f2a13fd61` green (one infrastructure apt failure rerun) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
