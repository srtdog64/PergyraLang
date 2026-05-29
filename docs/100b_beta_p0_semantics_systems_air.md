# Beta Readiness Checklist - P0 Semantics, Systems, CFG, AIR

> Split from `docs/100_beta_readiness_checklist.md` on 2026-05-29.
> Keep active blocker edits in the shard that owns the relevant closure track.

## 0. Formal Semantics / Proof Obligations

Status: `IN PROGRESS / BLOCKER-DOC`

Goal:

- The beta stable subset must have an explicit mathematical contract before it is called beta-complete.
- The proof source of truth is the split proof pack in `docs/semantics/`, with `docs/102_formal_semantics_and_proof_obligations.md` kept as the stable index.
- Proof evidence is not the same as proof: regression, smoke, and backend-compare runs are supporting evidence, while the stable theorem/invariant statements live in the formal semantics doc.
- The proof scope is intentionally narrow: core declarations, stable generics, anchored own/ref, stable collections, observability baseline, `parallel` baseline, CFG-backed body safety, runtime propagation, DAG, ABI ownership, and C/LLVM parity.

Closed now:

- `docs/semantics/` is the source of truth for beta proof vocabulary, judgments, theorem statements, and remaining proof obligations.
- `docs/102_formal_semantics_and_proof_obligations.md` now points to the split proof pack and remains as the stable English index.
- The doc explicitly separates language proof obligations from the math library design in `docs/45_math_layer_design.md`.
- Out-of-beta proof claims are sealed for full quantum, full FP/HKT/functor algebra, arbitrary ownership, arbitrary map keys, GPU/Spray, Skia/render, package manager, and advanced debugger semantics.
- `Runtime Panic Parity` now has a formal theorem slot in `docs/semantics/06_backend_parity.md`.
- Secure token unforgeability and authority transfer single-owner now have formal theorem slots in `docs/semantics/04_ownership_abi.md`.
- Slot capability calculus now has a formal theorem slot in `docs/semantics/08_slot_capability_calculus.md`; `docs/semantics/proofs/SlotCalculus.v` is explicitly proof-sketch only until a Coq CI gate type-checks it.
- Slot capability calculus now explicitly records the negative claim that Slot
  is not a borrow checker by itself. Runtime generation/token/pin-state safety
  and borrow-checker-equivalent static safety are separate proof claims.
- Slot capability calculus now also records the positive claim: Pergyra does
  not expose memory as address ownership; it exposes memory as a modular
  resource boundary. A Slot is the stable source-level boundary, while the
  backend handle below it remains replaceable.
- Canonical Slot thesis: Pergyra does not expose memory as address ownership;
  Pergyra exposes memory as a modular resource boundary with a replaceable
  backend handle.
- Canonical short form: Pergyra exposes memory as a modular resource boundary;
  Slot has a replaceable backend handle.
- Canonical semantic split: static rejection covers unsafe transition across a
  known boundary; runtime validation covers dynamic existence/state of a
  resource handle. Pergyra does not statically predict every business object's
  lifetime. It rejects unsupported world/zone/task handoff, missing authority,
  unsupported token transport, pin/view suspension or transport crossing, and
  projection source/target/kind mismatch when those coordinates are visible;
  generation freshness, token validity, TTL cleanup, registry presence, and
  tombstone state remain runtime facts unless a boundary rule exposes the escape
  statically.
- The Slot Coq sketch now models access modes explicitly (`Read`, `Write`,
  `Release`, `Pin`) and carries proof obligations for stale
  read/write/release handles, issued-token read/write/pin/release,
  unissued-token read/write/pin/release rejection, pinned-handle release
  rejection, and pin non-eviction.
- Linux CI now installs `coq`, so `make formal-semantics-test-smoke` type-checks `docs/semantics/proofs/SlotCalculus.v` in CI instead of only checking proof-pack text.
- Slot capability runtime evidence was rechecked with `make test-security` (142/142 passed): stale-generation read/write/pin/release rejection, stale `SlotIsValid` false, zero-id sentinel and slot-id wrap tombstone before ABA reuse, tampered-view generation unpin rejection, double-unpin rejection, release-while-pinned, TTL cleanup skip while pinned, invalid secure token rejection, revoked-token rejection, raw secure-slot release rejection, concurrent secure write rejection, and release-after-unpin are covered.
- `runtime-panic-abi-test-smoke` now covers forged zero-token read/write/release
  rejection for inline C and exported C/LLVM-linkable secure-slot entrypoints.
- SecureSlot token ABI is now build-mode stable: inline C, exported runtime, and
  LLVM-linkable runtime all use the same `PgyToken<T>` layout with read/write
  capability bits. The old release-mode SecureSlot macro has been removed, and
  `runtime-panic-abi-test-smoke` covers no-`PGY_SAFE_SLOTS` invalid-token and
  released-slot hard-fail paths.
- `pgy_abi_spec.h` now carries matching debug/release SecureSlot layout rows for
  all stable primitive payloads (`Int`, `Long`, `Float`, `Double`, `Bool`,
  `String`), and `make test-abi` checks runtime size/token offsets against the
  ABI spec.
- Non-pin handle expiration is a layered contract, not a pin-only story. The
  beta contract is: arena lane checks, CFG/body dataflow, zone/world
  channel-only crossing, token transport rejection, and generation/token
  runtime validation together cover stale-handle scenarios. First-class
  Zone-Bound Handle typing (`SlotHandle<T> in Zone` or equivalent `handle@zone`
  sugar) remains a beta-freeze design decision: implement it before freeze or
  keep the current `BORROW_TRACKED` / anchored-handle conservative rejection as
  the documented stable behavior.
- Authority token mismatch now has a shared runtime contract code/reason
  (`authority-token-mismatch`), queryable runtime state, C/LLVM ABI coverage in
  `authority_failure_abi`, backend-compare coverage in `authority_failure_surface`,
  and direct runtime coverage in `make test-security` (142/142 passed).

Remaining:

- Tie each B0 closure item to a theorem/invariant row before calling that item beta-complete.
- Keep DAG, runtime propagation, MIR declaration inventory, ABI ownership, panic parity, secure token invariants, and backend parity blockers open until their theorem statements and regression evidence match.
- Do not advertise mechanized proof for beta unless a separate Lean/Coq or executable small-step model is added and CI type-checks it.
- Do not advertise "Slot as borrow checker"; do not claim that Slot alone
  proves borrow-style safety;
  borrow-checker-equivalent claims require the section `0b` CFG bridge facts
  plus section `4` ABI ownership parity.
- Keep Slot wording positive and precise: Slot is an address abstraction,
  ownership boundary, capability gate, and replaceable backend handle. Do not
  frame it as raw pointer ownership or Rust-style lifetime ownership.
- Decide the Zone-Bound Handle direction before beta freeze. If it is in beta
  scope, add type-level zone scope facts and diagnostics for handle escape past
  zone lifetime. If it is out of beta, document conservative rejection as the
  stable subset and forbid docs from implying non-pin handles have Rust-style
  lifetime proof.
- Keep C/LLVM panic-class regressions green for divide-by-zero, out-of-bounds, released slot, double release, invalid secure-slot token, OOM, authority token mismatch, direct unwrap misuse, and `?` Err-in-non-Result misuse.
- **[NEW]** Add state-machine proofs for the Intent system's rollback and cleanup closure.

Evidence command:

```sh
make formal-semantics-test-smoke
```

## 0a. Systems Language Baseline Closure

Status: `BLOCKER`

Source of truth: `docs/19_design_philosophy.md`

Goal:

- Pergyra is a systems language with domain extensions. The systems-language
  baseline is non-negotiable: no mandatory GC, predictable memory, C FFI, ABI
  stability, raw escape, optional runtime, and compile-time determinism.
- Domain primitives (`intent`, `zone`, `world`, `authority`, `handoff`,
  `Channel`, `parallel`) are first-class, but they are layered on top of the
  systems baseline. They must not replace or weaken it.
- Beta must not claim ecosystem readiness until the systems substrate can
  survive domain-layer evolution without ABI drift, hidden runtime cost, or
  nondeterministic codegen.

Closed now:

- `docs/19_design_philosophy.md` now states the systems-language identity before
  the Slot/resource philosophy: Pergyra is a systems language first, and domain
  extensions are layered above that substrate.
- The stable identity explicitly ties abstraction portability to systems
  portability: if Pergyra cannot reach the target platform with predictable ABI
  and memory behavior, the domain abstraction portability claim is hollow.
- ABI stability is already partially enforced by `src/runtime/pgy_abi_spec.h`,
  ABI static assertions, `make test-abi`, runtime panic ABI smoke, and C/LLVM
  backend compare gates.
- Slot wording is aligned with this identity: source code observes a modular
  resource boundary and capability gate, while the backend handle below Slot
  remains replaceable.
- `--runtime=none` is now a parsed driver mode with structured diagnostics.
  It rejects runtime-dependent surfaces (`parallel`, `spawn`, `Channel`,
  `intent`, `zone`, `world`, `event`, async/future/select/task-group) through
  `PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED`, and it separately blocks false
  freestanding success until C/LLVM no-runtime lowering exists.
- `SlotRawPointer(...)` is now reserved as an explicit unstable raw-escape
  surface and rejected with `PGY_SEM_RAW_ESCAPE_UNSTABLE`. `unsafe { ... }`
  remains a lexical marker only; it does not grant raw pointer capability.

Remaining:

- Define the system-tier raw escape contract. `unsafe { ... }` exists, but
  raw pointer / inline-asm escape from Slot is not beta-stable until a syntax,
  semantic gate, ABI lowering rule, and diagnostics are implemented. Until then
  docs must not imply that `pin slot as view { ... }` is enough for driver,
  kernel, embedded ISR, or MMIO code. Pin/Lease is a typed lexical lease, not
  the system-tier raw escape; source of truth:
  `docs/74_slot_pinning_caching.md`.
- Freeze unsafe as scoped capability, not a mode bit. Plain `unsafe { ... }`
  must remain a lexical boundary marker until a scoped spelling such as
  `unsafe(raw) { ... }` / `unsafe(ffi) { ... }` has semantic gates, AIR
  evidence, ABI lowering, and C/LLVM parity. Source of truth:
  `docs/132_unsafe_capability_scope.md`.
- Implement verified freestanding C/LLVM lowering for `--runtime=none`.
  Current mode is intentionally conservative: it defines the CLI contract and
  rejection surface, but it does not emit a no-runtime binary yet.
- Elevate ABI non-leakage to a beta contract: intent/zone/world changes must
  not break C FFI ABI. Domain-layer evolution is allowed only inside the ABI
  envelope guarded by `pgy_abi_spec.h`, ABI tests, and backend parity.
