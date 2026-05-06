# Self-Hosted Track

This folder is the post-beta self-hosting entry point.

Self-hosting is not a beta blocker. It begins only after beta closure and final
dogfood evidence. Until then, this folder records the contracts future agents
must preserve while preparing the migration path.

## Position

Pergyra should not attempt a full compiler rewrite first. The practical path is:

1. **Soft self-host**: compiler-adjacent tools written in Pergyra.
2. **Partial self-host**: isolated analysis/validation passes that consume stable JSON or IR dumps.
3. **Hard self-host**: frontend/backend migration only after the stable subset survives real dogfood.

The current compiler remains C + LLVM/C backend until this track explicitly
graduates.

## Required Reading

- `../00_vision.md` - beta then self-hosting decision.
- `../117_backend_strategy_positioning.md` - why full self-host is deferred.
- `../120_vision_and_capability_audit.md` - self-host claims allowed/forbidden.
- `../124_ai_coding_atomic_units.md` - intent-verification pair workflow.
- `../100_beta_readiness_checklist.md` - beta gates that must close first.

## Documents

- `00_agent_entry.md` - rules for agents working on this track.
- `01_staged_roadmap.md` - staged migration plan.
- `02_required_language_surface.md` - language features needed before self-hosting.
- `03_tool_candidates.md` - first tools suitable for soft self-hosting.

