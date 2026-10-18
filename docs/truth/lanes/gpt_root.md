# GPT root lane

Since 2026-10-16 ([decision](../decisions/2026-10-16_main_only_integration.md)).

- Owns: main, its integration and Git, the WSL build lease and processes,
  and every compiler source change on main.
- Edits in `docs/truth/`: `state/*.md` and this file.
- Validation copy: see [identified gates](../state/gates.md). Older Linux
  roots are lookup evidence, not the current source/executable owner.
- Subagent scopes are fixed by the plan linked from [blocker](../state/blocker.md).
  Root runs commands and gates; agents do not own separate worktrees.
- Current work: [state/blocker.md](../state/blocker.md).
