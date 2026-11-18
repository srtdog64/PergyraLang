# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Checked 2026-11-18;
the bare-return/lifetime continuation is published and exact-SHA CI is green.
The inout member-restoration candidate and census are not yet published.

| Item | Observed value |
|---|---|
| Last fully green published main | `d21aa036dc7599085c61325c45e8954f7113357f`; exact remote verified; [CI 44219519773](https://github.com/srtdog64/PergyraLang/actions/runs/44219519773): all 33 jobs passed, including bootstrap, installed-driver contracts and the fixed-point join |
| Work branch at record | `main`, `D:\PergyraLang`; published main above plus the uncommitted inout-restoration candidate/census; unrelated untracked folder preserved |
| Candidate artifact identity | Actual Pergyra-built v11 driver SHA-256 `e32b0d5c9f0f3013e4834a342881407e7e51b4bb5b044e250dd6a82cb22a8a53`; changed `.pgy` files matched the WSL validation copy ([local evidence](../evidence/2026-11-18_inout_member_restore_transition.md)) |
| Active work | [Full MIR lifetime/array work](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md): focused inout C/LLVM and existing cleanup gates pass; full MIR still stops at ordinal 1438; a scratch [first-refusal census](../../audits/selfhost_full_mir_independent_refusal_census_2026-11-18.md) reached all route instances. S1/S2 and full source-to-C remain open |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main`, Git base `5b0d354fb` plus the verified production-source mirror |
| Last wholly green published CI | Exact published main above. The candidate has no CI result. |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).
