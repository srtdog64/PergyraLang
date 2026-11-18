# Main and CI

Owner: Claude lane, holding the GPT root lane's work since 2026-11-19
([decision](../decisions/2026-11-19_claude_takes_over_gpt_root_lane.md)).
Checked 2026-11-19.

| Item | Observed value |
|---|---|
| Last fully green published main | `2c199fb28`: [CI 44358794007](https://github.com/srtdog64/PergyraLang/actions/runs/44358794007), all 33 jobs passed, including bootstrap, installed-driver contracts, formal proofs and the fixed-point join |
| Route deletion on main | `34ae17546` deleted the per-program-shape collection program route and its registry row ([evidence](../evidence/2026-11-19_collection_program_route_deleted.md)); green in the run above |
| Work tree | `D:\PergyraLang` on `main`; unrelated untracked folder preserved |
| Linux validation | WSL `Ubuntu-C-WSL` `/root/ts2` builds the candidate; `/root/mg-main` is the GPT lane's copy (base `5b0d354fb` plus a source mirror) |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
