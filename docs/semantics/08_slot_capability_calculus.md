# 08. Slot Capability Calculus

Last updated: 2026-04-27

Status: `IN PROGRESS / PROOF-SKETCH`

This document defines the mathematical model for the stable Slot capability
surface: `Slot<T>`, `SecureSlot<T>`, token-guarded access, generation checks,
and the Pin/Lease fast path. It is a proof obligation and operational model,
not a claim that the entire runtime has been mechanically verified.

The accompanying Coq file in `docs/semantics/proofs/SlotCalculus.v` is a
minimal mechanized sketch for selected Slot capability invariants: stale handle
read/write/release rejection, mode-specific issued-token requirements for
read/write/pin/release, unissued-token read/write/pin/release rejection,
pinned-handle release rejection, and pin non-eviction. It is beta evidence only
when a CI gate type-checks the artifact with Coq. The beta contract must still
describe it as a proof sketch, not completed mechanized proof for the whole
language.

## Positive Claim: Slot Is A Modular Resource Boundary

Pergyra's ownership thesis is not "memory as address ownership". Pergyra
exposes memory as a modular resource boundary.

```text
Pergyra does not expose memory as address ownership.
Pergyra exposes memory as a modular resource boundary.
A Slot is the stable language-level boundary; the backend handle below it is replaceable.
```

This is the reason Slot appears in memory, authority, zone/world transfer,
device access, projection, and intent handoff. A source-level Slot does not
promise a stable raw pointer. It promises a stable contract boundary. The
runtime/backend representation below the boundary may be a C pointer, arena
index, generational handle, device buffer id, file handle, database row handle,
or remote-world handle, provided the Slot capability predicates and failure
classes remain unchanged.

The model can be summarized as:

```text
Slot = address abstraction + ownership boundary + capability gate + replaceable backend handle
```

This positive claim is separate from the negative claim below. Slot being a
modular resource boundary makes it the right abstraction substrate for Pergyra;
it still does not make Slot a Rust-style borrow checker.

## Negative Claim: Slot Is Not A Borrow Checker

The Slot calculus is a runtime capability calculus. It is not, by itself, a
Rust-style borrow checker.

This document proves and specifies runtime facts:

- stale handles cannot satisfy the stable handle predicates;
- unissued or mismatched tokens cannot satisfy the stable capability
  predicates;
- pinned slots cannot be released or evicted before unpin;
- C and LLVM must expose the same stable failure classes for these facts.

This document does not prove static borrow facts:

- no general aliasing-XOR-mutability theorem for arbitrary `ref` values;
- no general lifetime theorem for borrowed values across arbitrary CFG regions;
- no proof that every cleanup path inserts `unpin` / `release` exactly once;
- no proof that pinned views cannot escape without the CFG no-escape checker;
- no proof that async/task/channel boundaries preserve borrow validity without
  the body-dataflow summaries in `docs/100_beta_readiness_checklist.md` section
  `0b`.

The beta-safe claim is therefore:

```text
Slot = runtime capability + generation + token + pin-state safety.
Static borrow safety = ownership classifier + CFG/body dataflow +
channel/task boundary rules + token transport rejects + Slot runtime checks.
```

Any public wording that says "Slot is Pergyra's borrow checker" is rejected by
this proof pack. The honest wording is: "Slot is runtime-validated; Pergyra's
borrow-checker-equivalent is the static ownership/CFG layer above Slot."

## Stable Surface

- `Slot<T>` and `SecureSlot<T>` handles with generation checks.
- Token-guarded read/write/release for secure slots and authority-bearing
  runtime boundaries.
- Pin/Lease runtime ABI fast path: `PgyPinnedView`, `PergyraSlotPin`,
  `PergyraSlotUnpin`.
- Generated inline slot ABI fast path: `PgyPinnedSlotView_*`,
  `PgyPinnedSecureSlotView_*`, `pgy_pin_read_*`, `pgy_pin_write_*`,
  `pgy_unpin_*`, and secure pin wrappers. This layer preserves the current
  generated `PgySlot_*` layout and validates occupied/token state at lease
  entry; the harder non-eviction guarantee still belongs to the table-backed
  `PgyPinnedView` / `SlotManager` runtime.
