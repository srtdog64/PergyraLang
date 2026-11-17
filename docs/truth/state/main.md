# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-18; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Last verified published main | `6ff288d70013898a21e867e663f97eaf07e642f8`; all 33 jobs in CI run 44123611930 passed ([record](../evidence/2026-11-18_selfhost_string_seed_ci_green.md)) |
| Work branch at record | `main`, `D:\PergyraLang`; identity repair committed as `43c70b28c`; publication record is dirty; unrelated folder preserved |
| Candidate artifact identity | `43c70b28c` plus this publication record; [verified source, fixed point, installed driver and bounded memory](../evidence/2026-11-18_selfhost_ownership_declaration_identity.md) |
| Active work | publish the verified identity repair and observe exact CI, then continue [full MIR lifetime/array work](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md); full DRV-2 remains open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | Published bridge above; declaration-identity candidate still needs exact-SHA CI after publication |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
