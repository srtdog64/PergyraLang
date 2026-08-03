# Ownership Model Comparison And Pergyra Target

Last updated: 2026-08-03

Related documents:

- `docs/19_design_philosophy.md` §0 — **core identity** (Pergyra is a systems language; ownership model is layered on that baseline)
- `docs/74_slot_pinning_caching.md`
- `docs/146_sea_execution_lanes.md` -- current lane evidence, runtime scaffold,
  and precise-capture frontier
- `docs/100_beta_readiness_checklist.md`
- `docs/104_air_compiler_architecture.md`
- `docs/114_async_model_positioning.md` — sister positioning doc for concurrency
- `docs/117_backend_strategy_positioning.md` — sister positioning doc for backend strategy
- `docs/118_slot_model_rigor_audit.md` — sister audit doc; Slot vs borrow-check rigor and marketing-language guide
- `docs/119_pergyra_lineage_positioning.md` — sister positioning doc for language lineage (C# father, Tier 1-5 substrate borrow, DDD unique synthesis)
- `docs/120_vision_and_capability_audit.md` — sister audit; capability negative-space + current-vs-vision separation
- `docs/121_types_as_domain_medium.md` — sister positioning; type system as the syntactic machine of lost-meaning recovery (carrier / coherence / negative-space)
- `docs/122_managing_intent_drift.md` — sister positioning; drift management discipline (visibility / boundedness / evidence / recoverability / recognition across five drift kinds)
- `docs/semantics/04_ownership_abi.md`

## 1. Purpose

Pergyra should not copy Rust, Swift, Mojo, Verona, Vale, or C#. The useful part
is to identify which proven ownership ideas fit Pergyra's core axis:

- intent/zone/world orchestration
- capability-bearing slots
- anchored handles
- explicit cross-world transfer
- backend parity between C and LLVM

The beta goal is not "maximal ownership theory". The beta goal is a narrow,
stable ownership subset that can be checked by semantic analysis, runtime ABI,
CFG/AIR evidence, diagnostics, and backend tests.

## 2. Comparison Axes

| Axis | Meaning |
|---|---|
| Memory safety | Prevent use-after-free, double-free, invalid reference, data race. |
| Resource safety | Decide who releases files, locks, slots, device mappings, and views. |
| Abstraction safety | Keep intent, authority, zone, and backend behavior from drifting. |
| Distributed safety | Keep ownership transfer clear across task, channel, world, and device boundaries. |

Current Pergyra evidence is intentionally mixed. Percentages are not used here
because no executable gate owns such a score:

- Memory safety is `BOUNDED`: the covered CFG/AIR body and backend-compare
  slices are evidence; whole-language body ownership is open.
- Resource safety is `BOUNDED`: block-scoped region/pin cleanup and selected
  ownership flows are evidence; arbitrary mandatory finalization is open.
- Abstraction/distributed safety is `BOUNDED`: intent, authority, lane, and
  transfer contracts have named gates, while precise capture and full runtime
  propagation remain open.

Option C below is a direction, not a numerical completion forecast. Each item
becomes current only through its named semantic, runtime, C/LLVM, and negative
gate.

Slot positioning:

- Pergyra does not expose memory as address ownership.
- Pergyra exposes memory as a modular resource boundary.
- A Slot is the stable language-level boundary; the backend handle below it is
  replaceable.
- This keeps the same source meaning when the lower representation is a C
  pointer, arena index, generational handle, device buffer id, file handle,
  database row handle, or remote-world handle.

Beta lifetime policy:

- Pergyra does not expose Rust-style lifetime parameters such as `'a`.
- Surface lifetime is expressed through ownership classes, Slot/Token handles,
  typed views, Zone/Intent boundaries, and block-scoped leases.
- The compiler must still prove body-local safety, but that proof belongs in
  CFG/AIR dataflow and runtime ABI contracts rather than in user-authored
  lifetime syntax.
- This preserves the cross-platform abstraction goal: the same source should
  keep the same ownership meaning across C, LLVM, device, fiber, and future
  backend targets.

## 3. Six Prior Arts

### Rust

Strengths:

- Lifetimes prove reference validity.
- Aliasing XOR mutability is enforced: many `&T` or one `&mut T`.
- RAII `Drop` gives deterministic cleanup.
- `Rc<T>` and `Arc<T>` are explicit shared ownership escape hatches.
- `Send` and `Sync` classify thread transfer.

What Pergyra should take:

- `WriteView<T>` must have exclusive access semantics.
- Resource cleanup must be compiler-owned for block-scoped constructs.
- Shared ownership should be explicit, for example a minimal `Rc<T>`.

What Pergyra should not take for beta:

- Lifetime annotation syntax like `'a`.
- Rust `Pin<&mut T>` semantics. Pergyra `pin` means slot lease, not
  self-referential future pinning.
- `Send`/`Sync` trait surface before the fiber/task model is frozen.

#### 2026 Move/Forget direction and the Pergyra boundary

The accepted 2026 Rust project direction supplied for this review explores
positive capability traits such as `Move`, `Destruct`, and `Forget`. It is a
direction for RFC and implementation work, not a claim that stable Rust already
has the complete feature. The target lets a type opt out of relocation or
forgetting so self-referential values can keep a stable address and scoped task
handles cannot evade a required finalizer.

Pergyra has a different, bounded advantage in the structured-concurrency slice:

- `parallel` has a scoped join contract;
- the landed lane classifier can join pin, live-view, raw-slot, raw-channel,
  effect, and movability evidence and reject covered contradictions;
- the native compiler's certified region slice and gated pin fixtures carry
  bounded cleanup through plan/CFG evidence instead of asking a backend to
  guess.

This is not whole-language superiority. Precise closure-capture production is
still incomplete, several executor lanes remain synchronous scaffolds, `pin` is
a slot/view lease rather than arbitrary-type immovability, and Pergyra does not
yet prevent every user-defined required finalizer from being forgotten. The
installed self-host path also does not yet reach the native pin surface or the
full LLVM lane projection, so native closure must not be promoted into a
whole-self-host claim.

| Axis | Rust 2026 direction | Pergyra current evidence | Status / open work |
|---|---|---|---|
| scoped fork/join | `!Forget` can make a scoped handle non-evadable | structured `parallel` join contract | bounded; executor depth remains scaffolded |
| authority-aware lane choice | not the proposal's primary axis | one evidence classifier and fail-closed contradiction matrix | bounded; precise capture and runtime propagation remain open |
| arbitrary immovable user type | proposed `!Move` capability | no general equivalent | candidate `relocatable` fact, not current SoT |
| guaranteed finalizer for arbitrary type | proposed `!Forget` capability | certified cleanup slices only | candidate `must_finalize` consumption fact, not current SoT |

The scoped advantage applies to structured `parallel`, whose syntax joins
before continuation. A named `spawn` returns `Future<T>`; `await` consumes that
handle and prevents reuse, but the current semantic checker has no general
function-exit must-await rule for a still-live future. Pergyra therefore does
not yet have a general safe-scoped-spawn answer equivalent to the proposed
`!Forget` handle guarantee.

Observed on 2026-08-03, `execution-lane-policy-test-smoke` is green for the
12-row native decision table and 13-row AIR producer table. After its Windows
output-path bug was removed, `self-host-execution-lane-parity-test-smoke`
reaches and passes the C classifier rows 35/35, then fails closed before an
LLVM artifact because the installed self-host LLVM projector does not yet own
this program. The full C/LLVM self-host parity gate is therefore red, not a
current 35/35-each claim.

The overloaded word "move" must not collapse four different facts:

| Fact | Current owner | Meaning |
|---|---|---|
| ownership transfer | `OwnershipTypeClass`, `own`/`ref`, resource flow | which binding or boundary owns a value after transfer |
| executor relocation demand | `BoundaryCaptureFact.requires_movability` | whether one concurrency site requires a movable executor lane |
| physical address relocation | not implemented as a general type fact | whether a value may change address during its lifetime |
| cleanup/abandonment obligation | bounded CFG/region/pin plans only | whether scope exit must consume or finalize a value |

`MOVE_ONLY` therefore does not mean Rust-style `!Move`, and
`requires_movability` is a site demand rather than proof that every captured
type is physically relocatable. Likewise, the absence of a public
`mem::forget` equivalent is not by itself a guaranteed-finalizer proof.

If relocation or mandatory-cleanup semantics are promoted, they remain under
the same semantic ownership authority but use a separate orthogonal capability
row; they must not be squeezed into `OwnershipTypeClass`. Assignment/storage
consume relocation capability, spawn/parallel consume the join of capture
capability and site demand, and CFG/region/zone plans consume cleanup
obligations. Unknown required facts fail closed.

Do not introduce a second `Move`/`Forget` policy table, make routine users
select scheduling lanes, or reinterpret Pergyra `pin` as Rust `Pin`. Do not add
surface syntax until an actual authority, interoperability, observable-cost, or
ownership boundary requires it. The current negative contract is pinned by
`ownership-relocation-cleanup-contract-test-smoke`; executable lane behavior
remains owned by `execution-lane-policy-test-smoke` and
`self-host-execution-lane-parity-test-smoke`.

### Project Verona

Verona's `cown` model is the closest prior art for Pergyra zones:

- A concurrent owner isolates mutable state.
- `when (...)` executes only while the required owners are available.
- Cross-owner communication is message oriented.

Pergyra mapping:

- `cown[T]` maps conceptually to a zone authority owner.
- Verona `when` maps conceptually to intent step scheduling.
- Message isolation maps to Pergyra's channel-only cross-world transfer rule.

Lesson: Pergyra zone/world/authority should remain core language semantics, not
library conventions.

### Mojo

Mojo is useful because it treats ownership as part of AI and accelerator
programming:

- `owned` models ownership transfer.
- `borrowed` models read borrow.
- `inout` models mutable borrow.
- Accelerator memory movement is a first-class design pressure.

Pergyra mapping:

- `own` maps to ownership transfer.
- `ref` maps to borrow-like access, but Pergyra still needs CFG-backed lifetime
  rules.
- `DeviceSlot<T>` and future Spray/GPU APIs need Mojo-like host/device transfer
  clarity.

Lesson: AI-first does not mean putting GPU into core syntax. It means the module
ecosystem needs a strong device ownership contract.

### Swift ARC

Swift proves that reference counting can be ergonomic:

- `class` is reference counted.
- `struct` and `enum` are value types.
- `weak` and `unowned` solve cycles.
- `inout` marks mutation.

Pergyra mapping:

- A minimal `Rc<T>` can support graph/tree/callback structures.
- `Weak<T>` is part of the stable single-thread primitive/String beta subset
  so cycles can be broken without importing default ARC.
- Default ARC would blur Pergyra's anchored/capability model, so shared
  ownership must be opt-in.

Lesson: add shared ownership as an explicit module/core primitive only if the
semantic/runtime contract is small enough to freeze.

### Vale

Vale validates a direction Pergyra already uses:

- Generational references detect stale references.
- Regions reduce lifetime annotation pressure.
- Safety does not require Rust-style lifetime syntax in every program.

Pergyra mapping:

- `SlotHandle` generation counters and secure-slot tokens are generation-style
  stale handle protection.
- Scratch/result/persistent arenas map to role-based lifetime lanes.

Precision vs Vale's monotonic generational reference: closed (2026-07-02). Both
secure-slot twins issue a monotonic per-suffix token identity on every claim
(the inline C-leg twin previously derived the token from the claim temp's
address, so repeated claims through one call site reproduced the same id and
false-matched stale tokens). Locked by `make secure-token-reuse-test-smoke`;
history and mechanism in `docs/118_slot_model_rigor_audit.md` §6.7.

Lesson: Pergyra can reject lifetime annotation syntax and still become safer by
making generation, arena lane, and CFG cleanup facts explicit.

### C# `fixed`

C# `fixed` is the closest prior art for Slot Pinning:

```csharp
fixed (int* p = &array[0]) {
    for (int i = 0; i < n; i++) p[i] *= 2;
}
```

The useful shape is block-scoped pin with compiler-owned unpin. Pergyra's
candidate:

```pergyra
pin slot as view: WriteView<Int> {
    view[0] = 1
}
```

Pergyra adds token, generation, authority, and slot lifecycle validation. See
`docs/74_slot_pinning_caching.md`.

## 4. Generic + Ownership Interaction

Generics are part of the core language, not a side module. The unresolved
ownership question is what the compiler may assume about `T`:

```pergyra
func process<T>(x: own T): Void
func view<T: Display>(x: ref T): String
```

Required beta decision:

- Either define a generic param ownership classifier, or conservatively reject
  ownership-sensitive generic uses without enough facts.
- The classifier must map generic parameters into known classes such as
  `COPY_ONLY`, `MOVE_ONLY`, `BORROW_TRACKED`, `SUBJECT_IDENTITY`, and
  `ANCHORED_HANDLE`.
- Ability bounds may refine ownership class, but must not silently invent it.
- Current baseline: unresolved `TYPE_KIND_GENERIC` is treated as
  `BORROW_TRACKED` for ownership classification. This is a conservative reject
  path for generic `own/ref` until an ability-bound classifier can prove a more
  precise class.

This belongs in `docs/100_beta_readiness_checklist.md` §0c.

## 5. Async Ownership

Async boundaries are ownership boundaries:

- `await` can suspend while local references are live.
- `spawn` can move values to another task.
- `parallel` can create simultaneous access to the same anchored handle.
- Channels can transfer values across execution contexts.

Beta stance:

- `pin` blocks cannot contain `await`.
- Views cannot escape to `spawn`, `async`, `parallel`, callbacks, or channels.
- `spawn ref` remains rejected unless the callee summary and task lifetime are
  proven.
- Ownership-bearing non-blocking receive and cancellation payloads stay
  restricted until cleanup/backpressure summaries are complete.

This belongs in `docs/100_beta_readiness_checklist.md` §0b.

## 6. Option C Ownership Lift

Option C is the recommended small lift before beta if the project wants a safer
ecosystem-ready ownership surface without importing Rust lifetime syntax.

Implement:

- `pin slot as view { ... }` block syntax is active for typed views and now
  survives into HIR/MIR pin-region metadata plus MIR `pin-unpin-cleanup-edge`
  metadata. Generated inline slot runtime wrappers exist for C/LLVM call parity.
  The C source-block path emits cleanup-hooked pin wrapper locals, and the C
  plus LLVM MIR paths emit explicit pin/unpin on successor/return exits for the
  frozen pin backend-compare fixtures. Broader all-exit proof coverage remains
  the blocker.
- `PinnedView<T>` RAII handle as `ANCHORED_HANDLE`, but keep it post-block and
  not stable until function-boundary rules close.
- `WriteView<T>` exclusive static enforcement.
- Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable for
  `Int`, `Long`, `Float`, `Double`, `Bool`, and `String`. The contract is
  explicit shared ownership with semantic/runtime/C/LLVM/lifecycle regressions.
  Fractional numeric literals infer as `Float`; `Rc<Double>` is stable through
  explicit `Double`-typed values or annotations, not a separate double-literal
  surface.
  Payloads outside that set are semantic rejects, not backend fallback cases.
  `Arc<T>`, cross-thread shared ownership, and default ARC are post-beta.
- Generic param ownership classifier.

Do not implement for beta:

- Lifetime annotation syntax like `'a`.
- `Send`/`Sync` trait surface.
- Rust `Pin<&mut T>`.
- Default ARC for all objects.

## 7. Beta Mapping

| Option C item | Checklist location |
|---|---|
| `pin` block and `PinnedView<T>` | §4 ABI Ownership / Arena Lifetime Closure |
| `WriteView<T>` exclusive access | §0b Function CFG / Body Dataflow Closure |
| Minimal `Rc<T>` | §0c Core Language Semantic Closure |
| Generic param ownership classifier | §0c Core Language Semantic Closure |
| Async pin/view rejection | §0b Function CFG / Body Dataflow Closure |

Important wording:

- Table note: the `Minimal Rc<T>` row is now a stable subset row, not a broad
  ARC commitment. Stable means primitive/String payloads only, single-thread
  lifecycle only, and explicit `RcNew` / `RcClone` / `RcGet` / `RcDrop` /
  `RcDowngrade` / `WeakUpgrade` / `WeakDrop` calls. Non-stable payloads are
  explicit semantic rejects.
- The `Rc<T>` type shell appearing in DAG metadata is only stable when it maps
  to the closed semantic/runtime/C/LLVM/lifecycle regressions above.
- `DeviceSlot<T>` in the pinning matrix is a candidate, not an implemented stable
  device pin surface.
- Diagnostic names for Pin/Lease are planned until registered and tested.

## 8. Conclusion

Pergyra's ownership identity should be:

- capability and authority aware
- anchored-handle first
- generic-aware
- async-boundary explicit
- CFG/AIR validated before backend lowering

The best path is not to turn Pergyra into Rust. The best path is to import only
the narrow pieces that close current pain points: RAII-style scoped cleanup,
exclusive mutable view facts, explicit shared ownership, and generic ownership
classification. Relocatability and must-finalize semantics, if promoted, extend
that one classifier and its verified plans rather than becoming a parallel type
system.
