# Language Completion Board

Last updated: 2026-06-08

This board is the compact language-completion index. It intentionally does not
duplicate the operational beta checklist.

Live source of truth:

- Beta execution checklist: `docs/100_beta_readiness_checklist.md`
- Beta closure board: `docs/70_beta_closure_master_board.md`
- Stable subset: `docs/107_beta_stable_subset.md`
- Source-of-truth spine: `docs/125_source_of_truth_spine.md`
- Post-beta self-hosting handoff: `docs/self_hosted/README.md`

## Current Readiness

Feature-surface feel is about 85%. Strict beta readiness is about 83%.
Do not raise the official number beyond the low-80% band until the remaining
source-of-truth closures below are consumed by implementation and current
full-suite gates, not just by docs.

## Closure Axes

| Axis | Status | Beta risk | Current source of truth |
| --- | --- | --- | --- |
| CFG / body dataflow | In progress | High | `docs/100_beta_readiness_checklist.md` |
| AIR boundary verification | In progress | High | `docs/104_air_compiler_architecture.md` |
| Type-resolution DAG | In progress | High | `docs/65_type_resolution_audit.md` |
| MIR / LLVM declaration inventory | In progress | Medium-high | `docs/125_source_of_truth_spine.md` |
| ABI / Slot / Pin ownership | In progress | Medium-high | `docs/semantics/04_ownership_abi.md` |
| Runtime propagation frontier | In progress | Medium-high | `docs/70_beta_closure_master_board.md` |
| Tooling and platform matrix | Partial | Medium | `docs/100_beta_readiness_checklist.md` |
| Self-hosting readiness | Post-beta consumer | Not a beta blocker | `docs/self_hosted/README.md` |

## Stable Direction

Pergyra is a systems language with domain-semantic extensions. The language must
not become a Rust-style lifetime language, a Zig-style comptime language, or a
framework DSL. The beta line is stable when the compiler has a clear source of
truth for each judgment:

- Syntax belongs to the parser and AST.
- Body safety belongs to CFG/MIR facts.
- Abstraction-boundary verification belongs to AIR evidence.
- Declaration dependency and generic/ability facts belong to the DAG.
- Backend emission belongs to MIR/DIR/RIR inventories, not AST fallback.
- Runtime resource validity belongs to Slot/Pin/handle ABI contracts.
- Dogfood starts through C backend and host/module bridges; WebGL and GPU paths
  stay ecosystem modules, not core language surface.

## Closed Since The Earlier Board

- `.inc` debt is closed for production sources.
- Large production owners are below the 600 LOC signal threshold.
- Stable stdlib IO has `FileOpen`, `FileRead`, `FileWrite`, `FileClose`,
  `ReadFile`, `WriteFile`, and `FileExists` on the same runtime path policy.
- String scalar surface includes `StringIndexOf`; `StringJoin(Array<String>,
  String)` has C/LLVM parity coverage.
- Self-hosting has a soft diagnostic-catalog checker scaffold, but it is now
  treated as post-beta dogfood evidence rather than beta closure priority.

## Still Blocking Beta-Complete

- CFG must become the body-safety source of truth for ownership, cleanup,
  branch/join, loop, channel, cancellation, zone, and effect transitions.
- AIR must consume evidence from HIR CFG, RIR boundary/effect propagation, MIR
  cleanup/pin cleanup, DAG generic/ability facts, and runtime frontier policy.
- DAG compatibility seams must stop deciding semantic outcomes; compatibility
  counters may remain only as explicit diagnostics and smoke-gated inventory.
- MIR/LLVM declaration bootstrap must stop depending on AST-carried inventory
  for the frozen subset.
- Slot/Pin/Zone-bound handle contracts must be frozen as layered safety:
  static boundary rejection plus runtime generation/token validation.
- Self-hosting remains post-beta until compiler-writing surfaces are stable
  enough for a small Pergyra tool to replace an equivalent C tool without
  special-case runtime assumptions.