- Add deterministic codegen evidence. Type resolution, generic resolution, AIR
  verification, MIR inventory traversal, C emission, and LLVM emission must not
  depend on hash-map or pointer iteration order. A repeat-build artifact hash
  smoke now exists as `make codegen-determinism-test-smoke`; beta completion
  requires expanding it to the full frozen backend fixture set.

Evidence command:

```sh
make beta-readiness-checklist-test-smoke
make codegen-determinism-test-smoke
make runtime-none-contract-test-smoke
make raw-escape-contract-test-smoke
```

## 0b. Function CFG / Body Dataflow Closure

Status: `IN PROGRESS / BLOCKER`

Source of truth: `docs/103_cfg_body_dataflow_need.md`

Goal:

- Strict beta must not depend on AST-shaped local traversal for routine body safety.
- HIR/MIR CFG already exists, so the blocker is not "add a CFG from zero". The blocker is promoting CFG/dataflow facts to the semantic source of truth for function/action/intent bodies.
- Body safety must cover normal control flow, exceptional cleanup flow, ownership/resource flow, zone/effect transitions, and parallel/channel boundaries before the language is advertised as ecosystem-safe beta.

Closed now:

- HIR has function CFG v0 with predecessor/reachability, dominator/frontier, loop-depth, local-def, and phi-candidate skeleton facts.
- HIR CFG construction now has a hard structural contract before downstream
  consumers run: `hir_validate_cfg_shape()` rejects open fallthrough blocks,
  invalid successor indices, inconsistent terminator successor flags, missing
  branch conditions, and block-id drift before dominance/frontier/loop/phi
  analysis. `hir_validate_cfg_predecessors()` then verifies that materialized
  predecessor lists mirror every successor edge. This closes the previous
  "CFG consumers trust generated shape by convention" seam.
- HIR CFG summaries now expose `return_block_count` and
  `normal_exit_block_count`. Reachable `HIR_BLOCK_UNREACHABLE` blocks are the
  normalized normal-fallthrough exits, so all-path-return consumers can move
  toward a direct CFG fact instead of re-walking AST body shape.
- HIR CFG ownership is now a named compiler owner seam:
  `src/compiler/hir_cfg.c` owns CFG finalization, reachability,
  dominator/frontier, dominator tree, loop-depth, local-def, phi-candidate,
  phi-materialization, and CFG summary facts. `src/compiler/hir_lower_cfg.c`
  owns AST-body to basic-block CFG construction. `src/compiler/hir.c` is
  reduced to the declaration/routine lowering orchestration owner, while
  `src/compiler/hir_analysis.c` owns signature/direct-call/control-flow
  detection.
- HIR CFG lowering now represents loop exits and loop backedges explicitly for
  `break` / `continue`. `while` and `for` bodies carry a loop context, so
  `break` terminates the current block with a `goto` to the loop exit and
  `continue` terminates with a `goto` to the loop header. This keeps HIR CFG
  dominance/frontier/loop-depth facts aligned with semantic loop flow instead
  of leaving loop control as opaque AST payload.
- HIR CFG loop control is now label-aware: nested `break outer` and
  `continue outer` resolve to the named loop's exit/header rather than the
  nearest loop. This keeps HIR CFG edge facts aligned with semantic loop-label
  validation.
- HIR CFG lowering now represents `match` as a case dispatch chain instead of
  a single opaque statement payload. Each `case` is a CFG branch condition,
  case/default bodies flow to a join block when they fall through, and
  terminating cases stay closed. `src/test_hir.c` locks this with
  `HIR CFG lowers match cases and default as explicit edges`.
- HIR CFG lowering now represents `select` as the same dispatch/join shape.
  Channel readiness cases and default bodies are explicit CFG edges instead of
  an opaque select payload. `src/test_hir.c` locks this with
  `HIR CFG lowers select cases and default as explicit edges`.
- HIR CFG lowering now traverses `unsafe` block bodies instead of treating
  `unsafe` as an opaque statement. Nested terminators inside `unsafe` blocks are
  visible to the same CFG dominance/reachability consumers as ordinary block
  terminators.
- MIR has routine/block/instruction/cleanup blocks, SSA version maps, def/use summaries, rollback/invalidation exceptional CFG, liveness/DCE slices, and backend vertical slices.
- RIR already carries flow-block summaries for resource/projection/world-handoff/invalidation/authority-loss style facts.
- MIR cleanup consumes RIR flow/fact/semantic summaries for rollback and
  invalidation block decisions. The previous intent-step AST invalidation
  fallback is removed and gated out by `cfg-body-dataflow-test-smoke`.
- MIR validation now requires each reachable pin-region block to carry the
  matching `pin-unpin-cleanup-edge` fact for its source slot, view binding, and
  read/write mode. `test-mir` includes a negative corruption regression so this
  fact cannot silently become a backend convention again.
- MIR validation now requires every reachable non-cleanup block with a cleanup
  successor to also carry a materialized `cleanup-edge` MIR fact. Rollback and
  invalidation cleanup blocks must likewise carry their named cleanup-edge
  facts. This closes the field-vs-instruction drift seam: backend consumers can
  trust cleanup topology only when the explicit MIR fact inventory exists.
- MIR validation now also rejects cleanup blocks that carry normal CFG
  successors or pin-region state. Cleanup/rollback/invalidation blocks must stay
  on the exceptional cleanup chain rather than becoming normal body-flow blocks.
- MIR validation now also gates residual `MIR_INST_STMT` fallback through
  `mir_instruction_source_stmt_fallback_is_allowed(...)`. Non-intent semantic
  carriers must carry a source payload and `source_statement_index`, so
  residual statement emission cannot silently reopen raw source-array fallback.
  C and LLVM MIR block emitters consume the same helper before emitting
  residual source statements, keeping backend parity tied to the validator
  policy while CFG/body safety is being promoted to source-of-truth. `test-mir`
  includes `MIR validator rejects residual STMT without source inventory fact`;
  `cfg-body-dataflow-test-smoke` gates the policy owner, backend consumers, and
  regression string.
- Non-`Void` functions now consume the CFG body flow summary for all-path
  return. If any reachable normal path can fall through without a return
  terminator, semantic analysis emits `PGY_SEM_MISSING_RETURN` with `Reason:`
  and `Fix:`. The consumer now reads `SemanticBodyFlowSummary` through
  `semantic_check_body_flow_summary(...)`, and the diagnostic exposes the
  `fallthrough`, `return`, `break`, `continue`, and `defer` facts that drove the
  all-path-return decision.
- Semantic CFG body-flow flag consumption is centralized through named
  fallthrough/terminator helpers, so branch/join decisions are no longer
  repeated as open-coded flag masks in each consumer.
- Statements after direct terminators and after `if`/`match` bodies whose
  reachable paths all terminate now emit
  `PGY_SEM_UNREACHABLE_CODE` with `Reason:` and `Fix:` instead of being
  silently skipped by the body-flow walk.
- `QubitSlot` loop move/join now has source-level regression for break-exit
  consumption and continue-backedge consumed-resource detection.
- `defer` cleanup-body terminators are isolated from the surrounding CFG path:
  they do not make following statements unreachable and do not satisfy
  non-`Void` all-path return.
- `defer` cleanup-body resource facts are isolated by snapshot/restore:
  cleanup moves, releases, and cleanup-only loop terminators are checked without
  consuming the surrounding path's live resource state or outer loop flow.
- Dynamic-control `defer` rejection now consumes the CFG body-flow
  `FLOW_HAS_DEFER` fact. `if`/`match`/loop checks no longer reopen nested AST
  bodies through a separate pre-scan helper.
- The direct `type_check_statement()` fallback path delegates `defer` body
  checking to the same cleanup snapshot helper as CFG body flow, closing the
  previous split-brain semantic path.
- Async/select semantic body checking now consumes the CFG body-flow boundary:
  `AST_ASYNC_BLOCK` and `AST_SELECT_STMT` are explicit flow cases, and
  `type_checker_async_decl.c` uses `type_check_statement_flow_boundary(...)`
  for async statements, select case tails, recovery, and defaults.
- Raw `namespace Name { ... }` shells are now semantically traversed even when
  `semantic_analyze()` receives parser output before module-normalizer
  flattening. CFG body flow has an explicit `AST_NAMESPACE_DECL` case and the
  regression is covered by `test-semantic` plus
  `cfg-body-dataflow-test-smoke`.
- Resource snapshots now cover anchored slot state (`Slot<T>`, `SecureSlot<T>`,
  `DeviceSlot<T>`) as well as `QubitSlot` consumption facts. This closes the
  branch/join case where a release on a terminating branch used to leak into the
  reachable fallthrough path.
- CFG ownership snapshots now also track classifier-backed ownership boundary
  values (`subject` identity, borrow-tracked aggregates, movable resources, and
  anchored handles). `own subject` movement in terminating branches no longer
  poisons reachable fallthrough paths, fallthrough moves are joined as consumed,
  and parallel subject transfers participate in the same duplicate-consume
  conflict gate as slot resources.
- Parallel ownership snapshots now carry task-local `is_used` as well as
  consumed/released state. This closes the stable `ref` + `own` task-boundary
  conflict for ownership-bearing values: a task that borrows a subject cannot
  run in parallel with another task that consumes the same subject.
- Shared `ref` reads of the same ownership-bearing value across parallel tasks
  remain accepted. The beta contract is therefore explicit: shared `ref`/`ref`
  read boundaries are allowed, while `ref`/`own` and `own`/`own` task-boundary
  conflicts are rejected for the stable ownership subset.
- `spawn` direct named-call boundaries now reject borrowed `ref` parameters for
  ownership-bearing values (`subject`, borrow-tracked aggregate, movable
  resource, anchored handle). Copy-only `ref` arguments remain accepted, and
  the diagnostic uses `PGY_SEM_BORROW_ESCAPE` with `Reason:` / `Fix:` wording.
- Direct named `spawn` boundaries now also reject authority-bearing `Token<T>`
  parameters. This closes the stable beta token-transport rule across channel
  send/receive helpers, cancellation payloads, channel close, and spawn.
- Function types now carry first-stage interprocedural body summaries through
  `body_summary_mask`. The current seam records `may_return`, `may_escape_ref`,
  `moves_param`, `borrows_param`, `drops_resource`, `effects`,
  `requires_zone`, `spawns_task`, and `sends_channel`, giving later CFG/runtime
  propagation and backend parity work one stable fact surface instead of
  repeatedly rediscovering those facts from AST-shaped helpers. Direct function
  calls now consume callee summaries and propagate transitive caller-relevant
  facts while keeping callee-local `may_return` local to the callee.
  Direct function calls, method calls, and host calls also record
  declaration-known summary facts (`effects`, `requires_zone`, and `own/ref`
  parameter modes).
