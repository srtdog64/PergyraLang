# Beta Stable Subset Contract

Last updated: 2026-04-26

Status: `beta-freeze-source-of-truth`

This document is the single freeze point for the Pergyra beta stable subset.
Other docs may explain features, but they must not widen or weaken this
contract. A feature is beta-stable only when `syntax -> semantic -> runtime ->
C -> LLVM -> diagnostics -> regression -> docs` agree on the same behavior.

## 1. Core Stable Surface

Stable core:

- `subject`, `object`, `tobject`, `vessel`, `struct`, `enum`, `ability`, `role`,
  `party`, `roster`, `world`, `zone`, `relation`, `effect`, `intent`,
  `projection`, `authority`, `handoff`, `parallel`, `async`, `spawn`, `await`,
  `channel`, `select`, `defer`, `unsafe`, `import`, `use`, and `namespace`.
- Primitive values, `func`, `let`, control flow, basic callable values,
  `Option<T>`, `Result<T, E>`, and the collection implementations required by
  the core contract language.
- UTF-8 string payload preservation is stable for string literals and C/LLVM
  output. `StringLength` is byte-length, and equality/search are byte-exact and
  normalization-blind for beta.
- Module visibility/export contracts for the current resolver and module
  loader surface.
- Package/module resolver stable subset is defined in
  `docs/109_package_module_resolver_contract.md`: file-local
  `import "relative/path.pgy";` resolution, namespace/export visibility,
  circular import rejection, and `pgy init <name>` manifest scaffolding.

Explicit reject:

- Any parser-accepted surface that cannot be closed through semantic,
  runtime/backend parity, diagnostics, regression, and docs must produce an
  explicit diagnostic or be moved out of the beta surface.
- `QubitSlot` / `ClaimQubit` / `Measure` / `Entangle` remain partial
  experimental/v2 surface and must not be described as beta-stable.
- Unicode identifiers, Unicode normalization, locale-sensitive collation/case
  folding, grapheme-cluster iteration, display width, and mixed-encoding source
  files are out-of-beta.
- `pgy install`, dependency version solving, lockfiles, registries, remote
  imports, and package supply-chain integrity are explicitly out-of-beta.

Out of beta:

- Full quantum resource model, WASM backend, package manager release workflow,
  advanced debugger, full GPU/Spray, Skia/render, full FP/HKT/functor algebra,
  arbitrary ownership, arbitrary map keys, and richer multi-instance
  observability query language.

## 2. Generic Contract Stable Subset

Stable:

- Exact generic type argument matching.
- Ability-bound generic contracts.
- Multi-bound `where T: A + B` enforcement on implemented declaration/call and
  module-consumer paths.
- Default type argument actual resolution where implementation and regression
  evidence already exist.
- Generic ownership classifier baseline: unresolved `T` is not silently treated
  as copy-only; generic `own/ref` boundaries must use classifier-backed facts.

Explicit reject:

- Broader type-family generalization without DAG/source-of-truth evidence.
- Higher-kinded types, typeclass/functor algebra, and generalized FP module
  syntax.
- Generic ownership combinations that escape the current classifier contract.

## 3. Ownership Stable Subset

Stable:

