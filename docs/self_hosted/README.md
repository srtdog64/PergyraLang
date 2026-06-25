# Self-Hosted Track

This folder is the post-beta self-hosting entry point.

Self-hosting is not a beta blocker. Hard self-hosting now proceeds as staged
substitution: each Pergyra-written compiler rung must pass beside the C/LLVM
oracle pair before it can count. The released compiler is still not claimed as
self-hosted.

## Position

Pergyra should not attempt a full compiler rewrite first. The practical path is:

1. **Soft self-host**: compiler-adjacent tools written in Pergyra.
2. **Partial self-host**: isolated analysis/validation passes that consume stable files or IR dumps.
3. **Hard self-host**: staged compiler-pass substitution after the stable
   subset survives real dogfood and the C/LLVM oracle pair is usable.

The current compiler remains C + LLVM/C backend until this track explicitly
graduates. C and LLVM must finish first because the self-hosted implementation
will be judged by a three-way comparison: C oracle output, LLVM oracle output,
and Pergyra-written tool/pass output. If C and LLVM still disagree, a
self-hosted result cannot be trusted as the deciding value.

## Current Judgement (2026-06-17, supersedes 2026-06-13)

**BDFL decision (2026-06-17): the hard compiler-core migration freeze is lifted.**
The owner explicitly chose to open hard migration (codegen/runtime/compiler
driver) after being shown the substitution numbers; the canonical tracker now
records self-host compiler-internal substitution at ~3.96% direct owner-file LOC-scale, with
runtime/compiler driver/LSP still at 0%. Hard migration now proceeds
*incrementally and verified*: each compiler-core substitute lands as a rung with
a parity gate against the C/LLVM oracle before the next rung opens. This is not a
license to fork a full unverified compiler — the No-Hidden-Flow / verified-rung
discipline still governs. The first hard rung is `src/self_hosted/codegen/`
(C-emit rung-0, 2026-06-17).

C and LLVM remain the oracle pair. Self-host output is still the third comparison
value after C and LLVM, never the source that decides which oracle is right.

### Prior judgement (2026-06-13, retained for history)

Soft/partial self-host preparation may continue. The active work should stay on
compiler-adjacent tools, stable file/dump validators, lexer/parser parity
expansion, and C/LLVM/Pergyra comparator evidence. Hard compiler-core migration
is not open. *(Reversed 2026-06-17.)*

The executable self-hosted Pergyra sources live in `src/self_hosted/`. This
`docs/self_hosted/` folder is the contract and handoff documentation, and
`tests/self_hosted/` owns oracle harnesses plus long-lived parity fixtures. The
self-host source tree must not become a dumping ground for shell harnesses or
golden-output payloads; those are test artifacts, not compiler source owners.
It must also not become a Pergyra spelling of the C folder graph: the
self-hosted compiler flow is owned by `PgyCompilerWorld` under
`src/self_hosted/compiler/world.pgy`, while individual stage directories own the
facts consumed by that flow.

## Architecture Migration Judgement

The C compiler should not be reorganized into broad feature folders before
beta. That would mostly rewrite paths and includes while the real blockers are
still CFG/MIR body safety, AIR evidence coverage, DAG source-of-truth closure,
runtime frontier policy, backend inventory parity, and ABI ownership.

Self-host is the right point to recover the cleaner architecture:

- use Pergyra modules/namespaces to group features such as `intent`, `zone`,
  `world`, `relation`, `effect`, `projection`, `slot`, and `air`;
- avoid generic `_helpers` modules by default;
- split by responsibility and evidence owner, not by line count;
- keep Pergyra source owners under `src/self_hosted/`, and keep oracle/parity
  machinery under `tests/self_hosted/`;
- make the hard compiler flow visible through `PgyCompilerWorld` in
  `src/self_hosted/compiler/world.pgy` rather than a C-style driver mirror;
- keep C as the oracle while each Pergyra-written tool or pass proves parity;
- prefer small compiler-adjacent tools before moving frontend/backend core.

Future agents should treat this as a migration constraint: do not start with a
full compiler rewrite, and do not preserve the current C folder shape as the
target self-host shape.

## Handoff Gate

Self-host preparation starts only when beta closure has produced these
artifacts:

- stable subset contract: syntax, diagnostics, examples, and docs agree;
- CFG/MIR body-safety contract: stable body checks consume CFG/MIR facts, not
  AST fallback judgments;
