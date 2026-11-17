# GPT root lane

User resumed implementation and CI follow-through on 2026-11-17, from
`main` / integration checkpoint `5b0d354fb`.

- Owns the self-compile memory execution and its main integration, measurements,
  focused falsifiers, commit/push and CI verification. No parallel edit lane.
- Edits: `D:\PergyraLang`; validation: existing WSL `/root/mg-main`.
  Unrelated worktrees, dirty files and recovery copies remain preserved.
- Plan: [memory plan](../../agent_work_directives/selfhost_compile_memory_plan_2026-11-17.md).
  Active slice: [A1 execution](../../agent_work_directives/selfhost_memory_a1_execution_2026-11-17.md).
- A2 frozen MIR comparisons remain separate from current source entrypoints.
  A1 source/executable identities: [evidence](../evidence/2026-11-17_selfhost_memory_a1_flat_projection_and_oom.md);
  reached String OOM boundary: [evidence](../evidence/2026-11-17_selfhost_string_allocation_boundary.md).
- Current measurements and gate logs are under WSL
  `.tmp/selfhost-memory-20261117/`; they are not semantic authority.
- No timeout/memory increase, smaller correctness input, weaker negative,
  per-program-shape route, GC/RC, or speculative source annotation/drop.
- Completion is the actual self-compile memory target and required executed CI
  on the published candidate, not an estimate or a focused green slice.