- Parser-accepted anonymous async spawn bodies (`spawn async () { ... }`) are
  explicit beta rejects until closure capture/lifetime analysis is closed. The
  stable beta surface is named `spawn Worker(args...)`, where parameters,
  effects, and ownership boundaries are checked through declarations.
- `parallel` task bodies now consume CFG/resource snapshots directly: task-local
  terminators stay local to the task, task resource moves/releases are joined
  after the parallel block, and duplicate cross-task consumption is rejected with
  `PGY_SEM_PARALLEL_SLOT_CONFLICT`. Blocking channel send of a movable resource
  in a parallel task is fixed to the same consume/join contract.
- Non-blocking/timeout channel receive for ownership-bearing payloads is
  explicitly rejected (`TryRecv(Channel<QubitSlot>)`,
  `TryRecv(Channel<Slot<Int>>)`, `RecvTimeout(Channel<Array<Int>>, t)`).
  The stable non-blocking receive surface is copy-only; movable, subject,
  boundary-value, anchored-handle, and token payloads must use blocking `<-`
  into a named binding or a plain projection/value channel.
- Timeout/status channel send surfaces now share the same explicit transport
  policy as `TrySend`: movable resources remain blocked on builtin send
  helpers, authority-bearing `Token<T>` is rejected, and blocking `ch <- value`
  stays the explicit ownership-transfer path for named resources.
- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are copy-only for beta.
  Ownership-bearing payload futures are explicitly rejected until task-boundary
  cleanup summaries can prove where movable/anchored/subject/token payloads are
  released or observed.
- `ChannelClose(Channel<T>)` is copy-only for beta. Closing a channel with
  ownership-bearing queued payloads would need a cleanup/backpressure summary,
  so movable, subject, boundary-value, anchored-handle, and token channels must
  be drained explicitly before close.
- Slot borrow-safety bridge facts are now named in both
  `docs/103_cfg_body_dataflow_need.md` and
  `docs/semantics/08_slot_capability_calculus.md`: `NoEscape(view, region)`,
  `NoSuspend(view, region)`, `WriteExclusive(slot, region)`,
  `DropOnce(owner, all_cfg_exits)`,
  `ReleaseAfterUnpin(slot, all_cfg_exits)`, and
  `NoUnsupportedTokenTransport(token, boundary)`.
- Existing `ViewRead(...)` / `ViewWrite(...)` semantic constructors and the
  source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` block now
  cover the first bridge slice: `ReadView<T>` return escape uses
  `PGY_SEM_PIN_ESCAPE`, active view + `await` uses
  `PGY_SEM_PIN_AWAIT_BOUNDARY`, active view + direct named `spawn`, `async`
  block, and event lambda callback registration use the same
  suspension-boundary diagnostic, active view + channel send/receive/close uses
  the same handoff-boundary diagnostic, active view + `Cancel(...)` uses the
  same cleanup-boundary diagnostic, active view + `defer` registration uses
  the same cleanup-boundary diagnostic,
  active/acquired view across
  `parallel` uses `PGY_SEM_PIN_PARALLEL_CONFLICT`, `QubitSlot` pin attempts
  use `PGY_SEM_PIN_QUBIT_REJECT`, and `WriteView<T>` exclusive access is
  covered in semantic regression plus `diagnostics-json-test-smoke`. Source
  pin typed-view read parity is covered for plain, secure, and sequential
  mixed slot cases by `pin_read_view_block`, `pin_secure_read_view_block`, and
  `pin_mixed_read_view_sequence`; typed-view write parity is covered by
  `pin_write_view_block` and `pin_secure_write_view_block`; cleanup-edge
  parity is now fixed for normal successor exit, direct return inside a pin
  block, branch-to-return exit, and loop `break`/`continue` exit by
  `pin_successor_cleanup_block`, `pin_return_value_block`,
  `pin_branch_return_block`, `pin_continue_cleanup_block`, and
  `pin_break_cleanup_block`. Secure boundary-slot parameter pinning is covered
  by `pin_secure_param_read_view_block`.

Remaining:

- Richer reachability provenance across nested/exceptional CFG edges, the
  general branch/join assignment lattice beyond the current sealed local-`let`
  surface and longer-lived borrow lifetime beyond the current task-local
  borrow/use snapshot baseline, full drop/cleanup insertion and validation
  beyond current `defer` isolation, zone/effect transition, projection
  freshness, broader channel receive/backpressure, and richer cancellation
  task-boundary checks must consume CFG/dataflow facts directly. Anchored slot branch-join,
  `own subject` branch-join, parallel resource/boundary consume state, and
  parallel `ref`+`own` boundary conflicts, plus direct named-call `spawn ref`
  boundary rejection, anonymous async spawn explicit reject, timeout/status
  channel-send transport rejection, non-blocking ownership-bearing receive
  explicit reject, copy-only cancellation payload reject, and copy-only channel
  close are already covered by regression and remain as closed baseline
  evidence.
- Full mutable-borrow overlap is not a beta blocker because beta has no
  `mut ref`/`ref mut` surface. If such a surface is introduced after beta, it
  must be added as a new CFG lattice fact instead of being inferred from current
  `ref` parameters.
- Interprocedural body summaries must be fixed: `may_return`, `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`, `effects`, `requires_zone`, `spawns_task`, and `sends_channel`.
  First-stage `body_summary_mask` storage and semantic recording exist now; the
  direct function-call consumer and method declaration-summary consumer also
  exist. Lambda body checking is isolated now: lambda-local effect/body facts
  are stored on the lambda function type and do not leak into the enclosing
  routine before the lambda is called; function-typed lambda bindings propagate
  those facts through the same callee-summary path as named functions. The
  remaining blocker is making zone/effect/runtime/codegen consumers use those
  bits instead of local rediscovery.
- Diagnostics must report path provenance with branch/join edge, previous state, `Reason`, and `Fix`.
- C and LLVM must lower the frozen subset from the same CFG/dataflow facts and be covered by backend compare.
- Option C ownership lift now has the block-scoped
  `pin slot as view: ReadView<T>|WriteView<T> { ... }` parser/semantic surface.
  The source-level block desugars to the same typed-view semantic slice; that
  slice has C/LLVM read/write parity for plain and secure slot cases, including
  a sequential mixed read case. C source-block cleanup and C/LLVM MIR
  successor/return cleanup now emit explicit typed pin/unpin calls for the
  frozen pin backend-compare fixtures, including normal successor exit, direct
  return, conditional branch-to-return exit, and loop `break`/`continue` exit.
  Active view + `defer` registration is rejected semantically. The remaining
  runtime pin-block closure is broader exceptional/cancellation
  exit coverage plus the `DropOnce` / `ReleaseAfterUnpin` theorem row.

Evidence command:

```sh
make cfg-body-dataflow-test-smoke
make ir-pipeline-test-smoke
make test-semantic
make llvm-test-backend-compare
make llvm-campaign-projection-test-smoke
make llvm-dnd-campaign-test-smoke
```

Step skeleton:

1. CFG fact inventory gate: `cfg-body-dataflow-test-smoke` keeps HIR CFG, HIR dom, RIR flow-block, and MIR cleanup/SSA facts visible.
2. Semantic control-flow gate: all-path return and reachability use CFG facts;
   local delayed initialization stays sealed by `let = initializer`, while any
   future wider assignment surface needs explicit CFG lattice facts.
3. Ownership gate: move/use-after-move, borrow lifetime, mutable borrow overlap, and drop/cleanup use CFG join facts.
4. Orchestration gate: zone/effect/relation/projection/handoff facts use body summaries at branch/join.
5. Execution gate: `parallel`, task, cancellation, and channel boundaries use interprocedural body summaries.
6. Backend gate: C and LLVM consume the same frozen facts and backend-compare covers representative cases.

## 0c. Core Language Semantic Closure

Status: `BLOCKER`

Goal:

- The beta stable subset must be defined in one place, not inferred from scattered README/TODO/status notes.
- Intent, zone/world/authority/handoff, and projection freshness are core language semantics, not library polish.
- This section is the checklist entry for the formal proof documents under `docs/semantics/01_intent_world_zone.md` and the stable subset contract.
- Stable subset source of truth: `docs/107_beta_stable_subset.md`.

Stable subset that must be frozen:

- Core declarations: `subject`, `zone`, `world`, `intent`, `relation`, `effect`, `projection`, `authority`, `handoff`.
- Core execution: `func`, `let`, `if`, `match`, `for`, `while`, `return`, `parallel`, stable channel/task baseline.
- Generic contracts: exact type parameters, ability bounds, multi-bound `where T: A + B`, implemented default type argument resolution.
- Ownership: anchored slot-handle boundary subset, boundary-visible aggregate provenance, copy-value trivial `own/ref`, explicit reject for `Token<T>` transport.
- Collections: `Array<T>`, local borrowed `Slice<T>` with `SliceCopy`, `List<T>`,
  `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, plus any currently
  implemented additional key families only if docs/tests/backend parity list
  them explicitly.
- Observability: `last`, `history`, `active`, `recent` baseline.
- Option C ownership lift decision: a generic param ownership classifier is a
  beta blocker if ownership-sensitive generic code is accepted. Without that
  classifier, generic `own/ref` uses that depend on unknown `T` ownership class
  must be explicitly rejected instead of inferred.
- Current conservative baseline: unresolved `TYPE_KIND_GENERIC` classifies as
  `BORROW_TRACKED`, so generic `own/ref` boundaries are rejected unless a later
  classifier can prove a stable ownership class for `T`.
- Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable for
  `Int`, `Long`, `Float`, `Double`, `Bool`, and `String`. The closed contract
  includes resolver metadata, semantic builtin typing, C runtime, LLVM runtime
  exports, C emitter lowering, LLVM builtin lowering parity, ABI layout tests,
  and lifecycle backend-compare regression.
  Fractional numeric literals infer as `Float`; `Rc<Double>` is stable through
  explicit `Double`-typed values or annotations, not a separate double-literal
  surface.
  Payloads outside that set are explicitly rejected in semantic analysis before
  backend lowering.
  Gate phrase: shared ownership stable subset requires C/LLVM lifecycle parity.
  `Arc<T>`, cross-thread shared ownership, generic/object payloads beyond the
  frozen primitive/String set, and default ARC remain outside the beta contract.

Intent closure:

- Intent step ordering must be deterministic and backend-independent.
- Compensation, rollback, cancellation, and invalidation paths must have a formal meaning and an ABI smoke surface.
- Intent effect propagation must use the same runtime provenance vocabulary as zone/effect/projection propagation.
- Intent observability ABI fields and trace order must be versioned or explicitly frozen for beta.

Zone/world/authority/handoff closure:

