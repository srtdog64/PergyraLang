# Ownership Model Comparison And Pergyra Target

Last updated: 2026-04-25

Related documents:

- `docs/74_slot_pinning_caching.md`
- `docs/100_beta_readiness_checklist.md`
- `docs/104_air_compiler_architecture.md`
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

Current Pergyra position is intentionally mixed:

- Memory safety: about 70 percent until CFG/AIR body ownership is complete.
- Resource safety: about 60 percent until drop/cleanup and Pin/Lease close.
- Abstraction/distributed safety: stronger, about 90 percent in the language
  direction, but still gated by runtime propagation and backend parity.

If Option C below is implemented, the target moves roughly to memory 85 percent
and resource 90 percent without importing lifetime annotations.

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

- `pin slot as view { ... }` block syntax, but only after CFG cleanup and
  backend parity.
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
classification.
