# GPT root lane

User resumed implementation and CI follow-through on 2026-11-17, from
`main` / integration checkpoint `5b0d354fb`.

- Owns the self-compile memory execution and its main integration, measurements,
  focused falsifiers, commit/push and CI verification. No parallel edit lane.
- Edits: `D:\PergyraLang`; validation: existing WSL `/root/mg-main`.
  Unrelated worktrees, dirty files and recovery copies remain preserved.
- Plan: [memory plan](../../agent_work_directives/selfhost_compile_memory_plan_2026-11-17.md).
  Active slice: [A2 execution](../../agent_work_directives/selfhost_memory_a2_execution_2026-11-17.md).
- Fixed middle-size input is the semantic MIR emitted at the clean checkpoint;
  each candidate consumes the same bytes for output and RSS comparison.
- Current measurements and gate logs are under WSL
  `.tmp/selfhost-memory-20261117/`; they are not semantic authority.
- No timeout/memory increase, smaller correctness input, weaker negative,
  per-program-shape route, GC/RC, or speculative source annotation/drop.
- Completion is the actual self-compile memory target and required executed CI
  on the published candidate, not an estimate or a focused green slice.