- Zone generation and world embedding must define ownership and handoff behavior.
- Handoff frontier recompute must define pass limit, stale-read behavior, and hard-fail boundary.
- Projection freshness must state when `refresh`, `publish`, and `bind` make data visible.
- Authority rejection must expose queryable recoverable state for beta-stable recoverable failures.

Runtime frontier scheduler closure:

- The current beta evidence covers world derived-state bounded recompute, zone lifecycle bounded frontier loop,
  projection-chain bounded recompute, embedded world-zone projection freshness,
  embedded world-zone action-caused layer/state freshness, v1 handoff
  projection/world/layer-state freshness, and the authority/failure handoff queryable baseline.
- `make runtime-frontier-contract-test-smoke` gates that C and LLVM both keep
  bounded frontier loops, pass limits, and hard-fail overflow boundaries for the
  covered world/zone/projection slices, and that authority rejection remains a
  recoverable queryable failure surface (`last_ok / zone / participant / code /
  reason`) with C/LLVM parity cases. The same gate now requires the C and LLVM
  emitters to consume `src/codegen/domain_frontier_policy.h` for frontier
  pass-limit formulas instead of reintroducing helper-local constants.
- 2026-04-29 update: the stable world outer frontier now consumes the named
  `pgy_frontier_world_transitive_pass_limit(...)` policy in both C and LLVM.
  This makes the world zone-sync plus derived-state recompute family a shared
  source-of-truth contract instead of two backend-local helper choices.
- 2026-05-04 update: the transitive world frontier pass limit now includes the
  embedded zone frontier budget (`zone.state_count + zone.layer_slot_count`)
  in addition to world zone/state counts. The C and LLVM world frontier
  emitters compute that budget from active zone declarations and pass it to the
  same runtime policy helper, so embedded world-zone propagation no longer uses
  only the outer world shape as its bounded-fixpoint budget. The frontier
  contract smoke now emits the existing embedded-world action fixture and
  rejects the old outer-only generated limit. The embedded budget loop itself
  now lives in `pgy_domain_world_embedded_frontier_count(...)` under the shared
  codegen frontier policy wrapper; C and LLVM only provide backend-local zone
  lookup callbacks.
- 2026-05-04 update: zone, projection, world-transitive, and world-derived
  pass-limit selection now goes through the named `pgy_domain_*_frontier_*`
  wrappers in `src/codegen/domain_frontier_policy.h`. C and LLVM backend
  call sites no longer pick runtime frontier formulas directly, so the wrapper
  is the single codegen source of truth for bounded-frontier policy vocabulary.
- 2026-04-29 update: frontier pass-limit formulas now saturate through the same
  u32-bounded helper family before emission. This keeps C `size_t` loops and
  LLVM i32 loop counters on the same bounded contract for oversized generated
  frontier families.
- 2026-04-29 update: `make runtime-frontier-policy-test-smoke` compiles and
  executes the `src/codegen/domain_frontier_policy.h` arithmetic directly. This
  keeps the frontier policy gate from being only a string-contract check.
- 2026-05-02 update: frontier pass-limit policy moved to the runtime contract
  owner (`src/runtime/pgy_frontier_policy.h`), with the codegen header kept as a
  compatibility wrapper. The C and LLVM world emitters now also preserve a
  separate "derived state changed in this pass" fact, so a converged derived
  loop still feeds the outer transitive frontier once before dirty flags are
  cleared.
- 2026-05-13 update: bounded frontier overflow reason strings moved to the
  same runtime contract owner. C and LLVM frontier emitters now consume
  `PGY_FRONTIER_REASON_*` constants instead of hard-coding zone/world/projection
  overflow text locally; `runtime-frontier-contract-test-smoke` gates this.
  AIR runtime frontier policy evidence now counts both the 9 pass-limit facts
  and the 5 overflow-reason facts, so `pgy.air.graph.v1` publishes the same
  runtime policy surface that codegen consumes. The JSON dump exposes both
  sub-counts and the total count so CI/LSP consumers can detect which policy
  family drifted.
- 2026-05-13 update: `runtime-frontier-contract-test-smoke` now also rejects
  direct C/LLVM codegen calls to runtime `pgy_frontier_*_pass_limit(...)`
  helpers outside `src/codegen/domain_frontier_policy.{h,c}`. The runtime
  header remains the arithmetic owner, but the backend-facing source of truth
  is the codegen wrapper so emitter-local domain lookup cannot bypass the
  shared frontier policy seam.
- Remaining blocker: the full bounded fixpoint / transitive frontier scheduler
  must broaden that same transitive frontier policy beyond the currently
  covered world/zone/projection slices and embedded zone frontier budget so the
  broader world-zone propagation family cannot grow helper-specific edge
  policies.

Evidence command:

```sh
make formal-semantics-test-smoke
make runtime-authority-contract-test-smoke
make runtime-frontier-contract-test-smoke
make runtime-frontier-policy-test-smoke
make projection-diagnostic-contract-test-smoke
make llvm-test-backend-compare
```

## 0d. Runtime Panic And Secure Authority Invariants

Status: `BLOCKER`

Goal:

- Runtime failure behavior must be the same on C and LLVM for the frozen subset.
- Security-bearing surfaces must have explicit invariants before the language claims security semantics.

Runtime panic/unwinding policy that must be frozen:

- OOM
- divide-by-zero
- array/slice/list/map out-of-bounds
- released slot use
- double release
- released or double-released device slot use
- invalid secure-slot token
- authority token mismatch
- internal compiler/runtime invariant break

Implementation progress:

- `src/runtime/pgy_runtime_panic_contract.h` owns the panic class vocabulary and
  shared `PGY_RUNTIME_PANIC` emitter.
- Inline runtime `PGY_PANIC` delegates to the shared panic contract.
- LLVM exported typed slot read/write now hard-fails on released-slot access
  instead of logging and returning a default value.
- LLVM exported secure slot read/write/release now hard-fails on released secure
  slot, invalid token, and denied token capability.
- Inline and LLVM exported device slot read/write/release now hard-fails on
  released or double-released device slots instead of silently no-oping or
  returning a default value.
- Generated C and LLVM `Array<T>`/`Slice<T>` indexing, including temporary
  function-return access (`Words()[0]`, `Words().Slice(...)[0]`), and
  `ArraySet` now lower through checked runtime helpers, so stable collection
  out-of-bounds reaches the shared `out-of-bounds` panic class instead of
  direct memory access.
- Stable value-demanding collection APIs now share the same hard-fail policy:
  `ListGet` out-of-range, `QueuePop` on an empty queue, and `MapGet` on a
  missing key panic with `out-of-bounds` in generated C and LLVM. Recoverable
  absence checks stay on `ListSize`, `QueueEmpty`, and `MapHas`.
- Stable mutation collection APIs no longer silently no-op on invalid targets:
  `ListSet`, `ListRemove`, and `MapRemove` invalid access panic with
  `out-of-bounds` in generated C and LLVM.
- Stable unwrap misuse no longer drifts between backends: `Unwrap(Err)`,
  `Fail()?` in a `Void` function, and `UnwrapOption(None)` panic with
  `internal-invariant` in generated C and LLVM.
- `docs/105_runtime_panic_contract.md` records the runtime panic contract and
  the stable collection access/mutation hard-fail split.
- `runtime-panic-abi-test-smoke` executes inline and exported runtime hanesses
  for released-slot, invalid-secure-token, double-release, device-slot,
  authority-mismatch, OOM, and divide-by-zero panic classes.
- Authority token mismatch now records a stable recoverable query state before
  hard-fail use: code `authority-token-mismatch`, reason `zone authority
  validation failed: authority token mismatch`, and stderr without secret token
  material. `authority_failure_abi` and `authority_failure_surface` keep the
  C/LLVM ABI and backend outputs aligned.
- Authority-bearing `Token<T>` transport is explicitly rejected on the current
  beta transport surfaces: blocking channel send/receive, non-blocking/timeout
  channel helpers, channel close, cancellation payloads, and direct named
  `spawn` boundaries.

Required policy decision:

- Recoverable user/runtime contract failures expose `Bool`, `Result<T>`, or queryable runtime state.
- Contract violations at ownership/security boundaries are hard-fail unless explicitly modeled as recoverable.
- Intenal compiler/runtime invariant breaks are hard-fail.
- No beta path may silently fallback to a different backend behavior.

Secure slot / authority invariant obligations:

- Secure tokens are unforgeable by source-level code.
- Secure slot token mismatch cannot read or write the protected slot.
- Authority-bearing tokens cannot be copied into an untrusted boundary or transported through unsupported channels.
- Zone authority transfer cannot create two active owners for one authority boundary.
- Runtime snapshots must not expose secret token material.

Evidence command:

```sh
make runtime-authority-contract-test-smoke
make runtime-panic-contract-test-smoke
make runtime-panic-abi-test-smoke
make runtime-panic-codegen-test-smoke
make runtime-abi-lifetime-test-smoke
make backend-compare-llvm-coverage-test-smoke
make llvm-test-backend-compare
```

## 0e. User-Facing Beta Quality Gates

Status: `IN PROGRESS / BLOCKER`

Goal:

- Beta must be honest about platform support, diagnostics, stdlib API stability, tooling stability, and performance regression limits.

Diagnostic quality gate:

- Every user-facing parser, lexer, semantic, backend, and runtime error must have severity, stable code, source span when available, `Reason:`, and `Fix:`.
- `diagnostic-registry-test-smoke` verifies code registry drift, but beta also requires representative quality checks for parser/lexer/backend/runtime messages.
- The diagnostic registry gate checks `PGY_CODE_*` documentation even on
  minimal CI images without Python, including multiline macro definitions.
- Parser and lexer JSON routing now preserves `stage`, `code`, `cause_ir`, and
  `fix_source` for `PGY_PARSE_SYNTAX` and `PGY_LEX_INVALID_TOKEN`; remaining
  debt is richer parser code splitting and parser multi-error accumulation.
- Intent clause explicit rejects for control-transfer constructs now preserve
  source span through parser AST nodes. `make diagnostics-json-test-smoke`
  covers `PGY_SEM_INTENT_STEP_INVALID` for `on: spawn ...` and
  `on: ch <- value` with line/column, `cause_ir`, and `fix_source`.
- `diagnostics-json-test-smoke` is no longer Python-vacuous on Windows/Git Bash
  fallback paths: without Python it still validates JSON-array shape, required
  stable literals, and the success-path empty `[]` contract. This is the
  surface the first soft self-host Diagnostic Catalog Checker must consume.

Cross-platform support matrix:

