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

## Handoff Gate

Self-host preparation starts only when beta closure has produced these
artifacts:

- stable subset contract: syntax, diagnostics, examples, and docs agree;
- CFG/MIR body-safety contract: stable body checks consume CFG/MIR facts, not
  AST fallback judgments;
- AIR graph contract: `pgy.air.graph.v1` is stable enough for external tools to
  validate evidence nodes and boundary drift;
- DAG contract: stable type resolution has no retired recursive resolver or
  materializer fallback on stable paths;
- ABI contract: Slot/Pin/Zone-bound ownership, panic/failure classes, and C FFI
  layout are frozen for the beta subset;
- backend oracle contract: C remains the reference, and LLVM parity gaps are
  explicit, gated, and not silently successful;
- dogfood evidence: at least one compiler-adjacent tool path and the C
  `--emit-c` host-bridge path are proven.

If one of these is missing, future agents should return to beta closure instead
of starting self-host migration.

## First Self-Host Rule

The first Pergyra-written programs must be tools around the compiler, not the
compiler core. They should consume stable files (`JSON`, dumps, manifests,
diagnostic tables) and compare their output against the existing C compiler.
This keeps the C implementation as the oracle while making Pergyra useful for
its own ecosystem.

## Required Reading

- `../00_vision.md` - beta then self-hosting decision.
- `../117_backend_strategy_positioning.md` - why full self-host is deferred.
- `../120_vision_and_capability_audit.md` - self-host claims allowed/forbidden.
- `../124_ai_coding_atomic_units.md` - intent-verification pair workflow.
- `../100_beta_readiness_checklist.md` - beta gates that must close first.
- `04_beta_exit_handoff.md` - exact handoff checklist from beta closure to
  soft self-host.

## Documents

- `00_agent_entry.md` - rules for agents working on this track.
- `01_staged_roadmap.md` - staged migration plan.
- `02_required_language_surface.md` - language features needed before self-hosting.
- `03_tool_candidates.md` - first tools suitable for soft self-hosting.
- `04_beta_exit_handoff.md` - beta exit artifacts required before migration.
