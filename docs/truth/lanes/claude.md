# Claude lane

- **Since 2026-11-10: root lane.** By the user's
  [decision](../decisions/2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)
  Claude holds main, the bootstrap, the installed driver and required CI,
  and edits `state/main.md`, `state/blocker.md` and `state/gates.md`. Work
  branch `ci/all-green` in worktree `C:\Users\user\pgy-wt\ci-win`.
- **2026-10-24 to 2026-11-10: inactive.** By the user's decision the GPT root lane
  does all further work; the branch and its open items are in the
  [handoff](../../agent_work_directives/native_checker_handoff_claude_to_gpt_2026-10-24.md).
- **2026-10-18 to 2026-10-24:** holds the main-line work the GPT root lane held, by
  the user's request: native checker admission of the canonical program,
  then bootstrap, installed driver and required CI green
  ([decision](../decisions/2026-10-18_native_checker_in_v1.md)).
- **Where:** branch `claude/p0-native-checker` in worktree
  `C:\Users\user\pgy-wt\p0`; WSL build copies `/root/cc-main-1018`
  (clean) and `/root/cc-instr-1018` (scratch census instrumentation, never
  committed). The WSL distro is `Ubuntu-C-WSL` since 2026-10-23 (its disk
  moved to `C:\WSL`); the `Ubuntu-E-WSL` registration names the removed
  E: copy.
- **Must not touch:** the uncommitted files in `D:\PergyraLang`'s working
  tree (GPT's `state/main.md` line, its untracked 2026-10-18 directive,
  and the preserved `docs/01_intent_first_design.md` change), Codex
  worktrees, `bin/pgy.exe`, `bin/pgy-self-driver.exe`.
- **Edits in `docs/truth/`:** this file, `decisions/`, `evidence/`, and
  [state/native_checker.md](../state/native_checker.md); main's state files
  are the GPT root lane's.
- **Earlier:** P0 v16 lane, handed to GPT on 2026-10-16
  (`docs/agent_work_directives/p0_v16_handoff_claude_to_gpt_2026-10-16.md`).