- Linux/WSL native: required beta gate for C + LLVM.
- Windows native/MSYS2/MinGW: required support matrix entry; LLVM may be marked unsupported until a real runner is green.
- macOS: C-only CI preflight is required through `make ci-macos`; macOS LLVM/backend parity remains out-of-beta until a dedicated LLVM support contract is green.
- 2026-04-25 local check: `make ci-windows` was not runnable in this WSL/Linux shell because `gcc -dumpmachine` reports `x86_64-linux-gnu`, not MSYS2/MinGW. This is an environment gap, not a code green signal; Windows beta evidence still requires a real MSYS2/MinGW runner.
- 2026-04-26 support-matrix guard: `WINDOWS_LLVM_READY` now requires executable `llvm-config --libs core` evidence. A `C:/Program Files/LLVM/lib` directory alone is not accepted as Windows LLVM support because it can make MSYS2 CI run LLVM smoke/backend-compare without runnable LLVM tooling and fail with command-not-found status 127.
- 2026-04-26 release wording: macOS now has a C-only CI preflight (`make ci-macos`) while macOS LLVM/backend parity remains out-of-beta.

Stdlib beta freeze:

- Source of truth: `docs/108_stdlib_beta_freeze.md`.
- The stable stdlib API list identifies beta-stable builtin helpers, stable
  `use` modules, known experimental modules, and out-of-beta ecosystem work.
- `make stdlib-test-smoke` gates stable builtin stdlib behavior and stable
  `use` module behavior on C and LLVM when both backends are requested.
- `type_checker_stdlib_use.c` must stay aligned with the public freeze list.

Tooling beta conformance:

- Stable tooling subset is executable through `make tooling-conformance-test-smoke`.
- LSP beta-stable: initialize capability response, keyword hover, and keyword completion.
- Formatter beta-stable: `--check` detects drift, `--write` is idempotent, and formatted code compiles.
- Debugger beta-stable: CLI `pgy debug <file>` parse + semantic gate and interactive quit path.
- Out-of-beta: DAP, binary breakpoints, variable watch, multi-file workspace indexing, refactor edits, full editor-grade diagnostic streaming.

Package/module resolver beta surface:

- Source of truth: `docs/109_package_module_resolver_contract.md`.
- Stable module surface is `import "relative/path.pgy";`, resolved relative
  to the importing file with namespace/export visibility and circular import
  rejection.
- Stable package surface is only `pgy init <name>` manifest/project
  scaffolding.
- `pgy install`, dependency version solving, lockfiles, registries, remote
  imports, checksum/signature verification, and supply-chain integrity are
  explicitly out-of-beta.
- `make package-module-resolver-test-smoke` gates the doc contract, `pgy init`,
  explicit `pgy install` rejection, and JSON diagnostics for module-load
  failures.

Test quality gate:

- Source of truth: `docs/111_beta_test_suite_freeze.md`.
- `make beta-test-suite-freeze-test-smoke` checks that the mandatory pre-beta
  gates have a stable target and remain listed in the freeze doc.
- Fuzz/property tests remain out-of-beta until they have a seed corpus,
  minimization policy, and proof-pack property mapping.
- Coverage percentage is not yet a beta acceptance metric; named stable-surface
  coverage is the beta gate.

Performance gate:

- Compile/runtime perf baselines must be captured before major CFG/DAG/runtime propagation rewrites.
- Regressions beyond the chosen threshold must block beta unless explicitly waived.
- `make perf-contract-test-smoke` gates the `perf_summary` log grammar and C/LLVM average/worst-case summary output so `test-abi-perf` evidence remains machine-readable.
- `make perf-c-baseline-test-smoke` compares one stable arithmetic-loop fixture
  against hand-written native C. The gate checks output equality and records
  `pgy_over_c_ratio`; it does not claim Pergyra is faster than C. The honest
  baseline is near-C with run-to-run noise, and CI output is the source of truth
  for the active ratio. Local native Windows spot-checks can run
  `tests/perf_c_baseline_smoke.ps1`. The same gate also rejects regressions
  where `i % 97` or `i / 97` lower through checked div/mod helpers; constant
  nonzero integer divisors/moduli must emit direct arithmetic in both C and LLVM
  lowering because divide-by-zero panic is statically impossible. The shared
  source of truth is `codegen_scalar_arithmetic_policy.c`, not separate C/LLVM
  predicates.
- `make tooling-conformance-test-smoke` gates the tested formatter/LSP/debugger beta subset so tool maturity is not inferred from binaries merely existing.

Observability/tracing schema gate:

- Source of truth: `docs/112_observability_trace_schema.md`.
- Stable schema is intentionally narrow: `IntentLast*`, `IntentHistory*`,
  `IntentActive*`, `IntentRecent*`, authority failure snapshot
  (`ok/zone/participant/code/reason`), and backend-identical trace strings.
- Runtime string exports are `runtime-borrowed string` values: callers must not
  free them, and values are valid until the next registry/snapshot mutation.
- Rich event streaming, structured JSON trace export, distributed trace
  correlation, user-code registry hooks, stable binary trace format, and richer
  multi-instance timeline queries are explicitly out-of-beta.
- `make observability-schema-test-smoke` gates the C/LLVM stable schema
  fixtures.

Docs freeze:

- Language reference, getting-started tutorial, and migration/release notes must describe the frozen beta subset without overclaiming future surfaces.
- Documentation quality audit: `docs/116_documentation_quality_audit.md`
  records stale-path, mojibake, and async wording risks. User-facing docs should
  prefer 100-series source-of-truth contracts over older alpha-era design notes.

Memory/concurrency model gate:

- Source of truth: `docs/113_memory_concurrency_model.md`.
- Async positioning rationale: `docs/114_async_model_positioning.md`.
- Beta promise: Pergyra keeps suspension visibility but decomposes coloring;
  `await` is a completion join only, and `Future<T>` / `RemoteFuture<T>` are
  typed completion handles rather than a general user-level effect system.
- Stable contract: `parallel` joins before following control flow, accepted
  writes become visible after join, shared `ref`/`ref` reads are allowed, and
  `ref`/`own` plus `own`/`own` task-boundary conflicts are rejected.
- Stable channel contract: blocking send/receive is the ownership-transfer path;
  non-blocking/timeout receive, status send helpers, cancellation payloads, and
  channel close are copy-only for beta.
- Anonymous async spawn bodies, full weak-memory vocabulary, user-selectable
  memory orders, scheduler fairness guarantees, lock-free correctness claims,
  capture-bearing detached async block stability, and cross-thread `Arc<T>` /
  `Send` / `Sync` style trait systems are explicitly out-of-beta.
- `make memory-concurrency-model-test-smoke` gates the contract with
  `make async-model-positioning-test-smoke`,
  `parallel-core-contract-test-smoke`, and targeted C/LLVM backend compare for
  `parallel_channel_sum`,
  `parallel_channel_dual`, and `triple_paradigm`.

String/unicode policy:

- Source of truth: `docs/110_string_unicode_policy.md`.
- Beta-stable string policy is UTF-8 payload preservation in string literals and
  generated C/LLVM output.
- `StringLength` is byte-length for beta; equality/search are byte-exact and
  normalization-blind.
- Unicode identifiers, Unicode normalization, locale-sensitive comparison,
  case folding, collation, grapheme iteration, display width, and mixed-encoding
  source files are explicitly out-of-beta.
- `make unicode-policy-test-smoke` gates C/LLVM UTF-8 string execution and
  explicit Unicode identifier rejection.

Evidence command:

```sh
make diagnostic-registry-test-smoke
make parser-lexer-diagnostic-test-smoke
make module-taxonomy-test-smoke
make package-module-resolver-test-smoke
make unicode-policy-test-smoke
make beta-test-suite-freeze-test-smoke
make observability-schema-test-smoke
make memory-concurrency-model-test-smoke
make parallel-core-contract-test-smoke
make documentation-quality-test-smoke
```

## 0f. AIR Abstraction Safety Closure

Status: `BLOCKER`

Source of truth: `docs/104_air_compiler_architecture.md`

Goal:

- Pergyra의 killer 기능인 abstraction safety (intent ↔ implementation drift 검출) 에 명시적 verification IR 을 가진다.
- AST 기반 traversal 에 ownership 을 분산시켰다 사고친 패턴을 abstraction safety 도메인에서 반복하지 않는다. 분산된 metadata + cross-IR query 가 아니라 **단일 source of truth (AIR) + read-only synthesis** 로 푼다.
- 베타 후 ~1년간 코어 패치 freeze 가 예정되어 있으므로 AIR Phase 1 은 **문서 합의가 아니라 실 구현 완료 + 회귀 smoke 통과** 까지 닫는다.
- AIR 는 codegen path 위가 아니라 옆에 위치하는 **verification-only synthesis IR** 이다 (HIR + DIR + RIR → AIR, 단방향 read-only). codegen 출력에 영향이 없으므로 stale 위험이 codegen IR 보다 작다.
- 1.0 기준에서 AIR는 Pergyra의 abstraction-safety closure layer다. 단,
  타입/DAG, CFG/body safety, ownership, MIR cleanup, runtime propagation을
  대신하지 않는다. 각 layer가 자기 evidence를 만들고 AIR는 그 evidence가
  intent/zone/world/effect/IO/parallel/event/pin 계약과 일치하는지 감사한다.

Closed now:

- AIR 컴파일러 아키텍처 결정과 단방향 synthesis IR 포지셔닝이 `docs/104_air_compiler_architecture.md` 에 고정됐다.
- AIR 가 Rust MIR 과 의식적으로 다른 위치 (codegen path 옆) 에 산다는 architectural choice 가 명시됐다.
- Phase 1 / 2 / 3 scope 가 명시적으로 분리됐고, Phase 1 은 Intent Node + Boundary Node + 1 개 drift check 로 좁혀졌다.
- 1.0 AIR blueprint가 문서화됐다: Phase 1 beta는 `IntentNode` /
  `BoundaryNode` / strict evidence / drift facts를 닫고, 1.0은
  `EvidenceNode`를 1급화해 HIR CFG, DIR, RIR, MIR cleanup/pin, DAG
  generic/ability/module facts를 cross-layer로 감사한다.
- AIR 가 아닌 것 (codegen IR 아님, ownership/borrow 검사 home 아님, type 검사 home 아님, effect propagation 자체 아님, 새 keyword 추가 안 함) 이 명시적 negative space 로 docs 에 고정됐다.
- CFG 사고 (AST 기반 ownership 분산) 와의 동형 비교가 docs 에 고정되어, 같은 함정에 빠지지 않는 이유가 추적 가능하다.
- `src/compiler/air.h` defines the AIR Phase 1 data model,
  `src/compiler/air.c` owns DIR-based read-only synthesis, and
  `src/compiler/air_verify.c` owns global AIR validation plus sync/async drift
  and strict evidence diagnostics.
