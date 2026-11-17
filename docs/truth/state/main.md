# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-17; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Published main | `3a0a385b7050ecedc3bfe9e1517bd84bd1f9bd4d`; CI run 44045107368 PASS |
| Work branch at record | `main`, `D:\PergyraLang`; String allocation owner/symbols, fault gate, workflow and navigation updates dirty; unrelated untracked folder preserved |
| Candidate artifact identity | parent `3a0a385b7`; exact production source and executable hashes in [String evidence](../evidence/2026-11-17_selfhost_string_allocation_boundary.md) |
| Active work | publish locally validated String OOM repair and verify its exact CI; full DRV-2 memory still open, followed by direct-MIR/owned-value dependencies |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | `3a0a385b7` green, run 44045107368; all 33 jobs, including bootstrap and installed-driver checks, executed |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
