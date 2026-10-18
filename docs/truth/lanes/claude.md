# Claude lane

- **Since 2026-10-18:** holds the main-line work the GPT root lane held, by
  the user's request: native checker admission of the canonical program,
  then bootstrap, installed driver and required CI green
  ([decision](../decisions/2026-10-18_native_checker_in_v1.md)).
- **Where:** branch `claude/p0-native-checker` in worktree
  `C:\Users\user\pgy-wt\p0`; WSL build copies `/root/cc-main-1018`
  (clean) and `/root/cc-instr-1018` (scratch census instrumentation, never
  committed).
- **Must not touch:** the uncommitted files in `D:\PergyraLang`'s working
  tree (GPT's `state/main.md` line, its untracked 2026-10-18 directive,
  and the preserved `docs/01_intent_first_design.md` change), Codex
  worktrees, `bin/pgy.exe`, `bin/pgy-self-driver.exe`.
- **Edits in `docs/truth/`:** this file, `decisions/`, `evidence/`, and
  the `state/` files while it holds the main-line work.
- **Earlier:** P0 v16 lane, handed to GPT on 2026-10-16
  (`docs/agent_work_directives/p0_v16_handoff_claude_to_gpt_2026-10-16.md`).