- AIR synthesis가 HIR routine/CFG, RIR boundary/authority/effect, MIR cleanup/pin/terminator, DAG generic/ability, runtime frontier/schema evidence를 read-only로 수집하고 각 `Boundary Node`에 evidence flag를 부착한다. Default strict evidence에서 누락된 required layered evidence는 `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`로 hard-fail 된다.
- 2026-04-29 AIR HIR provenance split: AIR boundary evidence now records
  `has_hir_routine_evidence` separately from `has_hir_cfg_evidence`. A lowered
  intent routine summary can still prove routine provenance, but only a routine
  with generated CFG containing the same boundary AST increments
  `hir_cfg_evidence_count` when a boundary AST is available. This closes the
  routine-only-vs-CFG-backed wording drift without changing public syntax.
- 2026-05-02 update: AIR evidence policy is exposed through
  `air_boundary_requires_hir_evidence(...)`,
  `air_boundary_requires_rir_evidence(...)`, and
  `air_boundary_has_evidence(...)`. Driver diagnostics and AIR graph dumps now
  consume the evidence inventory first, with legacy per-boundary flags retained
  only as compatibility summaries when no inventory exists.
- 2026-04-29 HIR evidence tightening: `HIR_TOPLEVEL_INTENT` no longer grants
  blanket HIR evidence to every AIR boundary. HIR evidence must match the
  intent owner, step, or boundary source name. `test_air` now locks the negative
  case where an unmatched top-level intent routine is present but an
  implementation boundary still reports missing HIR CFG evidence.
- AIR read-only evidence is now regression-backed at the owner/evidence seam:
  `src/test_air.c` snapshots representative DIR step fields, HIR routine fields,
  and RIR scope/op/fact fields across `air_synthesize(...)`.
