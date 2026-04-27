# 08. Slot Capability Calculus

Last updated: 2026-04-26

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

## Stable Surface

- `Slot<T>` and `SecureSlot<T>` handles with generation checks.
- Token-guarded read/write/release for secure slots and authority-bearing
  runtime boundaries.
- Pin/Lease runtime ABI fast path: `PgyPinnedView`, `PergyraSlotPin`,
  `PergyraSlotUnpin`.
- Pinned slots cannot be released or evicted until they are unpinned.
- Source-level `pin slot as view { ... }` remains an explicit reject until CFG
  cleanup insertion and C/LLVM parity are closed.

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
- `make test-security` (132/132 passed locally) covers stale-generation
  read/write/pin/release rejection and `SlotIsValid` false for
  stale-generation handles.
- Slot panic contract gates released-slot and double-release hard-fail classes.
- `docs/semantics/proofs/SlotCalculus.v` sketches the
  `stale_handle_read_impossible`, `stale_handle_write_impossible`, and
  `stale_handle_release_impossible` lemmas for generation mismatch.

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

## Beta Acceptance Rule

The Slot capability calculus is beta-aligned only when:

- The stable runtime ABI matches these transition rules.
- Unsupported source syntax is explicitly rejected.
- Diagnostics use stable `PGY_SEM_PIN_*` and panic vocabulary.
- C and LLVM agree on hard-fail class and observable state.
- Mechanized proof is described honestly: proof sketch until checked by CI,
  evidence only for the modeled invariant, never a full language proof.
