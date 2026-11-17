# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-18; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Published main | `9049e547eaa9eb72c242def8a86e3f2b6c7d706d`; all 33 jobs in CI run 44099172008 passed |
| Work branch at record | `main`, `D:\PergyraLang`; String temporary seed bridge is dirty; unrelated folder preserved |
| Candidate artifact identity | parent `9049e547e`; [verified String bridge source/builds and failed full DRV-2 memory](../evidence/2026-11-18_selfhost_string_temporary_seed_bridge.md) |
| Active work | publish the locally verified bridge and observe exact CI, then continue the memory plan's full MIR lifetime/array work; full DRV-2 remains open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | `9049e547e` green, run 44099172008; all 33 jobs, including bootstrap and installed-driver checks, executed ([record](../evidence/2026-11-18_selfhost_role_ci_green.md)) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
