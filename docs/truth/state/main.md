# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Checked 2026-11-18;
the current array-transform/lifetime continuation is not yet published.

| Item | Observed value |
|---|---|
| Published main | `b2fd8d424f33720fcd857bb811842b28c824e050`; exact remote verified; [CI 44168299517](https://github.com/srtdog64/PergyraLang/actions/runs/44168299517), attempt 2: all 33 jobs actually ran and passed, including bootstrap, installed-driver contracts and fixed-point join |
| Work branch at record | `main`, `D:\PergyraLang`; published parent above plus uncommitted typed array-transform and lifetime-consumer changes; unrelated folder preserved |
| Candidate artifact identity | The actual Pergyra-built installed v5 driver SHA-256 is `b39bd3523df9333068f1b91e9cf3e130b49c3ee044402075a2e64c244e656552`; all 2,840 self-host source files match normalized map `55aef9b95ab6539099d9cd18595591e16aec97f207ebd512a06b692ce0fa1b26` in the WSL validation tree |
| Active work | [Full MIR lifetime/array work](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md): the typed transform and lifetime consumer are locally verified; the full source-to-C remains OOM at the unchanged 14 GiB boundary. The next reached direct-MIR transition and complete failure census remain open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last wholly green published CI | Published main above. Its first bootstrap attempt failed while reading a runner's LLVM header (`Input/output error`, followed by Git bus error); one bounded failed-job rerun and its dependent join passed. No source or gate relaxation was used |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