- Pinned slots cannot be released or evicted until they are unpinned.
- Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` is accepted
  as a typed-view lexical block. Full runtime `PgyPinnedView` lowering remains
  internal until CFG cleanup insertion and C/LLVM parity are closed.
- Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` now reaches
  HIR and MIR as explicit pin-region metadata: source slot, view binding, and
  read/write mode are preserved on CFG blocks. This is the compiler fact that
  cleanup-edge insertion consumes through MIR `pin-unpin-cleanup-edge`
  metadata. The generated inline runtime wrappers now exist for C/LLVM parity;
  C source-block emission uses cleanup hooks for recognized pin blocks, and C
  plus LLVM MIR successor/return exits emit explicit typed pin/unpin calls for
  the frozen source-level pin backend-compare fixtures. Current backend parity
  evidence covers normal successor exit, direct return inside the pin block,
  conditional branch-to-return exit, and loop `break`/`continue` exit. Active
  view + `defer` cleanup registration is rejected semantically so cleanup
  helpers cannot capture a view beyond its pin scope. The remaining proof
  blocker is expanding that evidence to exceptional and cancellation cleanup
  families and recording the corresponding `DropOnce` / `ReleaseAfterUnpin`
  theorem row.
- The source-level typed-view layer rejects `Release(source)` and `Move(source)`
  while a `ReadView<T>` or `WriteView<T>` derived from `source` is live. This is
  the static front door for the runtime "pinned slots cannot be invalidated"
  invariant.
- The same layer rejects direct owner writes while any typed view over that slot
  is live, and rejects direct owner reads while a `WriteView<T>` is live. The
  proof obligation is that the lease cannot be bypassed through the original
  slot identifier.
- Slot sugar is interpreted through the same proof obligation: assignment sugar
  is an owner write, and value-position owner identifiers are owner reads.
- Function-call boundaries are interpreted conservatively: an owning source slot
  cannot be passed to an `own`/`ref Slot<T>` helper while a typed view over that
  source is live.
- Container-store boundaries are interpreted conservatively too: array
  literals and `ArrayPush` / `ArraySet` / `ListPush` / `ListSet` / `SetAdd` /
  `MapSet` / `QueuePush` cannot store or forward the owning source slot while a
  typed view over that source is live.
- Return boundaries are treated the same way: `return source_slot` cannot
  forward the owning source slot while a typed view over that source is live.
- `Box<T>` is not a resource-handle storage boundary in the beta-stable
  surface. `Box(source_slot)` and `BoxSet(box, source_slot)` are rejected
  because they would introduce a second storage owner for the same slot handle.

Out of beta:

- Raw user-visible `void *` pin handles.
- Pinning `QubitSlot`.
- Pinned views crossing `await`, `spawn`, `async`, `parallel`, callback, or
  channel boundaries.
- General ownership/lifetime proof for arbitrary aggregates.
- Complete cryptographic proof for token construction.

## Semantic Domains

```text
Sigma : SlotId -> SlotRecord | bottom
Delta : set Capability
Gamma : variable -> Handle

SlotRecord = <value, type_tag, generation, ttl, pin_state>
Handle     = <slot_id, generation>
PinState   = Unpinned | Pinned
Mode       = R | W | Release | Pin
```

`Verify(Delta, slot_id, generation, mode)` is true when the current execution
context holds a capability for that slot generation and mode. The calculus
treats `Verify` as an abstract predicate; runtime crypto/token implementation
must refine it without exposing forgeable source-level constructors.

## Transition Rules

Claim:

```text
slot notin dom(Sigma)
Sigma' = Sigma[slot -> <null, T, gen=1, ttl=infinity, Unpinned>]
Delta' = Delta union { master_cap(slot, 1) }
---------------------------------------------------------------
<Gamma, Sigma, Delta> --Claim(T)-->
<Gamma[x -> <slot, 1>], Sigma', Delta'>
```

Read:

```text
Gamma(x) = <slot, gen>
Sigma(slot) = <value, T, gen, ttl, pin_state>
Verify(Delta, slot, gen, R)
---------------------------------------------------------------
<Gamma, Sigma, Delta> --Read(x)--> value
```

Write:

```text
Gamma(x) = <slot, gen>
Sigma(slot) = <value, T, gen, ttl, pin_state>
Verify(Delta, slot, gen, W)
---------------------------------------------------------------
<Gamma, Sigma, Delta> --Write(x, value')-->
<Gamma, Sigma[slot -> <value', T, gen, ttl, pin_state>], Delta>
```

