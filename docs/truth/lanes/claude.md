# Claude lane

- **P0 v16 lane:** closed 2026-10-16 and handed to GPT. Record:
  `docs/agent_work_directives/p0_v16_handoff_claude_to_gpt_2026-10-16.md`.
- **Now (2026-10-17):** the `docs/truth/` structure and the handoff facade,
  on branch `claude/docs-truth` in worktree `C:\Users\user\pgy-wt\perf`.
  Docs only.
- **Must not touch:** main's working tree in `D:\PergyraLang`, the WSL
  build lease and compiler source. The GPT root lane owns those.
- **Edits in `docs/truth/`:** `decisions/`, `evidence/`, the README and
  this file. `state/` changes go through the GPT root lane.
