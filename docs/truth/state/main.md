# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-18; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Published main | `568bf7433d613791ce8059c5fc7bd6d4257aa705`; exact remote verified; CI run 44140377041 has 32 successes and generated-inventory drift in Linux core ([record](../evidence/2026-11-18_selfhost_nested_return_transfer.md)) |
| Work branch at record | `main`, `D:\PergyraLang`; parent above plus the nested-return repair carried by this commit; unrelated folder preserved |
| Candidate artifact identity | [Source map, actual installed driver, C/LLVM execution and sanitizer controls](../evidence/2026-11-18_selfhost_nested_return_transfer.md); publication and its exact CI remain to be observed |
| Active work | publish the checked dependency and inventory repair, observe exact CI, continue [full MIR lifetime/array work](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md); full DRV-2 remains open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last wholly green published CI | `6ff288d70`, run 44123611930: all 33 jobs passed ([record](../evidence/2026-11-18_selfhost_string_seed_ci_green.md)); later parent run above is not wholly green |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
