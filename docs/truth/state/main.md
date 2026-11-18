# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Checked 2026-11-18;
the bare-return/lifetime continuation is locally verified but not yet published.

| Item | Observed value |
|---|---|
| Last fully green published main | `8343ded4c4961dd8e169b400f463f3c3b0afba71`; exact remote verified; [CI 44197220428](https://github.com/srtdog64/PergyraLang/actions/runs/44197220428): all 33 jobs passed, including bootstrap and installed-driver jobs |
| Work branch at record | `main`, `D:\PergyraLang`; published parent above plus uncommitted bare-return and copied-String retirement changes; unrelated folder preserved |
| Candidate artifact identity | Actual Pergyra-built v8 driver SHA-256 `9a06da62ed82fa3d4dbd21699931b70f168ebf5f996ad820395b3d295e92b174`; seven changed compiler files matched the WSL validation copy byte for byte ([local evidence](../evidence/2026-11-18_selfhost_bare_array_return_and_push_operand_lifetime.md)) |
| Active work | [Full MIR lifetime/array work](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md): the bare return and fresh copied String source pass focused C/LLVM lifetime gates; full MIR stops at transition ordinal 1438 and full source-to-C remains OOM. The complete failure census and S1/S2 remain open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last wholly green published CI | Published main above. New local changes have no remote CI result yet. |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
