# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-15.

| Item | Observed value |
|---|---|
| Published main | fast-forwarded to the commit that carries this file; previous published head `16ac154e1` (flow-summary row closed) |
| Work branch | `integrate/track-a` (integration of the parallel repair tracks onto main), worktree `C:\Users\user\pgy-wt\ci-win`; track branches `fix/native-flow-refusals` (landed `226613c3c`), `fix/flow-summary-row-gates` (landed), `fix/selfhost-landed-copies` (open) |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-phi` synced to the branch plus working changes; a pass-through `wslpath` shim at `/root/shim` |
| Last CI with Linux jobs executed | `20325cad5` (run 43653186068) green; later pushes and their fixes in [gates](gates.md) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
