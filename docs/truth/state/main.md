# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-14.

| Item | Observed value |
|---|---|
| Published main | fast-forwarded to the commit that carries this file; previous published head `edbb4575b` (the folder reorganization, CI green) |
| Work branch | `fix/generator-drift` (after `reorg/folders`), worktree `C:\Users\user\pgy-wt\ci-win` |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-phi` synced to the branch plus working changes; a pass-through `wslpath` shim at `/root/shim` |
| Last CI with Linux jobs executed | `edbb4575b` (run 43583259892): every required job green (see [gates](gates.md)) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
