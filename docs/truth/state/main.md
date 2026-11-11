# Main and CI

Owner: Claude root lane (taken over 2026-11-10,
[decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)).
Checked 2026-11-12.

| Item | Observed value |
|---|---|
| Published main | fast-forwarded to the `ci/all-green` commit that carries this file; previous published head `891a1f23a` |
| Work branch | `ci/all-green`, worktree `C:\Users\user\pgy-wt\ci-win` |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/cc-tip` at the branch head; its `bin/pgy-self-driver` is a local build from 2026-11-11 07:12 |
| CI at `891a1f23a` (run 43061878399) | codegen bootstrap failed at apt: the runner could not resolve the Ubuntu mirrors, so every other Linux job was skipped; Windows, macOS, formal proofs and tsan PASS |
| Last CI with Linux jobs executed | `51fbdca00`, RED (see [gates](gates.md)) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
