# 04. Ownership / ABI Proof Obligations

Last updated: 2026-04-26

Status: `IN PROGRESS / BLOCKER`

Keywords and surfaces: `own`, `ref`, anchored slot handles, slot boundaries, runtime ABI ownership.

## Stable Surface

- Anchored slot-handle own/ref subset.
- Boundary-visible aggregate provenance.
- Movable value transfer/borrow where explicitly covered.
- Ownership diagnostics for destructure/member/container/return/channel/helper-chain paths.
- Arena discipline: scratch/result/persistent/runtime lanes.
- SecureSlot and authority token invariants for the stable anchored boundary subset.

Out of beta:

- General ownership lattice.
- Non-anchored general value own/ref.
- Universal move semantics for every aggregate shape.
- Arbitrary runtime pointer ownership transfer.

## Judgments

```text
Gamma; ResourceState |- borrow(x) ok
Gamma; ResourceState |- move(x) => ResourceState'
Gamma; ResourceState |- release(slot) => ResourceState'
ABI |- returned_value owns lane
ABI |- scratch_value does not escape
Gamma; ResourceState |- secure_read(slot, token) ok
Gamma; ResourceState |- authority_use(zone, token) ok
```

## Theorem: Anchored Ownership Safety

Anchored slot-handle operations cannot observe a released slot, cross an invalid boundary, or duplicate a move-only resource without a contract violation or hard-fail.

Assumptions:

- Stable own/ref applies only to anchored slot-handle boundaries.
- Released slots and invalid tokens are invariant breaks or hard failures.
- General ownership is not accepted as stable beta surface.

Current evidence:

- Ownership classifier fixes the stable subset.
- Channel, destructure, member, return, container, and helper-chain ownership regressions exist.
- Non-anchored/general own/ref is an explicit reject or out-of-beta surface.

Remaining proof obligation:

- Finish ABI ownership seams for returned strings/helper payloads and runtime-owned values.

## Theorem: Secure Token Unforgeability

Secure slot tokens and authority-bearing tokens cannot be forged, copied into an unsupported trust boundary, or used to access a slot/zone authority boundary they were not issued for.

Required invariants:

- Token material is not constructible by source-level expressions outside compiler/runtime issuance points.
- Secure slot token mismatch cannot read, write, release, or otherwise mutate the protected slot.
- Authority-bearing tokens cannot be transported through unsupported channels or stored in stable beta containers unless the surface explicitly defines that transfer.
- Runtime snapshots and observability strings must not expose secret token material.

Current evidence:

- Secure slot read/write/pin/release paths validate token pairing.
- Raw `SlotRelease` cannot release secure slots; secure release requires
  `SlotReleaseSecure` after token validation.
- Stale-generation handles and revoked tokens are rejected by the runtime Slot
  capability tests.
- Forged zero-token read/write/release is covered for both inline C runtime and
  exported C/LLVM-linkable runtime entrypoints.
- SecureSlot token ABI is build-mode stable: debug/release inline C, exported
  runtime, and LLVM-linkable runtime all use the same `PgyToken<T>` layout with
  `can_write` and `can_read` capability bits. Release-mode inline secure-slot
  reads/writes/releases keep hard-fail token and occupancy checks, and the
  legacy release-mode SecureSlot macro has been removed; only plain `Slot<T>`
  has a zero-overhead release layout.
- The ABI spec carries matching debug/release SecureSlot layout rows for every
  stable primitive payload (`Int`, `Long`, `Float`, `Double`, `Bool`, `String`),
  and `make test-abi` checks runtime size and token offsets against that spec.
- Runtime authority failure surface exposes reason/code state for missing-zone,
  missing-participant, and authority-token-mismatch without exposing secret
  token material.
- Authority token mismatch has C/LLVM ABI and backend-compare coverage through
  `authority_failure_abi` and `authority_failure_surface`.
- Unsupported beta transport of authority-bearing `Token<T>` is rejected on
  blocking channel send/receive, non-blocking and timeout channel helpers,
  channel close, cancellation payloads, and direct named `spawn` boundaries.
- `runtime-authority-contract-test-smoke` and `runtime-abi-lifetime-test-smoke` guard parts of the runtime ABI vocabulary and borrowed-string lifetime surface.

Remaining proof obligation:

- Extend the same token-transport reject gate whenever a new beta transport
  surface is admitted.

## Theorem: Authority Transfer Single-Owner

Zone authority transfer and handoff cannot create two active owners for one authority boundary.

Required invariants:

- A handoff either materializes a new owner and invalidates the previous authority frontier, or fails with a recoverable authority/boundary state.
- A failed handoff cannot leave source and target both active.
- Projection and observability state after handoff must report the same authority owner on C and LLVM.

Current evidence:

- Handoff projection/world-state/layer-state frontier ABI cases exist.
- Authority guard snapshots and intent authority snapshots share the same runtime reason vocabulary.

Remaining proof obligation:

- Generalize handoff authority ownership beyond the currently covered frontier slices.
- Add explicit invalid-authority transfer tests for C and LLVM parity.

## Theorem: Arena Lifetime Non-Escape

Values allocated in a scratch arena cannot be returned, cached, or stored in long-lived runtime ABI state unless copied into a result/persistent lane.

Assumptions:

- Scratch, result, persistent, and runtime-owned lanes are documented per subsystem.
- Long-lived metadata stores stable indexes or owned copies, not scratch pointers.

Current evidence:

- Arena direction is fixed as `Arena + Index reference + lane-specific arena separation`.
- Several semantic and backend scratch/result paths have been split.
- Stable runtime string ABI exports are documented as `runtime-borrowed string` values: the caller must not free them, and they are valid until the next mutation of the corresponding runtime registry or snapshot.
- `runtime-abi-lifetime-test-smoke` verifies that stable intent last/history/active/recent and authority string export functions return borrowed runtime state and do not allocate or free in the export body.
- Stable string helper returns are `result-owned string` values: the caller owns
  and must eventually release the returned pointer unless a higher-level Pergyra
  runtime owner consumes it immediately.
- Stable string-array helper returns are `result-owned array` values: the array
  shell and its string payload elements are copied into result-owned runtime
  memory, not borrowed from source inputs or map storage.
- Stable integer file descriptors are `runtime-owned handle` values: the caller
  receives a numeric handle, while the runtime owns the backing `FILE *` table
  entry until `pgy_file_close` releases that slot for reuse.
- `runtime-abi-lifetime-test-smoke` also verifies that result-owned string and
  string-array helpers allocate/copy their payloads and do not return string
  literals, stack buffers, or source input pointers.
- The same smoke verifies that `pgy_file_open` reuses released runtime-owned
  handle slots and that `pgy_file_close` clears the runtime table entry.

Remaining proof obligation:

- Extend the same lifetime gate to additional runtime-owned handles as they
  become beta-stable.

## Theorem: ABI Ownership Parity

C and LLVM must agree on who owns every stable runtime value returned through the ABI.

Current evidence:

- ABI same-process and backend compare tests cover many current runtime paths.
- Runtime-borrowed string exports for intent observability and authority failure snapshots now have an explicit smoke gate.
- Result-owned string and string-array helper payloads now have an explicit
  allocation/copy smoke gate.
- The file-descriptor runtime-owned handle table now has an explicit release and
  reuse smoke gate.

Remaining proof obligation:

- Add explicit ownership assertions for runtime-owned handles beyond the current
  file-descriptor surface.
