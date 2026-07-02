# Slot Model Theoretical Rigor Audit

Last updated: 2026-04-27

Related documents:

- `docs/19_design_philosophy.md` §0 — **core identity** (Pergyra is a systems language; this audit's Slot model lives on that baseline)
- `docs/106_ownership_model_comparison.md` — sister positioning doc for ownership
- `docs/114_async_model_positioning.md` — sister positioning doc for concurrency
- `docs/117_backend_strategy_positioning.md` — sister positioning doc for backend
- `docs/100_beta_readiness_checklist.md` §0 / §0c — formal semantics + core
  semantic closure
- `docs/103_cfg_body_dataflow_need.md` — CFG completeness status
- `docs/74_slot_pinning_caching.md` — pin block boundary design
- `docs/semantics/` — proof obligations index
- `docs/security/` — AI Validator adversarial counterexample audits (Tier 3 invariants)
- `docs/119_pergyra_lineage_positioning.md` — sister positioning doc for language lineage; §11 marketing-phrasing table extends this doc's §8 negative-space audit
- `docs/120_vision_and_capability_audit.md` — sister audit; capability negative-space + current-vs-vision separation (completes the three-pair protocol with this doc's §8 and `docs/119` §11)
- `docs/121_types_as_domain_medium.md` — sister positioning; type system as the syntactic machine of lost-meaning recovery (Slot is the state-transition coordinate axis in §2)
- `docs/122_managing_intent_drift.md` — sister positioning; drift management for when this doc's §6 danger zones produce real drift in real programs

This document is an **honest audit** of what Pergyra's Slot model and
ownership system actually guarantee statically, what they check at runtime,
and where the marketing language risks outpacing the implementation. It
exists because the language designer expressed concern that the Slot model
was built as "I wish it worked like this" rather than rigorous formalism,
and that it borrowed Dijkstra's logic without strict mechanization.

The short answer: **the worry is partially correct and partially overstated**.
This document maps the proof status of every static and runtime guarantee
in the system, in three tiers, and lists the real danger zones where
documentation could overpromise.

## 0. Thesis — Slot Rigor Is Load-Bearing For Every Higher Abstraction

This audit matters more than any other rigor audit Pergyra will write,
because **Slot is the minimum unit of abstraction in the language**.
Pergyra has trivial copyable primitives (`Int`, `Bool`, `String`,
`Float`, `Double`, `Long`) below Slot, but every ownership-bearing,
resource-bearing, capability-bearing, identity-bearing, or
boundary-crossing abstraction starts at Slot. Every higher-level
construct ultimately reduces to operations on Slot:

- `Subject` identity is anchored to slot ownership.
- `Authority` and `Token<T>` are slot-resident capabilities.
- `Zone` and `World` own and arbitrate slot lifecycles.
- `Channel<T>` transports slot-typed payloads.
- `intent` step state is slot-backed.
- `parallel` task ownership snapshots are slot-keyed.
- `pin` views project a typed window onto a slot.

If Slot semantics are wrong, every abstraction above Slot is wrong.
The 5-component static layer described in §3 is not a *replacement* for
Slot; it is the language's way of reasoning **about** Slot operations.
Slot is the substrate; the static layer is the proof apparatus that
operates over the substrate.

This is why an audit at the Slot level has higher leverage than any other
rigor audit:

- A bug in `intent` step rollback semantics affects intent users.
- A bug in `Channel<T>` ownership transfer affects channel users.
- **A bug in Slot ownership semantics affects every user of every
  abstraction in the language.**

The audit's three-tier classification (§4) is therefore not academic.
It is the language's honest accounting of where the foundation is
provably correct, where it is conservatively rejected, and where it is
runtime-validated. Future contributors should treat any change to Slot
semantics as automatically requiring re-audit of every higher
abstraction's claimed guarantees.

## 0-entry. Slot Is Not The Default Value Model

Slot is the explicit resource-boundary model, not the ordinary value model.
Pergyra programs should not need `ClaimSlot` / `Write` / `Read` / `Release`
for a trivial value, a pure calculation, or a log statement.

```pergyra
func Main() -> Void
{
    Log("Hello, Pergyra!");
}
```

Use `Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`, pin/view syntax, and
explicit release paths when the code crosses a resource, authority,
backend-handle, lifetime, or runtime boundary. This distinction is part of
the beta surface trust contract: examples must not present Slot lifecycle
boilerplate as the default way to write ordinary code.

## 0a. Positive Thesis — Slot Is A Modular Resource Boundary

Slot should not be judged as "a weaker Rust borrow checker". It is solving the
same broad memory/resource problem at a different abstraction boundary.

```text
Pergyra does not expose memory as address ownership.
Pergyra exposes memory as a modular resource boundary.
A Slot is the stable language-level boundary; the backend handle below it is replaceable.
```

Rust makes the address/reference/lifetime relation visible and proves much of
that relation statically. Pergyra deliberately hides the lower address relation
behind a module-like resource boundary. The user manipulates the Slot contract,
not the backend address. Under that contract, the runtime/backend can replace a
raw pointer with an arena index, generational handle, device buffer id, file
handle, database row handle, or remote-world handle without changing the source
meaning.

The exact positioning is:

```text
Slot = address abstraction + ownership boundary + capability gate + replaceable backend handle
```

The static ownership/CFG layer proves when this boundary is used safely for the
stable subset. The runtime Slot layer validates generation, token, release, and
pin-state when execution reaches the boundary. This split is intentional: the
language goal is a uniform resource-boundary experience across memory, device,
world, and authority resources, not a lifetime-first pointer language.

## 1. Purpose and Target Reader

This audit is written for:

- Language evaluators and reviewers asking "does Pergyra give Rust-equivalent
  static guarantees?"
- Future contributors who need to know what is proven, what is conservatively
  rejected, and what is checked at runtime.
- The author, when writing future README / blog / marketing copy, as a
  negative-space guide (§8).

This is not a user surface guide. New users should read `docs/22_ownership_model.md`,
`docs/74_slot_pinning_caching.md`, and `docs/106_ownership_model_comparison.md`
first.

## 2. Slot Is Not a Borrow Checker

The most common confusion: Slot is often described as "Pergyra's borrow
checker." That framing is wrong, and clarifying it is the first step of
the audit.

This is now a formal documentation rule, not a wording preference:
`docs/semantics/08_slot_capability_calculus.md` records "Slot is not a borrow
checker" as a negative claim. Any README, tutorial, issue, or release note that
uses the phrase must rewrite it before beta.

**Slot is a runtime-validated handle**:

- `SlotHandle` carries a generation counter; stale handles are rejected at
  read/write time.
- `SecureSlot<T>` carries a token capability; forged tokens are rejected at
  read/write/release time.
- TTL cleanup expires unused slots after a configurable interval.
- Release / double-release / use-after-release / use-after-cleanup all
  hard-fail through the runtime panic contract.

These are *runtime checks*. They produce panics or returned errors when
violated, not compile errors.

A borrow checker, by contrast, is a *static procedure*: it rejects programs
at compile time that *could* violate memory safety. Rust's borrow checker
proves aliasing-XOR-mutability and reference validity through lifetime
analysis. Slot proves none of these things on its own.

The borrow-checker-equivalent in Pergyra is not Slot; it is the **5-component
static layer** that sits above Slot. Section 3 names that layer.

The practical consequence is simple:

- Slot alone can make a bad access fail safely at runtime.
- Slot alone cannot prove that no bad access exists.
- The proof that no bad access exists must come from CFG/body dataflow,
  ownership classification, boundary rejection, and cleanup insertion.

### 2.1 Forbidden: Lifetime Annotation Syntax (`'a`)

This is a hard design constraint (BDFL, 2026-06-29), not a default that may be
revisited per proposal: **Pergyra must never import Rust-style lifetime
annotation syntax** — `'a`, generic lifetime parameters `<'a>`, annotated
references `&'a T`, lifetime bounds `'a: 'b`, or higher-ranked `for<'a>`. It is
explicitly rejected, in any future form, for any motivating feature.

Rationale — this is the positive corollary of "Slot is not a borrow checker":

- The entire slot / `own` / `ref` / lifecycle / generation-token stack exists
  *precisely to deliver safety without lifetime annotations*. Importing `'a`
  would reintroduce the exact cognitive load that stack was designed to remove
  and would make the model self-contradictory.
- Ownership and validity are already carried by other facts: `own`/`ref` at
  boundaries (interprocedural, compile-time), slot release/generation/token
  state (runtime), zone/handle scope, and CFG cleanup. Lifetime parameters would
  be a redundant second encoding of the same information, in the most
  notation-heavy form available.
- Even Vale — which is *more* aggressive on memory safety — rejects pervasive
  lifetime syntax (generational references + regions instead). So refusing `'a`
  is well-trodden, not reckless.

Enforcement of the constraint:

- Any proposal to add lifetime annotations for *any* reason (variance,
  self-referential structs, higher-ranked bounds, escaping closures, returned
  handles, etc.) is auto-rejected at the design gate. A real expressiveness gap
  must be closed by a slot/zone/handle/`SlotHandle<T> in Zone` primitive, never
  by lifetime syntax.
- Surface, docs, tutorials, and release notes must not present `'a`-style
  annotations even as an example or comparison target beyond the explicit
  "we do not adopt this" framing in `docs/106_ownership_model_comparison.md`.
- If the apostrophe ever becomes a type-position sigil through a parser change,
  it must be a hard, helpful rejection ("Pergyra has no lifetime annotations;
  express ownership with `own`/`ref` and slot lifecycle — see docs/118 §2"),
  never a silently-accepted construct.

## 3. The Actual Borrow-Checker-Equivalent

The static layer that does the borrow-checker-equivalent work has five
components. None is sufficient on its own; together they form the static
contract of the language.

| Component | Role | Rust analogue | Implementation site |
|---|---|---|---|
| Ownership classifier (5 classes) | Type-level ownership classification | `Copy` / affine type | `src/semantic/type_checker_ownership_classify.c` |
| CFG body dataflow | Move / use-after-move tracking | Borrow check (subset) | HIR/MIR CFG; `docs/103` |
| pin block boundary rules | Resource cannot cross suspension | `Pin` + part of `Send`/`Sync` | Active source-level typed-view pin blocks; `docs/74`; diagnostics + backend compare |
| Channel-only cross-World rule | Cross-task / cross-world transfer | `Send` / `Sync` | `docs/113`, `docs/106` |
| Token transport reject | Capability flow control | (Pergyra-unique) | semantic + `make test-security` |

The 5 ownership classes (`OWNERSHIP_TYPE_COPY_ONLY`, `OWNERSHIP_TYPE_MOVE_ONLY`,
`OWNERSHIP_TYPE_BORROW_TRACKED`, `OWNERSHIP_TYPE_SUBJECT_IDENTITY`,
`OWNERSHIP_TYPE_ANCHORED_HANDLE`) are defined in
`src/semantic/type_checker_ownership_internal.h:6-12` and drive every other
component's behavior.

Anyone evaluating Pergyra's static safety should evaluate this 5-component
layer, not Slot in isolation.

## 3a. Handle Expiration Is A Layered Contract, Not Pin Alone

Pin/Lease is only the answer for one narrow hot-path case: keep a slot live
inside a lexical block while repeated access is amortized. It does **not**
solve every stale-handle shape. The honest beta contract is layered.

Non-pin stale-handle scenarios:

| Scenario | Risk |
|---|---|
| Handle escapes a function through return or storage | Caller can retain a stale handle after the callee's resource boundary ends |
| Handle is stored in a long-lived collection or field | Collection can outlive the slot release path |
| Handle is captured by `async` / `spawn` closure | Task may execute after the source slot has been released |
| Handle crosses channel / world handoff | Receiver may observe a stale or authority-mismatched handle |
| Handle is copied, then only one copy follows a release path | The other copy becomes silently stale unless rejected or runtime-validated |

Pergyra's current answer is five mechanisms, not one mechanism:

| Layer | Mechanism | Scenario covered | Tier |
|---|---|---|---|
| 1 | Arena lane (`scratch` / `result` / `persistent`) | Function escape, long-lived storage lane mismatch | Static Tier 1/2 |
| 2 | CFG body dataflow over `BORROW_TRACKED` / anchored handles | Return/store escape and use-after-move in the covered body subset | Static Tier 1, partial |
| 3 | Zone/world ownership plus channel-only crossing | World handoff and cross-boundary authority movement | Static Tier 1 |
| 4 | `Token<T>` transport rejection | `spawn`, channel, cancel, and authority-bearing boundary transport | Static Tier 1 |
| 5 | Generation + token runtime validation | Fallback for all stale access paths that reach runtime | Runtime Tier 3 |

If layers 1-4 reject a program, the guarantee is compile-time. If only layer 5
applies, the program is still fail-safe but not statically proven: stale access
must execute before the runtime can reject it. This distinction must remain
visible in docs and diagnostics.

The missing expressivity piece is first-class **Zone-Bound Handle** typing.
Today the compiler often uses conservative `BORROW_TRACKED` or anchored-handle
escape rejection where a more expressive model would say "this handle is valid
only for zone Z." The target design is a type-level owner such as:

```text
SlotHandle<T> in Zone
```

or a sugar such as:

```text
handle@zone
```

This is the Pergyra-shaped equivalent of region/lifetime ownership in Rust,
Vale, and Project Verona, but it must avoid Rust-style user lifetime
annotation burden. Until this is implemented, the sound beta behavior is
conservative rejection plus runtime generation/token hard-fail.

## 4. Three-Tier Classification of Guarantees

Every guarantee in the system falls into one of three tiers. The tier
determines *how* the guarantee is delivered, which determines what kind
of marketing language is honest.

### 4.1 Tier 1 — Active static rejection (borrow-check analogue, narrow subset)

These rejections fire at compile time today, with regression evidence.

| Rule | Diagnostic | Evidence |
|---|---|---|
| Channel cross-World transfer outside channel | semantic reject | `make test-security` 182/182 |
| Token<T> spawn / channel send / cancel payload | semantic reject | semantic + `make test-semantic` |
| Anchored slot branch/join consume conflict | CFG snapshot | `docs/100` §0b closed items |
| Authority-bearing token spawn boundary | semantic reject | `make test-semantic` |
| Non-Void function all-path return missing | `PGY_SEM_MISSING_RETURN` | `make cfg-body-dataflow-test-smoke` |
| Statements after a terminator | `PGY_SEM_UNREACHABLE_CODE` | `make cfg-body-dataflow-test-smoke` |
| Parallel `ref`/`own` boundary conflict | `PGY_SEM_PARALLEL_SLOT_CONFLICT` | `make parallel-core-contract-test-smoke` |
| Function reference escape (conservative) | `PGY_SEM_BORROW_ESCAPE` | `make test-semantic` |
| `QubitSlot` move-after-consume | semantic reject | `make test-semantic` |
| Anonymous async spawn body | parser reject | parser test |
| Movable resource non-blocking channel receive | semantic reject | `docs/113` happens-before contract |

These are real Tier 1 for the listed rules: the compiler rejects today, with
regression evidence, and there is no accepted stable path for these specific
violations. This is a borrow-check analogue for a narrow domain-boundary subset,
not a claim of Rust-level lifetime or aliasing coverage.

### 4.2 Tier 1-active for typed-view pin blocks, runtime-lowering pending

These rules are defined in design docs, have diagnostic codes registered, and
now fire from source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }`
blocks because the parser desugars that syntax to the existing typed
`ViewRead(...)` / `ViewWrite(...)` semantic surface. What remains pending is
explicit backend lowering through `PgyPinnedView` / `PergyraSlotPin` /
`PergyraSlotUnpin` cleanup edges.

| Rule | Diagnostic | Status |
|---|---|---|
| `pin` block crossing `await` / `spawn` / `async` / callback / channel / cancel | `PGY_SEM_PIN_AWAIT_BOUNDARY` | Active through source-level pin block desugaring and JSON smoke |
| `pin` view escape | `PGY_SEM_PIN_ESCAPE` | Active through typed view return-escape semantic regression |
| `pin` parallel conflict | `PGY_SEM_PIN_PARALLEL_CONFLICT` | Active through source-level and constructor view boundary/acquisition checks |
| `pin` of `QubitSlot` | `PGY_SEM_PIN_QUBIT_REJECT` | Active through `ViewRead/ViewWrite(QubitSlot)` semantic regression |
| source-level `pin` read/write parity for `Slot<T>` / `SecureSlot<T>` | backend compare | Active through `pin_read_view_block`, `pin_secure_read_view_block`, `pin_mixed_read_view_sequence`, `pin_write_view_block`, and `pin_secure_write_view_block` C/LLVM compare fixtures |
| `pin` token capability check | `PGY_SEM_PIN_TOKEN_INVALID` | Source-level emission: `ViewRead/ViewWrite` over a `SecureSlot<T>` whose paired capability token symbol is not reachable in scope is rejected before runtime; runtime ABI hard-fail remains the deeper backstop |

This separation still matters. Public communication may now say that
source-level typed-view pin blocks reject suspension and transport boundaries.
It must not claim that the block already lowers to raw runtime
`PgyPinnedView` pin/unpin cleanup edges; that backend ABI path remains internal.

### 4.3 Tier 2 — Conservative rejection (no unsoundness, limited expressiveness)

These rules reject when the compiler cannot prove safety. The rejection is
sound (no unsafe program is accepted) but conservative (some safe programs
are also rejected).

- **Generic param ownership**: unresolved `TYPE_KIND_GENERIC` is classified
  as `OWNERSHIP_TYPE_BORROW_TRACKED`. Generic `own/ref` uses that depend on
  unknown ownership class are conservatively rejected. (`docs/100` §0c)
- **Branch/join assignment lattice**: sealed to `let = init`. Reassignment
  in branches without an explicit lattice is rejected.
- **Borrow lifetime**: task-local snapshot only. Longer-than-task borrow
  patterns are rejected.
- **Function reference return**: rejected with `PGY_SEM_BORROW_ESCAPE`
  unless the value is owned-return.

Tier 2 is honest — the language admits "we cannot prove this is safe, so
we reject." Rust 1.0 had similar conservative gaps before Non-Lexical
Lifetimes (2018). The honest comparison is *"Pergyra rejects what Rust
1.0 rejected, with similar conservatism."*

### 4.4 Tier 3 — Runtime check (Vale / Project Verona pattern)

These checks fire at runtime and produce panics or returned errors.

| Check | Failure mode | Evidence |
|---|---|---|
| Slot generation mismatch | runtime reject | `make test-security` |
| Zero/max slot id exhaustion plus released-slot recycle | fresh id exhaustion rejects; released ids recycle with generation advance | `make test-security` |
| Tampered pinned-view generation / double unpin | runtime reject as invalid pin | `make test-security` |
| Secure slot token forgery | runtime reject | `make test-security` |
| Authority token mismatch | runtime reject | `make runtime-authority-contract-test-smoke` |
| TTL cleanup of stale slot | runtime cleanup | `make test-security` |
| Secure scope destroy while pinned | `SLOT_ERROR_PINNED` via checked destroy; void destroy panics | `make test-security` fixture coverage; local object compile when OpenSSL is unavailable |
| Release/move of source while typed view is live | static reject + runtime reject | `make diagnostics-json-test-smoke`, `runtime-panic-abi-test-smoke` |
| Result-owned file/string helper lifetime | resolved paths are freed on error exits; string length arithmetic is checked before allocation | `make runtime-abi-lifetime-test-smoke` |
| Inline/export input ABI parity | `pgy_input` returns result-owned strings on both C inline and LLVM-linkable surfaces | `make runtime-abi-lifetime-test-smoke` |
| Exported array slice pointer derivation | zero-length slices return before pointer arithmetic; range checks use subtract form | `make runtime-abi-lifetime-test-smoke` |
| Slot boundary read `Result` wrappers | inline and LLVM-linkable Slot / DeviceSlot / SecureSlot read failures return typed `PgyRuntimeSlotResult_*` data at host/service boundaries | `make runtime-panic-abi-test-smoke`, `make security-portability-contract-test-smoke` |
| File I/O boundary `Result` wrappers | inline and LLVM-linkable `FileOpen` / `FileExists` / `FileRead` / `FileWrite` / `ReadFile` / `WriteFile` / `Input` compatibility paths have typed `PgyRuntimeIoFailure` result owners for host/service boundaries | `make runtime-panic-abi-test-smoke`, `make runtime-abi-lifetime-test-smoke`, `make security-portability-contract-test-smoke` |
| Channel receive boundary `Result` wrappers | inline and LLVM-linkable channel receive failures return typed `PgyRuntimeChannelFailure` data for closed/empty/timeout states while legacy value wrappers keep sentinel behavior | `make runtime-panic-abi-test-smoke`, `make runtime-abi-lifetime-test-smoke`, `make security-portability-contract-test-smoke` |
| `List<String>` payload ownership | raw list push/set duplicate strings; get returns list-borrowed pointer; remove frees owned payloads | `make runtime-abi-lifetime-test-smoke` |
| `Queue<String>` payload ownership | queue push duplicates strings; LLVM uses string-specific raw queue exports instead of pointer memcpy | `make runtime-abi-lifetime-test-smoke` |
| LLVM `Channel<String>` payload ownership | send duplicates strings into channel-owned storage; receive clears the slot and transfers the owned payload; destroy frees pending payloads | `make runtime-abi-lifetime-test-smoke` |
| `Set<String>` payload ownership | raw set add duplicates strings; has uses borrowed probes; remove frees owned payloads and tombstones the slot | `make runtime-abi-lifetime-test-smoke` |
| LLVM raw `HashMap<K,String>` value ownership | set duplicates string values; get returns map-borrowed values; remove frees both key and value while keeping tombstones | `make runtime-abi-lifetime-test-smoke` |
| Raw `HashMap` probe-chain integrity | remove frees the runtime-owned key and leaves a tombstone so later linear-probe entries remain reachable | `make runtime-abi-lifetime-test-smoke` |
| Inline collection probe-chain integrity | generated-C `HashMap` and `Set` removals use tombstones rather than empty slots | `make runtime-abi-lifetime-test-smoke` |
| Direct owner/slot-sugar access bypassing live view | static reject | `make diagnostics-json-test-smoke` |
| Helper-boundary owner bypass while typed view is live | static reject | `make diagnostics-json-test-smoke` |
| Container-store owner bypass while typed view is live | static reject | `make diagnostics-json-test-smoke` |
| Return-boundary owner bypass while typed view is live | static reject | `make diagnostics-json-test-smoke` |
| Box resource-handle payload | static reject | `make diagnostics-json-test-smoke` |
| Runtime pointer arithmetic overflow | pool/arena/slice/BoxArray/SlotPool panic or reject before deriving an invalid pointer | direct runtime-lib, SlotPool, and data-structure object compile |
| Async scope tracked/scheduled fiber mismatch | scheduler enqueues the same fiber that the scope owns; failed enqueue rolls back tracking | direct async runtime object compile |
| Double release | `released-slot` panic class | `runtime-panic-abi-test-smoke` |
| Use after release | `released-slot` panic class | `runtime-panic-abi-test-smoke` |

This is the Vale generational-references pattern (Vale 2021 OOPSLA
"Generational References"). It is a real, well-formalized technique, not a
shortcut. Saying "Slot is runtime-validated" is honest; saying "Slot is a
borrow checker" is not.

Current implementation note: the table-backed C ABI is not a 64-bit generation
handle yet. It uses a 32-bit `slotId` plus a 32-bit generation field. Released
slot ids are recycled only by advancing `generation`, so a stale handle with the
old generation cannot revalidate. Fresh claims still reject the zero-id sentinel
and max-id wrap as `SLOT_ERROR_ID_EXHAUSTED`; a released entry whose generation
is already exhausted also returns `SLOT_ERROR_ID_EXHAUSTED` instead of allocator
OOM. This keeps id-space exhaustion separate from allocator OOM without claiming
a widened ABI. Unpin also requires the issued view generation/mode/thread/
pointer to match, so tampered views and double-unpin attempts cannot clear a
live pin. A future 64-bit handle ABI can relax the exhaustion point further, but
beta documentation must not claim that ABI until the headers, runtime, C
backend, LLVM backend, and ABI spec all carry it.

## 5. Dijkstra Application — What Was Actually Borrowed

The author's concern that Pergyra "borrowed Dijkstra's logic loosely" is
worth auditing precisely. Dijkstra produced several distinct bodies of work,
not one.

| Dijkstra area | Pergyra application | Rigor |
|---|---|---|
| Structured programming / CFG | HIR/MIR CFG, all-path return, unreachable code, defer cleanup body isolation | ✅ Settled theorem, applied directly |
| Predicate transformer (`wp` calculus) | Not applied | n/a |
| Mutual exclusion / semaphore | `pin` block + `WriteView<T>` exclusive | ⚠️ Same-slot `WriteView<T>` exclusivity is active; full borrow/lifetime lattice still waits for Option C/CFG cleanup |
| Self-stabilization | Not applied | n/a |
| Guarded commands | Not applied (Pergyra uses pattern matching) | n/a |
| Concurrent processes (PA) | Influenced but not directly | partial — happens-before model is closer to Java JMM ancestry |

The honest phrasing: **Pergyra rigorously applies Dijkstra's structured
programming / CFG result, implements the first same-slot mutual-exclusion
slice for `WriteView<T>`, and still treats the full borrow/lifetime lattice as
in flight. It does not apply predicate transformer or self-stabilization work.**

This is not "borrowing logic loosely." It is a precise, narrow application
of one of Dijkstra's most settled results, plus aspirational application
of another. The CFG portion is as rigorous as any production language's
CFG.

## 5a. Corrections Against Common Stale Claims

These corrections are intentionally blunt because they are easy to get wrong
when summarizing the project status:

- **Stale claim**: "pin syntax is only parser-gated, so PIN diagnostics are
  registered but inactive."
  **Correction**: source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }`
  now parses and reaches the same typed-view semantic diagnostics as
  `ViewRead(...)` / `ViewWrite(...)`. The remaining gap is explicit runtime
  `PgyPinnedView` pin/unpin lowering and cleanup-edge parity.
- **Stale claim**: "`WriteView<T>` exclusivity is not enforced."
  **Correction**: current `ViewRead(...)` / `ViewWrite(...)` same-slot
  exclusivity is enforced and covered by semantic plus JSON diagnostics
  regression. The block-scoped source `pin` surface is active for typed views.
- **Stale claim**: "release/move while pinned is only a runtime concern."
  **Correction**: source-level typed-view pins now reject `Release(source)` and
  `Move(source)` while `ReadView<T>` / `WriteView<T>` over that source is live.
  The runtime still keeps its hard-fail pin-state guard for lower-level
  SlotManager users.
- **Stale claim**: "same-slot view exclusivity is enough even if the owner name
  is still usable."
  **Correction**: owner writes are rejected while any typed view over that slot
  is live, and owner reads are rejected while a `WriteView<T>` is live. Slot
  sugar follows the same rule: `slot = value` is an owner write, and
  value-position `slot` is an owner read.
- **Stale claim**: "helper calls can safely analyze their own slot access."
  **Correction**: while a typed view is live, passing the owning source slot to
  an `own`/`ref Slot<T>` helper is rejected. The helper must accept a typed view
  directly or be called after the pin/view scope ends.
- **Stale claim**: "collections are just value storage and do not affect pin
  safety."
  **Correction**: array literals and stable collection-store helpers are escape
  boundaries for owner handles. While a typed view is live, storing or
  forwarding the owning source slot through `[slot]`, `ListPush`, `MapSet`,
  `SetAdd`, `ArrayPush`, `ArraySet`, `ListSet`, or `QueuePush` is rejected.
- **Stale claim**: "returning the owner during a read view will be caught by
  backend type checking anyway."
  **Correction**: return is a semantic ownership boundary. `return slot` while
  a typed view over that source is live is rejected before C/LLVM lowering so
  backend auto-read behavior cannot become the diagnostic surface.
- **Stale claim**: "`Box<T>` can store any `T` accepted by the parser."
  **Correction**: the beta-stable `Box<T>` surface rejects resource handles.
  Slot handles already have ownership/runtime anchors; boxing them would create
  a second storage owner not covered by the current proof or ABI contract.
  The remaining work is automatic cleanup-edge insertion and explicit runtime
  `PgyPinnedView` backend lowering parity.
- **Stale claim**: "Slot must be judged alone against Rust's borrow checker."
  **Correction**: Slot alone is runtime capability safety. The comparable
  static safety unit is the five-component layer above Slot: ownership
  classifier, CFG/body dataflow, pin boundary rules, channel/world transfer
  rules, and token transport rejection.

## 6. Real Danger Zones

These are the places where current marketing language risks
outpacing current implementation. Each is recorded honestly so future
documentation does not drift.

### 6.1 WriteView<T> Aliasing-XOR-Mutability

- **Status**: partially enforced today for both source-level pin blocks and the
  existing `ViewRead(...)` / `ViewWrite(...)` semantic surface: `WriteView<T>`
  conflicts with any active view of the same source slot, while shared
  `ReadView<T>` / `ReadView<T>` remains accepted. Releasing or moving the
  source slot while any typed view over it is live is also rejected. Direct owner
  writes are rejected under any live view, and direct owner reads are rejected
  under a live `WriteView<T>`, including slot assignment sugar and value-position
  owner identifier sugar. Passing the owner to `own`/`ref Slot<T>` helpers while
  a typed view is live is also rejected, as is storing or forwarding that owner
  through stable container-store paths or a return boundary. It is not yet a
  complete arbitrary borrow/lifetime system. `Box<T>` also explicitly rejects
  resource-handle payloads in the beta-stable surface.
- **Closure path**: Option C ownership lift (`docs/106` §6); CFG branch/join
  evidence required for full enforcement.
- **Marketing to avoid**: "Pergyra enforces Rust-style aliasing-XOR-mutability
  for all references." Today this is false. Honest version: "The current
  `WriteView<T>` surface enforces same-slot write exclusivity; the full
  borrow-checker-equivalent claim waits for CFG no-escape, cleanup, and
  boundary facts."

### 6.2 Generic Param Ownership Classifier

- **Status**: unresolved generics conservatively classified as
  `OWNERSHIP_TYPE_BORROW_TRACKED` (sound, not expressive).
- **Closure path**: Option C lift; ability-bound classifier or trait-style
  refinement.
- **Marketing to avoid**: "Pergyra generics carry full ownership info."
  Honest: "Generic ownership is conservative; user code may need explicit
  classifier hints."

### 6.3 Branch/Join Assignment Lattice

- **Status**: sealed to `let = init`. Other reassignment patterns are
  rejected by CFG or treated conservatively.
- **Closure path**: full lattice in CFG body dataflow (`docs/103`).
- **Marketing to avoid**: "Pergyra supports arbitrary mutable patterns."
  Honest: "Sealed local-`let` baseline; wider mutation requires future
  lattice work."

### 6.4 CFG Body Dataflow Completeness

- **Status**: ~70% (closed items in `docs/100` §0b).
- **Open items**: drop / cleanup insertion, longer-than-task borrow
  lifetime, full branch/join, zone/effect transition CFG, projection
  freshness through CFG, broader channel receive / backpressure, and
  richer cancellation task-boundary checks. Multi-pin sequencing in one MIR
  block is now covered for straight-line typed-view read/write parity across
  plain and secure slots; branch/loop multi-pin proof coverage is still pending.
- **Marketing to avoid**: "Pergyra body safety is statically complete."
  Honest: "Body safety covers the implemented subset; remaining items
  are listed in `docs/100` §0b."

### 6.5 Runtime Failure Misread As Static Proof

- **Status**: Slot runtime failure classes are real and important, but they are
  not static proofs.
- **Closure path**: every runtime failure class that beta depends on must be
  paired with either a static reject or a documented runtime hard-fail class.
- **Marketing to avoid**: "Released slot use cannot be written." Honest:
  "Released slot use is statically rejected for the covered CFG subset and
  hard-fails at runtime if reached through a runtime-only path."

### 6.6 Non-Pin Handle Expiration And Missing Zone-Bound Handle Type

- **Status**: the language has layered stale-handle defenses, but no first-class
  zone-bound handle type yet. Pin blocks solve lexical hot-path liveness only;
  they do not solve return/storage escape, async capture, channel/world handoff,
  or copied-handle release divergence by themselves.
- **Current safe behavior**: use arena lane checks, CFG/body dataflow,
  anchored-handle copy rejection, channel/world transport rules, token transport
  rejection, and generation/token runtime validation. When the static layer
  cannot prove safety, the beta subset must reject conservatively rather than
  pretend the handle is lifetime-proven.
- **Closure path**: decide before beta freeze whether `SlotHandle<T> in Zone`
  (or equivalent `handle@zone` sugar) is in-scope. If yes, add zone-scope <=
  handle-scope checking and make zone exit invalidate all in-zone handles in
  both static facts and runtime generation/token state. If no, document
  `BORROW_TRACKED` conservative rejection as the beta behavior.
- **Marketing to avoid**: "Pinning solves handle expiration." Honest:
  "Pinning solves lexical pinned access; non-pin handle expiration is protected
  by a layered static/runtime contract, with Zone-Bound Handle typing still a
  beta-freeze design decision."

### 6.7 Secure-Slot Token Reuse Collision — CLOSED (monotonic generation)

- **Status: CLOSED (2026-07-02).** Both secure-slot twins now issue a
  monotonic, per-suffix token identity on every claim. The audit that opened
  this item found the gap was narrower — and in one spot wider — than first
  written: the **extern twin** (`pgy_runtime_lib_secure_slot_exports.h`, the
  LLVM leg) already used a monotonic atomic counter, while the **inline twin**
  (`pgy_runtime_slot_macros.h`, the C leg) still derived the token purely from
  an address: `id = (uintptr_t)s ^ 0xDEADBEEFCAFEBABE`. Worse than same-address
  slot reuse, the address used was the *claim temp* (`pgy_claim_secure` builds
  the slot in a local and returns it by value), so **repeated claims through one
  call site reproduced the identical id every time** — any stale token
  false-matched any later slot claimed through that site. That was also a
  runtime-twin lockstep violation (C and LLVM legs disagreed on identity
  semantics).
- **Fix**: the inline twin now mirrors the extern twin exactly — a per-suffix
  `atomic_uint_least64_t` counter (same golden-ratio seed, same zero-skip),
  `pgy_make_token` takes `fetch_add + 1`. Every claim's identity is fresh; a
  stale token can never equal a later one, meeting the same standard as Vale's
  monotonic generational reference (§4.4).
- **Locked by** `tests/secure_token_reuse_failclosed_smoke.sh`
  (`make secure-token-reuse-test-smoke`) driving
  `src/tests/secure_token_reuse_test.c`: `distinct_ids` asserts two claims
  through one call site get distinct ids (the old scheme fails this), and
  `stale_read`/`stale_write` assert a token retained across release/re-claim
  fails closed with `class=invalid-secure-token`, never a silent false-accept.
- **Marketing**: "generation-style stale-handle protection" may now say both
  slot-token legs use monotonic generation. Still do not conflate this with a
  static lifetime proof — it is a runtime fail-closed layer (§2, §4.4).

## 7. Comparison To Rust Across Time

| Snapshot | Static strength | Mechanized proof | Notes |
|---|---|---|---|
| Rust 1.0 (2015) | Borrow checker, lifetime, Send/Sync | None at 1.0 release | Mechanized proof came later |
| Rust 2018+ | NLL borrow checker, edition refinements | RustBelt (2018) for unsafe core | First mech results |
| Rust 2026 | const generics, GAT, refined trait solver | Extended RustBelt + ongoing | Multi-decade research |
| **Pergyra Beta (2026)** | **Tier 1 + Tier 2 static + Slot runtime + 5-class classifier** | **None — Level 2 evidence pack** | **Smaller static subset than Rust; different runtime-capability model** |
| Pergyra 1.0 + freeze | + Option C lift, + AIR Phase 1 | None | Between Rust 1.0 and 2018 in scope |
| Pergyra post-1.0 | Level 3 paper proof attempts | Optional academic | Pergyra-equivalent of RustBelt |

The point: **Rust 1.0 shipped without mechanized proof**, but that does not
make Pergyra's beta safety story Rust-equivalent. Pergyra Beta has a smaller
static subset plus runtime handle/capability validation. Level 2 evidence
(theorem statements + judgments + evidence map) is acceptable for beta only
when the claim is scoped to Pergyra's frozen subset. Mechanized proof remains
a multi-year research project, not a beta claim.

The comparison to Rust 1.0 at launch is a scope-bounded analogy: Pergyra has
active static rejection for the frozen subset and no mechanized proof, but it
does not claim Rust-level borrow checking, full lifetime inference, or
Rust-level memory safety.

The POSITIVE form of this comparison — why the smaller static subset plus
fail-closed runtime guards is a coherent position rather than a deficit — is
now formalized: `docs/semantics/20_minimal_verification_position.md` states the
minimal-verification position (same no-UB end guarantee, obligation reduced to
a finite per-operation coverage table plus the small `Proven` set's promises),
and `docs/semantics/proofs/GuardCalculus.v` mechanizes it (coqc-checked:
`no_silent_ub`, `coverage_is_local`,
`guarded_more_permissive_at_equal_safety`). The marketing constraint stands:
that position is never phrased as "Rust-equivalent safety".

## 8. Marketing Language Audit

These are phrases that have appeared or could appear in docs / README /
blog / external description, mapped to the honest version. Treat this
table as a negative-space guide before publishing.

| Risky phrasing | Honest phrasing |
|---|---|
| "Rust-level memory safety" | "Pergyra has a smaller static subset plus runtime handle/capability checks; do not describe it as Rust-equivalent." |
| "Slot is a borrow checker" | "Slot is runtime-validated; the static layer is the ownership classifier + CFG + pin + channel + token rules" |
| "Slot proves borrow safety" | "Slot proves capability/generation/token/pin-state runtime safety; borrow safety requires the CFG bridge facts" |
| "Aliasing-XOR-mutability enforced" | "Current `WriteView<T>` same-slot exclusivity is enforced; full Rust-style borrow safety waits for CFG no-escape/cleanup/boundary facts" |
| "Lifetime safety guaranteed" | "No `'a` lifetime annotations; arena lanes + handle generation + CFG body checks cover the frozen subset; other patterns are rejected or runtime-validated." |
| "No data races possible" | "Channel-only cross-World + parallel `ref`/`own` conflict reject covers the covered subset; full `Send`/`Sync` analogue is not in beta" |
| "pin blocks use raw runtime pointers safely" | "source-level typed-view pin blocks reject suspension and transport boundaries; explicit runtime `PgyPinnedView` lowering remains internal until cleanup parity closes" |
| "Dijkstra-rigorous" | "Pergyra applies Dijkstra's structured programming / CFG result rigorously; mutual exclusion application is in flight; predicate transformer is not applied" |
| "Borrow checker without lifetime annotations" | "5-component static layer + Slot runtime; conservative on patterns Rust would prove with lifetime annotations" |

## 9. Path Forward — Proof-Level Progression

| Level | Definition | Pergyra timing |
|---|---|---|
| Level 0 | No formal semantics doc | Not Pergyra |
| Level 1 | English prose semantics | Not Pergyra |
| Level 2 | Theorem statements + judgments + evidence map | **Beta target (current)** |
| Level 3 | Paper proof TeX | 1-year freeze attempt; Anchored Ownership Safety first |
| Level 4 | Mechanized small-step model (Coq/Lean) | Post-1.0 academic collaboration |
| Level 5 | Mechanized full compiler | Long-term (CompCert / CakeML pattern) |

Beta closure does not require Level 3+. Level 2 is an evidence target for the
frozen subset, not a license to claim production-grade proof. Insisting on
Level 4-5 for beta would push beta back 3-5 years; claiming those levels early
would be dishonest.

The 1-year freeze after beta is the right window to attempt one paper
proof (Level 3) for one core theorem — the recommended candidate is
`Anchored Ownership Safety` from `docs/semantics/04_ownership_abi.md`,
because it directly grounds the Slot model and is narrow enough to be
provable without proving the entire language.

## 10. Summary

The author's worry has three components, and the audit splits them
honestly:

1. **"Slot is not rigorous"** — partially true: Slot is runtime-validated,
   not statically proven. But Slot is not the borrow checker. The static
   layer is 5 components above Slot, and Tier 1 of that layer is
   regression-backed.

2. **"Dijkstra was borrowed loosely"** — false: structured programming /
   CFG is applied as Dijkstra's settled theorem allows. Predicate
   transformer is not used. Mutual exclusion application is partial and
   honest about being so.

3. **"Marketing outpaces implementation"** — partially true: §6 lists the
   real danger zones, and §8 lists phrases to avoid. Documentation
   discipline closes the gap; no implementation work is required to fix
   the marketing drift, only careful wording.

The static guarantees Pergyra Beta delivers are not Rust-equivalent. They are
specific compile-time rejects for the covered boundaries, backed by runtime
generation/token checks and conservative rejection elsewhere. Future evolution
toward Option C lift, AIR Phase 1, and Level 3 paper proof is documented in
the related closure documents, but must not be marketed as current behavior.

The Slot model is not "I wish it worked like this." It is a
well-defined runtime-validated handle with a documented static layer
above it, written in three tiers with regression evidence at each tier.
That is the audit's honest answer.