- Classifier-backed `own/ref` over copy values, boundary-visible aggregates,
  movable resources, anchored handles, `Slot<T>`, `SecureSlot<T>`,
  `DeviceSlot<T>`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`, and stable
  direct/summary helper-chain boundaries.
- `Token<T>` transport across `spawn`, channel/cancellation payloads, and task
  boundaries is explicitly rejected.
- `SecureSlot<T>` token ABI is beta-stable across build modes and backends:
  inline C, exported runtime, and LLVM-linkable runtime use the same
  `PgyToken<T>` layout with read/write capability bits and hard-fail token
  checks. Only plain `Slot<T>` has a zero-overhead release layout.
- Shared `ref`/`ref` reads across parallel tasks are allowed.
- `ref`/`own` and `own`/`own` parallel task-boundary conflicts are rejected.
- Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable only for the
  documented runtime ABI and lifecycle regression set.

Option C ownership lift:

- `pin slot as view { ... }` remains the target stable surface for scoped
  Pin/Lease, with automatic cleanup.
- `PinnedView<T>` is the non-block handle form.
- `WriteView<T>` is exclusive; it must participate in CFG/dataflow aliasing and
  parallel conflict checks.
- `defer` cleanup is part of the ownership closure because pin/unpin and drop
  edges must survive early return, branch join, and cleanup paths.
- Generic parameter ownership classifier is required before generic `own/ref`
  is called beta-closed.

Explicit reject:

- Pinning `QubitSlot`.
- Pin view escape from its block/lifetime.
- Pin across `await` unless a later checked suspension contract is added.
- Pin conflicts across parallel tasks.
- Invalid or forged pin token/capability.
- General Rust-style lifetime annotations and universal ownership lattice.

## 4. Collections Stable Subset

Stable:

- `Array<T>` through array literals and the documented array builtin family.
- `Slice<T>` where current semantic/runtime/backend parity exists.
- `List<T>`.
- `Set<T>`.
- `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`,
  `HashMap<Bool, T>`.
- Queue/map/list string access fixtures already in backend compare.

Explicit reject:

- Unsupported map key kinds.
- Arbitrary key-universal collection contracts.
- Collection operations that lack C/LLVM parity regression.

## 5. Intent / Zone / World / AIR Stable Subset

Stable:

- Intent step ordering, inherited/derived contract baseline, `where`, `using`,
  `who`, `requires`, `authorized by`, `causes`, `on`, and `compensate` baseline.
- Zone/world query and handoff baseline covered by runtime/frontier regression.
- Runtime observability baseline: `last`, `history`, `active`, `recent`.
- Observability/tracing schema is defined in
  `docs/112_observability_trace_schema.md`: stable intent query families are
  `IntentLast*`, `IntentHistory*`, `IntentActive*`, `IntentRecent*`; stable
  authority failure snapshot fields are `ok`, `zone`, `participant`, `code`,
  and `reason`.
- Memory/concurrency model is defined in
  `docs/113_memory_concurrency_model.md`: `parallel` joins before following
  control flow, accepted writes are visible after join, shared `ref`/`ref` reads
  are allowed, `ref`/`own` and `own`/`own` conflicts are rejected, and
  non-blocking ownership-bearing receive/cancel/close surfaces are copy-only or
  explicit rejects for beta.
- AIR Phase 1: verification-only `Intent Node` + `Boundary Node`, strict
  evidence by default, sync/async drift detection, source-span diagnostics, and
  execution-boundary scanning for `spawn` / `async` / `parallel`,
  `channel` / `select`, and known IO calls.

Explicit reject:

- Runtime authority/boundary failures that cannot be surfaced as queryable
  state or deterministic hard-fail under the failure class contract.
- Richer multi-instance trace queries outside the frozen observability baseline.
- Event streaming, structured JSON trace export, distributed trace correlation,
  user-code registry hooks, and stable binary trace format.
- Full weak-memory vocabulary, user-selectable memory orders, scheduler
  fairness guarantees, lock-free correctness claims, anonymous async closure
  capture/lifetime analysis, and cross-thread `Arc<T>` / `Send` / `Sync` style
  trait systems.

## 6. Backend And Tooling Contract

Stable:

- Linux: C backend + LLVM backend regression coverage.
- Windows: C backend regression coverage always; LLVM smoke/backend compare
  only when executable `llvm-config --libs core` evidence is present.
- macOS: C-only CI preflight through `make ci-macos`; macOS LLVM/backend parity
  remains out-of-beta until a dedicated LLVM support contract is green.
- Formatter, LSP, and debugger are beta-conformance surfaces only for their
  explicitly tested subset; they must not imply full editor/debugger maturity.
- Tooling beta-stable subset is exactly the `make tooling-conformance-test-smoke`
  contract: formatter `--check`/`--write` idempotence plus compile smoke, LSP
  initialize capabilities plus keyword hover/completion, and debugger CLI
  parse/semantic/interactive quit path.
- Tooling out-of-beta: DAP conformance, binary breakpoints, variable watch,
  multi-file workspace indexing, refactor edits, and full editor-grade
  diagnostic streaming.
- Performance baseline is guarded by `make perf-contract-test-smoke`.
- Mandatory beta gate inventory is frozen in `docs/111_beta_test_suite_freeze.md`
  and guarded by `make beta-test-suite-freeze-test-smoke`.
- Stdlib stable subset is defined in `docs/108_stdlib_beta_freeze.md` and gated
  by `make stdlib-test-smoke`; known modules such as `http`, `storage`, `page`,
  and `spray` remain experimental even if compiler-known.
- Package/module resolver beta subset is defined in
  `docs/109_package_module_resolver_contract.md` and gated by
  `make package-module-resolver-test-smoke`.
- String/unicode beta policy is defined in
  `docs/110_string_unicode_policy.md` and gated by
  `make unicode-policy-test-smoke`.
- Observability/tracing schema is defined in
  `docs/112_observability_trace_schema.md` and gated by
  `make observability-schema-test-smoke`.
- Memory/concurrency model is defined in
  `docs/113_memory_concurrency_model.md` and gated by
  `make memory-concurrency-model-test-smoke`.

Beta-complete gate:

- `make test-all`
- `make test-semantic`
- `make llvm-test-smoke`
- `make llvm-test-abi-same-process`
- `make llvm-test-backend-compare`
- `make air-drift-test-smoke`
- `make air-backend-nonimpact-full-test-smoke`
- `make air-strict-backend-compare-test-smoke`
- `make cfg-body-dataflow-test-smoke`
- `make type-resolution-dag-test-smoke`
- `make runtime-panic-contract-test-smoke`
- `make runtime-panic-abi-test-smoke`
- `make runtime-frontier-contract-test-smoke`
- `make projection-diagnostic-contract-test-smoke`
- `make beta-readiness-checklist-test-smoke`
- `make formal-semantics-test-smoke`
- `make stdlib-test-smoke`
- `make package-module-resolver-test-smoke`
- `make unicode-policy-test-smoke`
- `make beta-test-suite-freeze-test-smoke`
- `make observability-schema-test-smoke`
- `make memory-concurrency-model-test-smoke`
- `make tooling-conformance-test-smoke`
- `make perf-contract-test-smoke`