- AIR graph contract: `pgy.air.graph.v1` is stable enough for external tools to
  validate evidence nodes and boundary drift;
- DAG contract: stable type resolution has no retired recursive resolver usage
  or unresolved metadata dead-end on stable paths;
- ABI contract: Slot/Pin/Zone-bound ownership, panic/failure classes, and C FFI
  layout are frozen for the beta subset;
- backend oracle contract: C remains the reference, LLVM parity gaps are
  explicit, gated, and not silently successful, and hard self-host migration
  waits until the frozen C/LLVM support matrix is green or explicitly
  unsupported. `self-host-backend-tri-compare-test-smoke` is the small
  comparator smoke, and `self-host-backend-tri-compare-extended-test-smoke` is
  the opt-in 29-case C/LLVM/Pergyra comparator gate for C/LLVM closure work;
- dogfood evidence: at least one compiler-adjacent tool path and the C
  `--emit-c` host-bridge path are proven.

If one of these is missing, future agents should return to beta closure instead
of starting self-host migration.

## Hard Contract

The active contract is now
[`10_hard_self_host_contract.md`](10_hard_self_host_contract.md): SoT closure is
a self-hosted pass condition, the C compiler remains the primary oracle, LLVM is
the second oracle when enabled, and bridge scripts are allowed while hidden
fallbacks are not.

The architecture shape is recorded in
[`11_compiler_world_architecture.md`](11_compiler_world_architecture.md) and
[`12_intent_zone_self_host_architecture.md`](12_intent_zone_self_host_architecture.md).
The compiler/codegen/substrate architecture stack is recorded in
[`13_compiler_substrate_architecture.md`](13_compiler_substrate_architecture.md).
The short version: `intent` owns compiler flow, `zone` owns distinct compiler
resources, stage files remain fact owners rather than fake zones, and compiler
substrates such as imports, deterministic collections, diagnostics, MIR facts,
ABI facts, emission buffers, and parity evidence must have named owners.

Fast and heavy self-host checks are split. Use
`make self-host-preparation-contract-test-smoke` for quick structure/manifest
work, and use `make self-host-preparation-test-smoke` when developing or
validating a full rung because it also runs the heavy C/LLVM/Pergyra parity
bundle. A normal compiler build must not imply the full self-host parity suite.

## First Self-Host Rule

The first Pergyra-written programs must be tools around the compiler, not the
compiler core. They should consume stable files (JSON where the source format is
already JSON, diagnostic blocks where the source is a diagnostic verdict, dumps,
manifests, diagnostic tables) and compare their output against the existing C compiler.
This keeps the C implementation as the oracle while making Pergyra useful for
its own ecosystem.

## Required Reading

- `../00_vision.md` - beta then self-hosting decision.
- `../117_backend_strategy_positioning.md` - why full self-host is deferred.
- `../120_vision_and_capability_audit.md` - self-host claims allowed/forbidden.
- `../131_ai_coding_atomic_units.md` - intent-verification pair workflow.
- `../100_beta_readiness_checklist.md` - beta gates that must close first.
- `04_beta_exit_handoff.md` - exact handoff checklist from beta closure to
  soft self-host.

## Documents

- `00_agent_entry.md` - rules for agents working on this track.
- `01_staged_roadmap.md` - staged migration plan.
- `02_required_language_surface.md` - language features needed before self-hosting.
- `03_tool_candidates.md` - first tools suitable for soft self-hosting.
- `04_beta_exit_handoff.md` - beta exit artifacts required before migration.
- `05_compiler_core_gap_analysis.md` - why hard self-host could not start as a
  broad compiler-core rewrite, and what substrate gates opened staged
  substitution.
- `10_hard_self_host_contract.md` - active hard substitution contract, oracle
  rule, bridge/fallback split, and CI owner.
- `11_compiler_world_architecture.md` - hard self-host source shape: the
  compiler flow is owned by `PgyCompilerWorld`, while stage directories own
  facts.
- `12_intent_zone_self_host_architecture.md` - intent/zone architecture for
  compiler flow, codegen resources, and path/source-intake facts.
- `13_compiler_substrate_architecture.md` - self-hosted compiler architecture
  stack and substrate contract for codegen, stage owners, imports,
  deterministic facts, runtime materialization, caching, and parity promotion.