Pin:

```text
Gamma(x) = <slot, gen>
Sigma(slot) = <value, T, gen, ttl, Unpinned>
Verify(Delta, slot, gen, Pin)
---------------------------------------------------------------
<Gamma, Sigma, Delta> --Pin(x)-->
<Gamma, Sigma[slot -> <value, T, gen, ttl, Pinned>], Delta>
```

Unpin:

```text
Gamma(x) = <slot, gen>
Sigma(slot) = <value, T, gen, ttl, Pinned>
Verify(Delta, slot, gen, Pin)
---------------------------------------------------------------
<Gamma, Sigma, Delta> --Unpin(x)-->
<Gamma, Sigma[slot -> <value, T, gen, ttl, Unpinned>], Delta>
```

Release:

```text
Gamma(x) = <slot, gen>
Sigma(slot) = <value, T, gen, ttl, Unpinned>
Verify(Delta, slot, gen, Release)
---------------------------------------------------------------
<Gamma, Sigma, Delta> --Release(x)-->
<Gamma, Sigma[slot -> bottom], Delta>
```

There is intentionally no release rule for `Pinned`.

## Theorem: ABA Safety

If a handle `<slot, gen>` was issued for generation `gen`, and the slot has
since been released and reallocated at generation `gen' != gen`, then any
read/write/release using the old handle is rejected or hard-fails in the stable
panic class.

Current evidence:

- Runtime slot manager stores generation counters and rejects stale handles.
- `make test-security` (142/142 passed locally) covers stale-generation
  read/write/pin/release rejection and `SlotIsValid` false for
  stale-generation handles.
- The current C ABI is a 32-bit `slotId` / `generation` handle, so ABA safety
  also depends on never using the zero-id sentinel or wrapping the id space.
  `SlotClaim` tombstones those states by returning `SLOT_ERROR_OUT_OF_MEMORY`
  instead of reusing an old id; `make test-security` covers these guards.
- `make test-security` also covers tampered pinned-view generation rejection and
  double-unpin rejection, so unpin cannot silently clear a pin without matching
  the issued view.
- Slot panic contract gates released-slot and double-release hard-fail classes.
- `docs/semantics/proofs/SlotCalculus.v` sketches the
  `stale_handle_read_impossible`, `stale_handle_write_impossible`,
  `stale_handle_release_impossible`, `zero_slot_id_claim_impossible`,
  `max_slot_id_claim_impossible`, `tampered_view_unpin_impossible`, and
  `double_unpin_impossible` lemmas for generation/id/view mismatch.

Remaining obligation:

- Add generated C/LLVM parity coverage for stale generation handles when the
  stable source surface can express that path without test harness internals.

## Theorem: Token Unforgeability

Source-level code cannot manufacture a capability that satisfies
`Verify(Delta, slot, gen, mode)` unless the runtime/compiler issued that
capability through a stable claim/handoff/token API.

Current evidence:

- Token material is not a normal source-level construct in the stable subset.
- Invalid secure-slot token and denied capability paths are represented as
  hard-fail or explicit rejection, not silent fallback.
- Authority failure snapshots do not expose secret token material.
- Runtime token validation is bound to the current `SlotEntry` generation and
  stored token generation, so stale handles and revoked tokens are rejected.
- `make test-security` covers revoked-token read/write/pin/release rejection.
- Raw `SlotRelease` cannot release secure slots; successful secure release must
  use `SlotReleaseSecure` after token validation.
- `make runtime-panic-abi-test-smoke` covers forged zero-token
  read/write/release rejection for both inline C runtime and exported
  C/LLVM-linkable runtime entrypoints.
- `docs/semantics/proofs/SlotCalculus.v` sketches mode-specific
  `handle_*_requires_issued_token` and `unissued_token_*_impossible` lemmas for
  read, write, pin, and release in the capability-environment part of this
  theorem.

Remaining obligation:

- Extend authority-token mismatch C/LLVM parity tests to every stable authority
  access form.
- Keep unsupported authority token transport as semantic explicit reject.

## Theorem: Pin Non-Eviction

If `Sigma(slot).pin_state = Pinned`, no `Release` or background `Evict` rule may
produce `Sigma'(slot) = bottom` until an `Unpin` transition occurs.

