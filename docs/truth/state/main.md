# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-17; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Last main observed before this candidate | `7ea3c781f24385f3adbb4faaec2ee6f3fe9b7ef9`; CI run 44024724037 PASS |
| Work branch at record | `main`, `D:\PergyraLang`; A1 and the reached OOM repair staged for publication; unrelated untracked folder preserved |
| Candidate identity | parent `7ea3c781f`, production Pergyra source and executable hashes in [A1 evidence](../evidence/2026-11-17_selfhost_memory_a1_flat_projection_and_oom.md) |
| Active work | exact-candidate CI and the unchanged full DRV-2 memory boundary; then the memory plan's direct-MIR/owned-value dependency order |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | `7ea3c781f` green, run 44024724037; bootstrap and installed-driver jobs executed |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