- `src/test_air.c`가 direct AIR 케이스와 parser/semantic/DIR/HIR/RIR source integration 케이스를 함께 고정한다: sync intent + sync boundary pass / sync intent + async boundary drift / async intent + async boundary pass / strict missing-boundary drift / mismatched authority participant drift / HIR+RIR evidence collection / parsed intent source no-drift.
- AIR synthesis now scans stable intent-step execution clauses (`using`,
  `intent`, `pre`, `guard`, `post`, `invariant`, `expect`, `on`,
  `compensate`) for `spawn` / `async` / `parallel`, `channel` / `select`, and
  stable resource IO/time calls. The current stable AIR boundary set is
  `FileOpen`, `FileExists`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`,
  `WriteFile`, `Input`, `ReadLine`, `Now`, and `Sleep`. `Print` / `Log*` remain
  observability output calls, not AIR resource-boundary evidence in Phase 1.
  This is a codegen outputter-owner split, not the AIR/RIR resource-boundary inputter
  set: `Print` and `Log*` remain observability outputter artifact calls and are
  explicitly excluded from `io_boundary_builtin.c`.
  `src/test_air.c` covers AST-backed spawn boundary drift, IO `either`
  boundary non-drift, the stable execution boundary set (`parallel`, `async`,
  `channel-send`, `channel-recv`, `select`), and the full stable boundary
  builtin set so semantic builtin growth cannot silently bypass AIR.
- AIR drift messages are owned by AIR and `air_check_drift()` clears existing
  drift messages before recomputing; `src/test_air.c` covers repeated drift
  checking on the same AIRProgram so the validation path does not leak or retain
  stale diagnostics.
- AIR synthesis now hard-fails if the precomputed intent/boundary node counts
  diverge from the actual append counts. `make air-drift-test-smoke` gates the
  invariant so future boundary scanners cannot silently underfill or overrun the
  AIR node inventory.
- `air_verify(...)` is now the global AIR validation entry point. It validates
  AIR inventory invariants, authority participant shape, and evidence
  provenance before computing drift/evidence failures. `air_check_drift(...)`
  remains only as a compatibility wrapper.
- AIR inventory validation now rejects non-zero intent/boundary/drift counts
  without matching arrays and rejects boundary step-index drift from the
  referenced intent node before recomputing drift facts. `src/test_air.c`
  covers both crash-prevention paths.
- AIR inventory validation also rejects empty intent owner/step names, empty
  boundary owner/source names, boundary-owner mismatch against the referenced
  intent owner, and invalid boundary sync-class shape (`world`, `parallel`, and
  `channel` async; IO either-sync) before drift computation. These are
  `PGY_AIR_INVARIANT_INVALID` compiler IR failures, not user-facing drift facts.
- AIR drift inventory is validated before recomputation: stale drift nodes with
  placeholder kind, invalid intent/boundary references, or empty messages are
  rejected as `PGY_AIR_INVARIANT_INVALID` instead of being silently cleared.
- AIR evidence validation now rejects RIR authority evidence without prior RIR
  boundary evidence and rejects authority evidence on a non-authority boundary,
  keeping evidence provenance as a layered proof instead of a boolean flag.
- AIR evidence-node validation now also rejects mismatched boundary shapes:
  global evidence attached to a concrete boundary, HIR CFG evidence without
  same-boundary HIR routine evidence, undeclared authority subjects, and MIR pin
  cleanup evidence attached to a non-pin boundary.
- AIR evidence-node validation now also accepts and validates the global
  observability schema evidence node. This keeps `pgy.intent.observability.v1`
  and `pgy.intent.trace.v1` tied to AIR evidence inventory, not only JSON
  presentation.
- AIR inventory validation failures are now routed as
  `PGY_AIR_INVARIANT_INVALID` / `air:invariant:invalid` /
  `report-compiler-bug`, separate from user-facing
  `PGY_SEM_INTENT_BOUNDARY_*` drift diagnostics.
- AIR now owns synthesized intent/boundary/authority names instead of borrowing
  DIR/AST strings. Parsed-source AIR remains valid after DIR/parser teardown,
  and the parsed `where + transfer` regression asserts `PaymentZone` zone source
  plus `payment` world-handoff source.
- AIR expression-derived boundary nodes keep the expression span when available
  and fall back to the enclosing intent-step AST span when parser call nodes have
  no location. Parsed IO boundary regression ties the missing-evidence drift to
  the synthesized `ReadFile` boundary node instead of only checking that some
  drift exists.
- AIR dump ownership is split by consumer: `src/compiler/air_dump.c` owns
  human-readable debug output, `src/compiler/air_dump_json.c` owns the stable
  `pgy.air.graph.v1` JSON graph, and AIR validation/drift ownership now lives in
  `src/compiler/air_verify.c`. This keeps `src/compiler/air.c` below the 600
  LOC split-review threshold while keeping synthesis behavior focused.
- `where + transfer` no longer collapses to only a zone boundary. AIR emits a
  zone boundary for `where: Type` and a separate world boundary for the transfer
  handoff, with the world source anchored to the transfer target alias when
  present.
- World boundary evidence is now source/op-specific. A matching RIR intent
  scope alone does not discharge a transfer boundary; AIR requires RIR `Move` or
  `Claim` evidence for the boundary source alias.
- Implementation boundary evidence now requires HIR CFG proof. `parallel`,
  `channel`, IO, and execution boundaries cannot be discharged by RIR evidence
  alone; `src/test_air.c` gates this with the `AIR strict evidence requires HIR
  for implementation boundary` regression.
- HIR proof matching is source-specific; a top-level intent HIR routine is not
  accepted unless it matches the AIR owner/step/source identity.
- `tests/diagnostics_json_smoke.sh` now includes a parsed-source AIR negative
  case: a valid semantic intent requiring `authorized by: buyer` without a
  lowering-visible zone authority declaration is rejected by default strict AIR
  as `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`, with JSON
  `cause_ir`/`fix_source` and `expected authority participant(s): buyer`.
- `tests/diagnostics_json_smoke.sh` also covers a parsed-source execution
  boundary negative: an intent step calling `ReadFile(...)` is rejected by
  default strict AIR until AIR/RIR synthesis exposes real IO boundary evidence.
  This prevents owner-name-only RIR scope matching from falsely satisfying
  expression boundary evidence.
- Intent step source locations now flow parser → DIR → AIR, so AIR driver
  diagnostics for parsed sources report the offending step span instead of
  falling back to `line 0, column 0`.
- `docs/semantics/07_air_abstraction_safety.md`가 AIR synthesis read-only, Intent Node coverage, Boundary Closure, Drift Detection Soundness, Codegen Non-Impact proof obligation을 고정한다.
- `src/compiler/driver_app.c`가 AIR를 MIR lowering 전에 semantic-validation 단계로 실행하고, drift 발생 시 `PGY_SEM_INTENT_BOUNDARY_DRIFT` + `PGY_CAUSE_INTENT_BOUNDARY_DRIFT` + `PGY_FIX_ALIGN_INTENT_BOUNDARY_SYNC`를 text/JSON diagnostic으로 노출한다.
- `CompilerIRBundle`은 AIR를 담지 않는다. C / LLVM backend 가 AIR를 consume하지 못하게 막는 것이 Phase 1 설계다.
- `make air-drift-test-smoke`가 AIR source-of-truth 문서, checklist section, TODO readiness gate, Makefile wiring, AIR implementation/test/driver validation presence를 함께 검사한다.
- `make air-backend-nonimpact-test-smoke` compares generated C and LLVM text for
  the intent/zone, cross-world transfer, handoff frontier, world projection,
  relation/effect propagation, and authority-failure fixture set with relaxed AIR
  (`PGY_AIR_STRICT_EVIDENCE=0`) and default strict AIR, proving the no-drift AIR
  validation path does not mutate backend output.
- `make air-backend-nonimpact-full-test-smoke` runs the full frozen
  backend-compare fixture sweep (`PGY_AIR_NONIMPACT_SOURCE=all`) and is now the
  CI Linux AIR backend non-impact gate. Large local runs may shard the same
  frozen sweep with `AIR_NONIMPACT_SHARD_COUNT` / `AIR_NONIMPACT_SHARD_INDEX`,
  or cap a triage run with `AIR_NONIMPACT_CASE_LIMIT`; these options do not
  change the default full gate.
- `make air-strict-backend-compare-test-smoke` runs the normal C/LLVM backend
  execution compare under default strict AIR validation, so strict AIR
  validation is covered by real binary parity, not only generated text
  comparison.

Strict evidence update:

- Strict evidence is now the default AIR validation mode.
- Missing HIR CFG, RIR boundary, or RIR authority evidence becomes
  `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`, with dedicated
  `PGY_CAUSE_INTENT_BOUNDARY_EVIDENCE` and
  `PGY_FIX_ALIGN_INTENT_BOUNDARY_EVIDENCE`.
- Authority evidence is participant-sensitive: a boundary declared with
  `authorized by: X` is not satisfied by unrelated authority facts or authorize
  ops in the same RIR scope.
- AIR boundary evidence is now provenance-carrying: each boundary stores
  AIR-owned HIR routine, RIR boundary scope, and RIR authority participant names
  when evidence is found. This makes strict evidence failures debuggable without
  borrowing source IR lifetimes.
- AIR synthesis now records whether HIR input was present. In default strict
  evidence mode, any boundary synthesized with HIR input must have matching HIR
  routine provenance before it can be accepted as an abstraction-boundary fact.
  This is weaker than requiring HIR CFG proof for every boundary, but it closes
  the prior gap where RIR-only zone/world evidence could look complete even
  though AIR had no matching body-level routine owner.
- AIR strict-evidence diagnostics now print the same provenance summary
  (`evidence hir=... hir_cfg=... rir_boundary=... rir_authority=...`) in text/JSON output,
  so tooling does not need to infer which proof leg was absent.
- `air_dump()` now prints per-boundary evidence provenance names and the AIR unit
  suite gates that debug surface, so compiler-debug output stays aligned with
  text/JSON diagnostics.
- Parsed `where + transfer` coverage now requires both the emitted zone boundary
  and emitted world boundary to carry RIR boundary/authority evidence
  provenance, not just to exist.
- Authority evidence diagnostics include the expected authority participant
  list in `Reason:` when required RIR authority evidence is missing.
- `PGY_AIR_STRICT_EVIDENCE=0` remains as a development/debug opt-out for
  isolating AIR evidence coverage regressions; it is not the beta default.

Remaining (Phase 1 — beta 진입 전 반드시 실 구현):

- HIR + DIR + RIR synthesis edge coverage: stable intent subset evidence 누락은 default strict hard-fail로 승격됐다. Direct AST-backed execution-boundary coverage now exists for `spawn` and IO. Direct AIR coverage now verifies world boundaries require source-specific RIR transfer op evidence. Parsed-source positive coverage now verifies `where + transfer` emits both zone and world boundaries with owned source names. Parsed-source negative baseline now exists for missing authority evidence, missing IO execution-boundary evidence through the full driver JSON path, and missing world-transfer RIR evidence after source lowering. Remaining work is any later parsed execution-boundary drift that becomes semantically valid instead of pre-AIR rejected.
- Parsed-source AIR transfer negative coverage now exists: a source-lowered
  `transfer` world boundary fails strict AIR when its boundary-scoped RIR
  transfer evidence is removed, and the drift keeps
  `source_provenance=transfer` plus the boundary source.
- AIR text dump and driver diagnostic evidence summaries now read provider /
  subject provenance from `AIREvidenceNode` inventory instead of the legacy
  boundary summary-name fields through the shared
  `air_boundary_evidence_node/provider/subject` read seam, keeping user-facing
  evidence details on the same source of truth as strict verification.
- backend non-consumption regression: source scanning and generated C/LLVM
  non-impact smoke now cover the full frozen backend-compare fixture set in
  Linux CI. Strict evidence also runs through the backend execution compare.
  Remaining work is Windows native evidence and additional parsed-source
  negative diagnostics beyond the current authority-evidence and IO-boundary
  evidence cases.
- Phase 1 invariant docs exist in `docs/semantics/07_air_abstraction_safety.md`
  for drift detection soundness, synthesis read-only, codegen non-impact,
  intent node coverage, boundary closure, and strict evidence failure
  soundness. Source-backed transfer/world boundary negative coverage now
  exists; remaining invariant work is later parsed execution-boundary drift
  that becomes semantically valid instead of pre-AIR rejected.
- Phase 1 schema 가 Phase 2 (Constraint Node, Effect Node) 와 Phase 3 (drift fact 종류 확장) 를 막지 않도록 **future-compatible 하게 enum/struct** 설계 (`drift_kind` 가 enum 1 종에서 시작해 추가 가능, Boundary `kind` 가 5 종에서 시작해 추가 가능).
- AIR 를 codegen path 에 연결하지 않는다. AIR drift 검사는 semantic/compiler validation 단계에 머물고 C / LLVM output을 직접 바꾸지 않는다.

Out of Phase 1 (베타 후 생태계 단계에서 추가 가능):

- Phase 2: Constraint Node (sync/async, local/distributed, fallible/infallible, persistence), Effect Node (DB/Network/FS/External), 추가 drift fact (failure-class mismatch, transactional-scope mismatch).
- Phase 3: AIR 가 안정되면 일부 metadata 의 단일 source-of-truth 화 (예: zone boundary 정보가 DIR 와 AIR 양쪽에 있던 것을 AIR 단일화).

Evidence command:

```sh
make air-drift-test-smoke
make air-json-schema-test-smoke
make air-backend-nonimpact-test-smoke
make air-backend-nonimpact-full-test-smoke
make AIR_NONIMPACT_SHARD_COUNT=2 AIR_NONIMPACT_SHARD_INDEX=0 air-backend-nonimpact-full-test-smoke
make AIR_NONIMPACT_SHARD_COUNT=2 AIR_NONIMPACT_SHARD_INDEX=1 air-backend-nonimpact-full-test-smoke
make air-strict-backend-compare-test-smoke
make formal-semantics-test-smoke
make diagnostic-registry-test-smoke
make llvm-test-backend-compare
```

## 0g. Compiler Design Quality Verification

Status: `IN PROGRESS / VERIFICATION`

Source of truth: this section + cross-reference into `docs/19` §0 (systems
language identity), `docs/20_compiler_pipeline_guide.md`, and
`docs/118_slot_model_rigor_audit.md`.

Goal:

- Pergyra의 컴파일러 구현이 production-grade design quality를 만족함을
  명시적으로 검증한다.
- CS 석사-tier 평가 체크리스트 (textbook 4가지 패턴) 와 modern production
  컴파일러 architecture 기준 (multi-IR, pattern dispatch, persistent scope,
  rich diagnostics) 둘 다 통과해야 한다.
- 베타 closure 직전 미통과 항목은 lift 또는 명시적 out-of-beta로 분류한다.
- 이 검증은 *compiler engineering quality*에 대한 것이고, runtime safety
  (docs/security/), formal semantics (docs/semantics/), abstraction
  portability (docs/117) 는 별도 layer 이다.

### Textbook checklist (CS 석사-tier 평가)

학생/junior 컴파일러 평가 시 표준 4점 체크리스트와 Pergyra 매핑:

| Textbook 제약 | 의도 | Pergyra 답 | 상태 |
|---|---|---|---|
| AST nodes are immutable (분석이 트리 오염 안 함) | 파싱 결과 보존 | 5-IR pipeline (AST → HIR → DIR → RIR → MIR). AST는 read-only entry IR이고 분석은 다음 IR에서 진행. mutate 안 함 | ✅ 교과서 답 *초과* |
| AST와 분석 로직 디커플링 (Visitor / Double Dispatch) | 데이터-로직 분리 | C tagged-union + switch on `node->kind`. AST는 class hierarchy 아니라 tagged union이므로 method 첨부 자체가 불가능. Visitor 흉내 안 함 | ✅ 교과서 답 *idiom 적합* |
| 심볼 테이블 = HashMap stack (블록 스코프 shadowing) | 변수 가시성 | **Frame chain + per-scope hash index with flat-array ownership storage** (`src/semantic/symbol_table.c`). The stable path is no longer a pure linear lookup; the linear helper is a malformed-index compatibility fallback and tiny-scope safety net. | 🟢 audit 완료 / stale linear-scan debt closed |
| CompilerDiagnostic 객체 (line/col/hint, exception 아님) | 진단 누적 | `diag_codes.h` 100+ stable codes + level/stage/`Reason:`/`Fix:`/span/multi-span/JSON 회귀 (`diagnostics-json-test-smoke`) | ✅ 교과서 답 *대폭 초과* |

→ **4점 textbook 중 3개 *초과*, 1개 audit 필요 (스코프 implementation pattern).** textbook 채점은 대부분 통과.

### Production architecture 기준 (textbook 너머)

modern 컴파일러 (rustc, Clang, TS, GHC, Roc) 가 textbook 4점을 *넘어서*
공통적으로 가진 추가 criteria 와 Pergyra 매핑:

| Production criterion | Pergyra 답 | 상태 |
|---|---|---|
| Multi-IR pipeline (single AST analysis pass 아님) | AST/HIR/DIR/RIR/MIR + AIR (verification IR) = 6 IR | ✅ |
| Pattern matching dispatch (Visitor 안 씀) | C tagged union switch (idiomatic in C; pattern matching 등가) | ✅ |
| Persistent / versioned scope (naive HashMap-of-HashMap 아님) | **Frame chain + per-scope hash index + flat array for ordered ownership cleanup**. This keeps scope teardown simple while making normal name lookup hash-backed. | 🟢 audit 완료 / 베타 acceptable |
| Stable diagnostic codes (free-text 아님) | `PGY_*` 100+ stable codes, `diag_codes.h` | ✅ |
| Span representation (range + snippet, line/col만 아님) | **point-span only** (`Diagnostic` line/col 단일, ASTNode도 line/column 단일). end_line/end_column 또는 byte range 미지원. snippet 렌더링은 별도 도구가 line/col로 부분 재구성 | 🟡 lift 후보 (범위 표현 추가) |
| Multi-span 진단 (offending site + related def site) | **API single-span only**. `Diagnostic` 단일 line/col, `semantic_error*` 단일 `node`. 일부 site (slot_analyzer)가 메시지 텍스트에 prior line 숫자 임베드하지만 JSON/LSP consumer는 추출 불가. 대부분 diagnostic은 prior site 0 정보 (e.g., class redeclaration) | 🔴 명시적 out-of-beta / LSP 통합 시 lift |
| Suggestion / fix-it 힌트 | `Reason:` / `Fix:` 구조 + auto-fix 일부 | 🟡 |
| Error recovery (parse-through-error / sema-through-error) | 부분 — 베타 closure에서 lift 후보 | 🟡 |
| Diagnostic JSON 회귀 (진단 자체가 stable) | `make diagnostics-json-test-smoke` | ✅ |
| Side-table for analysis (mutable AST annotation 아님) | IR pipeline이 자체적으로 side-table 역할 (각 IR이 별도 store) | ✅ |

→ **10개 production criteria audit 결과: 5 ✅ + 1 🟢 (스코프 audit 완료) + 3 🟡 lift 후보 + 1 🔴 명시적 out-of-beta:**
- 스코프 pattern: 🟢 audit 완료 (frame chain + flat array, 베타 acceptable, 프로파일링 후 lift)
- Span 범위 표현: 🟡 point-span only, range/snippet 추가 lift 후보
- Multi-span 진단: 🔴 명시적 out-of-beta (API 자체 미지원, LSP 통합 시 lift)
- Fix-it 확대: 🟡 lift 후보 (LSP 통합과 묶음)
- Error recovery 확대: 🟡 lift 후보

### Closed now

- 5-IR + AIR 6-IR pipeline 운영 (AST/HIR/DIR/RIR/MIR + AIR)
- C tagged union switch dispatch가 모든 semantic / codegen pass의 표준
- 100+ stable diagnostic codes, level/stage/Reason/Fix 구조 강제
- Diagnostic JSON 회귀 gate (`make diagnostics-json-test-smoke`)
- Span + 부분 multi-span
- AST는 mutate 안 됨 — 분석 결과는 HIR+에 들어감 (immutable AST 교과서 답 초과)
- Visitor pattern 흉내 안 함 — C에 idiom-적합한 tagged union switch 채택
  (Visitor는 OOP without pattern matching의 workaround이므로 C에서는
  anti-idiomatic)

### Remaining

- **스코프 manager pattern audit** — *완료 (2026-04-27).* 패턴은 *(e) Frame
  chain + per-scope hash index + flat ownership array*, hash-backed lookup
  (`src/semantic/symbol_table.c` 296 LOC). textbook (a)~(d) 어느 쪽도 아니고
  *더 단순한 (e)*. 작은 scope에서는 cache-friendly로 hash 기반보다 *빠를 수
  있음*. 큰 scope (전역에 수천 symbol, 큰 함수에 수백 local) 워크로드에서는
  hash 기반으로 lift 필요. **베타 acceptable, 프로파일링이 hot path 보일 때
  lift 후보.** Lift 시 권장 패턴: linear-array 일정 크기까지 유지하다가
  threshold 넘으면 hash로 promote (Clang/GCC와 유사).
- **Multi-span 진단 audit** — *완료 (2026-04-27).* `Diagnostic` struct가 단일
  line/col만 갖고 `semantic_error*` API가 단일 `node`만 받음. 즉 *API 자체에
  multi-span 미지원*. 일부 site (예: `slot_analyzer.c`)는 메시지 텍스트에
  prior line 숫자를 임베드하지만, JSON consumer / LSP / IDE 도구는 그 secondary
  위치를 추출 불가. 대부분 diagnostic (class/ability/role redeclaration 등)은
  prior site 정보를 *전혀* 안 줌. **베타 closure 결정: 명시적 out-of-beta.**
  Lift 비용 ~2-3일 (Diagnostic struct 확장 + API 추가 + 핵심 5-10개 마이그레이션
  + JSON 회귀 + 텍스트 렌더러). LSP 통합 작업과 묶어서 한 번에 처리 권장.
- **Span 범위 표현 audit** — *완료 (2026-04-27).* ASTNode와 Diagnostic 모두
  단일 line/column만 보유. end_line/end_col 없음 → 범위 표현 미지원. snippet
  렌더링은 도구가 line/col로 부분 재구성. **lift 후보 (베타 acceptable, post-1.0
  LSP/IDE 통합 시 lift).**
- Fix-it / suggestion 확대 — `Reason:` / `Fix:` 구조에 textual fix는 있는데,
  *machine-applicable* fix-it (rustc `--fix` 같은 자동 적용) 은 없음.
  베타 closure 후 LSP 통합 시 lift 후보.
- Error recovery 확대 — parse-through-error / sema-through-error / type-error
  recovery. 현재 부분. 베타 closure 안에 어디까지 lift할지 결정.
- `compiler-design-quality-test-smoke` 신설 — 위 항목들을 자동 검증하는 회귀.
  현재 `make diagnostics-json-test-smoke` 가 진단 layer만 cover하고, 스코프
  pattern / IR pipeline shape는 별도 검증 필요.

### Out of scope (명시적 reject)

- **Visitor pattern wrapper 도입**. C에서 anti-idiomatic. tagged union +
  switch가 production-correct. Visitor 강제 시 N×M class explosion + indirect
  dispatch overhead + cache miss + 새 노드 추가 시 모든 visitor 깨짐. 교과서
  답이지만 implementation language (C) 와 맞지 않으므로 의도적 reject.
- **Mutable AST annotation**. 5-IR pipeline 전체 거부. 분석 결과는 다음 IR
  store로 가지 AST에 leak되지 않음.
- **Single-pass interpreter-style 분석**. 5-IR pipeline 정체성과 충돌.

### Evidence command

```sh
make test-semantic
make diagnostics-json-test-smoke
make diagnostic-registry-test-smoke
make ir-pipeline-test-smoke
# 신설 후보:
# make compiler-design-quality-test-smoke
```

## 0h. Type-Resolution DAG Closure

Status: `IN PROGRESS / BLOCKER`

Source of truth: `TODO.md` type-resolution DAG section,
`tests/type_resolution_dag_smoke.sh`, and
`tests/type_resolution_resolver_inventory_smoke.sh`.

Goal:

- Stable type refs must resolve through graph/topo metadata rather than
  owner-local recursive resolver fallback.
- Declaration order, generic defaults, where bounds, ability consumers, module
  contracts, zone/world authority consumers, and alias cycles must share one
  dependency vocabulary and one diagnostic provenance model.
- The retired recursive resolver implementation must stay absent; only audit
  counters may remain for zero-call reporting.

Closed now:

- DAG graph/topo inventory is active and smoke-gated.
- Owner-local resolver seams are gone: new direct `resolve_type_node(...)`
  calls outside the resolver body / metadata owner fail
  `type-resolution-resolver-inventory-test-smoke`.
- Central metadata materializer fallback is dormant in the semantic suite:
  `materializer_fallbacks=0`.
- Current local stats are `graph-backed skips=2061`,
  `metadata_entries=3735`, `metadata_owned=261`,
  `metadata_hits=8771`, and `materializer_unresolved=0`.
- Metadata fallback families are all zero, including named, generic-named,
  compound, other, builtin shell, generic class, alias, non-class symbol, and
  missing-symbol fallback.
- Top-level program placeholder signatures consume DAG annotations through
  `program_lookup_dag_type_annotation_or_unknown(...)`. The resolver-inventory
  smoke blocks old `program_resolve_*` naming and local metadata materialization
  in `type_checker_program.c`, so this path cannot silently drift back into a
  recursive resolver-style seam.
- AIR evidence inventory now rejects duplicate evidence nodes with the same
  kind, boundary, provider, and subject. Repeated evidence must increase
  `fact_count` instead of adding ambiguous duplicate nodes.
- Non-CFG MIR statement population now hard-rejects accidental use on
  CFG-backed HIR routines. This keeps legacy source-statement population out of
  the CFG/body-safety source-of-truth path.
- Alias compatibility surface is closed at the DAG stage: `compat_alias=0`,
  `compat_non_alias=0`, `alias_materialized=6`,
  `alias_diagnostic_unresolved=78`, and
  `alias_diagnostic_resolver_calls=0`.
- Valid alias stage replay now uses metadata-only lookup before the quiet
  diagnostic unresolved path. The DAG smoke gates `compat_alias == 0`, so any
  alias replay that leaks back into the recursive resolver path fails the beta
  DAG contract.
- Non-metadata `semantic_type_resolution_lookup_resolved_annotation(...)`
  readers are smoke-gated at zero. Contract/boundary readers now enter the
  materializing type-ref seam, while the remaining annotation-only
  `or_unknown` consumer is the program placeholder path.
- Central metadata materialization no longer falls through to
  `resolve_type_node(type_node, ctx)`. Unsupported shapes are recorded as
  explicit fallback inventory and return unresolved; the DAG smoke keeps
  `materializer_fallbacks=0`, and resolver-inventory smoke gates recursive
  fallback escape hatches at zero.
- Metadata alias chain and cycle handling now has a dedicated owner:
  `type_checker_resolution_metadata_alias.c` owns alias-chain materialization,
  cycle formatting, and `semantic_type_resolution_lookup_metadata_name_or_alias(...)`.
  The central metadata owner is now orchestration-only for lookup and
  materialization dispatch.
- The previous recursive alias resolver and `SemanticContext.alias_resolution_*`
  stack are removed. Direct named alias resolution now goes through the same
  metadata alias owner as staged alias replay.
- `resolve_named_type(...)` is now metadata-first for stable builtin, scope,
  generic-parameter, nominal, and alias names. It only falls back to the old
  diagnostic path when DAG metadata cannot answer the named string.
- Stable named builtin/shell lookup is no longer duplicated in the
  compatibility helper. `resolve_named_type(...)` delegates scalar builtin and
  stable shell recognition to
  `semantic_type_resolution_metadata_named_builtin_or_shell_singleton(...)`,
  and `type-resolution-resolver-inventory-test-smoke` rejects reintroducing a
  local `strcmp(name, "...")` builtin/shell table in
  `type_checker_resolution_helpers.c`.
- 2026-04-29 update: the metadata type-ref API now materializes stable
  constructed refs before the compatibility resolver can run, and
  `semantic_stage_resolve_type_quiet(...)` consumes that same type-ref API
  before compatibility fallback accounting. This closes a small but important
  signature-stage seam: constructed stable refs reached by compatibility
  callers stay DAG-metadata-first instead of silently reopening recursive
  resolver fallback.
- 2026-04-29 API update, superseded by the 2026-05-03 semantic-owner closure:
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)` was the named
  semantic-owner API for "metadata-first, diagnostic-materializer second" type
  refs. It prevented each checker owner from hand-rolling the same preflight
  while the compatibility seam was being retired. The current beta gate rejects
  this symbol under `src/semantic`; semantic owners consume metadata facts plus
  narrow diagnostic helpers directly.