Current evidence:

- Runtime pinning tests cover release while pinned, TTL cleanup skip while
  pinned, invalid token rejection, concurrent secure write rejection, and
  release-after-unpin persistence.
- Evidence command: `make test-security`.
- `docs/semantics/proofs/SlotCalculus.v` sketches the `pin_non_eviction`
  lemma for the small-step model and `pinned_handle_release_impossible` for the
  stable handle predicate.

Remaining obligation:

- Keep the Coq CI gate green before treating the sketch as mechanized evidence.
- Connect source-level pin syntax to CFG cleanup insertion before making it
  stable. Every early return, break, continue, panic cleanup, and cancellation
  edge must unpin exactly once.
- Add C/LLVM backend parity for stable pin usage once the source surface is
  opened.

## Bridge Obligation: Borrow-Checker-Equivalent Safety

The Slot calculus becomes part of a borrow-checker-equivalent claim only when
the static layer proves the following additional facts for the stable source
surface:

```text
NoEscape(view, region)
NoSuspend(view, region)
WriteExclusive(slot, region)
DropOnce(owner, all_cfg_exits)
ReleaseAfterUnpin(slot, all_cfg_exits)
NoUnsupportedTokenTransport(token, boundary)
```

Required interpretation:

- `NoEscape` is a CFG fact: a `ReadView<T>`, `WriteView<T>`, or future
  `PinnedView<T>` cannot be returned, stored in longer-lived state, captured by
  a spawned task, or sent through a channel unless a stable ownership transfer
  rule explicitly admits it.
- `NoSuspend` forbids live views across `await`, `spawn`, `async`, `parallel`,
  callback, `defer`, or channel handoff boundaries.
- `WriteExclusive` is the aliasing-XOR-mutability baseline for `WriteView<T>`:
  while a write view is active, no other read or write view of the same slot may
  be active, and the owning slot name cannot be used as a parallel read/write
  path. Shared read/read views remain allowed.
- `DropOnce` and `ReleaseAfterUnpin` are cleanup facts over all normal and
  exceptional CFG exits.
- `NoUnsupportedTokenTransport` keeps authority/secure tokens out of task,
  channel, cancel, and container surfaces that do not define a stable transfer
  rule.

Current evidence:

- Existing `ViewRead(...)` / `ViewWrite(...)` semantic surface enforces the
  first stable `WriteView<T>` exclusivity slice.
- Parallel `ref`/`own`, ownership-bearing channel helpers, token transport, and
  direct named-spawn borrowed-ref boundaries are covered in the CFG/body
  closure checklist.
- Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` now reaches
  the same `ViewRead(...)` / `ViewWrite(...)` semantic diagnostics. The
  typed-view read/write path has C/LLVM parity for plain and secure slot cases,
  including a sequential mixed read case, and `Release(source)` / `Move(source)`
  are statically rejected while an active typed view over `source` is live.
  Direct owner write/read bypass cases, including slot assignment sugar and
  value-position owner identifier sugar and `own`/`ref Slot<T>` helper calls,
  are also rejected for the covered typed-view subset. Explicit runtime
  pin/unpin lowering now has C/LLVM parity for normal successor exit, direct
  return inside a pin block, conditional branch-to-return exit, and loop
  `break`/`continue` exit. Active view + `defer` registration is rejected at
  semantic time. The remaining cleanup/no-escape/backend-parity bridge is the
  exceptional/cancellation exit family.

Remaining obligation:

- Do not upgrade Slot marketing from "runtime capability safety" to
  "borrow-checker-equivalent safety" until section `0b` CFG/body dataflow
  closes these bridge facts and section `4` ABI ownership confirms matching
  C/LLVM lowering.

## Beta Acceptance Rule

The Slot capability calculus is beta-aligned only when:

- The stable runtime ABI matches these transition rules.
- Unsupported source syntax is explicitly rejected.
- Diagnostics use stable `PGY_SEM_PIN_*` and panic vocabulary.
- C and LLVM agree on hard-fail class and observable state.
- Mechanized proof is described honestly: proof sketch until checked by CI,
  evidence only for the modeled invariant, never a full language proof.
- Borrow-checker-equivalent language claims are allowed only through the bridge
  obligation above, never from Slot runtime checks alone.
