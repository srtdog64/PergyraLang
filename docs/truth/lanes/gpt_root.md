# GPT root lane

**Inactive since 2026-11-19.** By the user's
[decision](../decisions/2026-11-19_claude_takes_over_gpt_root_lane.md) the
Claude lane continues this work; see [claude](claude.md).

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
