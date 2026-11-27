# GPT root lane

**Active on DRV-2 since 2026-11-28.** The user requested the fixed-input
refusal census, return/call ownership evidence and general C/LLVM execution
before Slot work. The edit checkout is `D:\PergyraLang` on `main`.
See the [active plan](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md)
and the [fixed-input scan](../evidence/2026-11-28_drv2_fixed_mir_refusal_scan.md).

From 2026-11-19 until resumption, [Claude](claude.md) carried this work
under the [handoff decision](../decisions/2026-11-19_claude_takes_over_gpt_root_lane.md).

- 2026-11-17 to 2026-11-18: the user resumed this lane on the self-compile
  memory plan from main `5b0d354fb`. It published A1/A2, the String
  allocation repair, role receiver erasure, declaration-ID joins, nested and
  bare returns, typed array reverse and inout member restoration, through
  main `a9fbfd298` (CI 44240563770, 33 jobs green).
- Its validation tree was WSL `/root/mg-main` (Git base `5b0d354fb` plus a
  copied source mirror); on 2026-11-19 it held no source change beyond main,
  only scratch probes for the owned-result callee proof at full-MIR ordinal
  1438.
- Plan it left: [MIR owned-value lifetime](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md);
  measurements and gate logs are under `/root/mg-main/.tmp/selfhost-memory-20261117/`.
