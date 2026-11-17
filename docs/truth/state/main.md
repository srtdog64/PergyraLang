# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Pre-publication
snapshot on 2026-11-17; the candidate is the commit carrying this record.

| Item | Observed value |
|---|---|
| Published main | `9f5573f18eaef050d5b52880006a380d5fabdfb0`; all 33 jobs in CI run 44073079061 passed |
| Work branch at record | `main`, `D:\PergyraLang`; role receiver migration ready for publication; unrelated folder preserved |
| Candidate artifact identity | parent `9f5573f18`; [role source and binary hashes, focused results and limits](../evidence/2026-11-17_selfhost_memory_role_receiver_migration.md) |
| Active work | publish the general role ABI and old override-route deletion, observe exact CI, then the memory plan's owned-value/retire dependency; full DRV-2 memory remains open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last completed required CI observed | `9f5573f18` green, run 44073079061; all 33 jobs, including bootstrap and installed-driver checks, executed ([record](../evidence/2026-11-17_selfhost_string_ci_green.md)) |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
