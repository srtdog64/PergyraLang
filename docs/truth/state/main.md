# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-17; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Published main | `1ff353e890021c33ca7fe1a513df8502c9e32974`; CI run 44063602879 has a Linux gate failure |
| Work branch at record | `main`, `D:\PergyraLang`; phi header-policy correction staged for publication; unverified role receiver carrier edits and unrelated folder preserved outside the candidate |
| Candidate artifact identity | parent `1ff353e89`; no production Pergyra change; frozen source/executable identity in [String evidence](../evidence/2026-11-17_selfhost_string_allocation_boundary.md) |
| Active work | publish [phi consumer correction](../evidence/2026-11-17_selfhost_string_ci_header_fix.md) and verify exact CI; then resume role ABI and owned-value dependencies; full DRV-2 memory stays open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | `3a0a385b7` green, run 44045107368; all 33 jobs, including bootstrap and installed-driver checks, executed |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
