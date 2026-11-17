# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-18; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Published main | `6ff288d70013898a21e867e663f97eaf07e642f8`; CI run 44123611930 attempt 2 retries an unacquired macOS runner; the other 32 jobs passed |
| Work branch at record | `main`, `D:\PergyraLang`; declaration-identity repair is dirty; unrelated folder preserved |
| Candidate artifact identity | parent `6ff288d70`; [verified source, fixed point, installed driver and bounded memory](../evidence/2026-11-18_selfhost_ownership_declaration_identity.md) |
| Active work | publish the verified identity repair and observe exact CI, then continue [full MIR lifetime/array work](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md); full DRV-2 remains open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | `9049e547e` green, run 44099172008; all 33 jobs, including bootstrap and installed-driver checks, executed ([record](../evidence/2026-11-18_selfhost_role_ci_green.md)) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