- 2026-04-29 domain seam update: intent participant/value/where type refs and
  zone authority subject-slot type refs now consume that API. Ability where,
  class/function signature, action contract, domain slot, and world slot refs
  use the same helper. Expression/member/operator annotation refs, generic
  default/contract refs, async channel parameter refs, ownership refs, and
  projection path refs also use it. `type-resolution-resolver-inventory-test-smoke`
  now fails if a semantic owner bypasses the metadata-first helper and calls the
  diagnostic materializer directly; only central metadata/diagnostic
  compatibility owners may call `semantic_type_resolution_lookup_or_materialize(...)`.
  This keeps the first semantic-owner compatibility seam metadata-owned without
  starting the beta+1 Domain AST -> Core AST rewrite.
- Semantic regression now covers provider-after-consumer alias materialization
  for a nested constructed alias (`Later = Channel<Slot<Int>>`) consumed by a
  function signature before the alias declaration.
- The resolver inventory smoke now also gates the materializer fallback
  recorder and rejects recursive metadata escape hatches at zero.

Remaining:

- Keep the recursive resolver retired as an evaluator source for stable type
  refs. The central metadata escape hatch and private compatibility body are
  removed; remaining work is to keep every non-semantic driver/backend path on
  DAG/topo facts and prevent evaluator-body reintroduction.
- Keep provider-after-consumer generic/default/ability/module/zone-world
  regressions in semantic and C/LLVM parity suites.

Evidence command:

```sh
make type-resolution-dag-test-smoke
make type-resolution-resolver-inventory-test-smoke
make test-semantic
```
