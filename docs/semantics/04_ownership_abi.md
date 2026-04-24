# 04. Ownership / ABI Proof Obligations

Last updated: 2026-04-25

Status: `IN PROGRESS / BLOCKER`

Keywords and surfaces: `own`, `ref`, anchored slot handles, slot boundaries, runtime ABI ownership.

## Stable Surface

- Anchored slot-handle own/ref subset.
- Boundary-visible aggregate provenance.
- Movable value transfer/borrow where explicitly covered.
- Ownership diagnostics for destructure/member/container/return/channel/helper-chain paths.
- Arena discipline: scratch/result/persistent/runtime lanes.

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

## Theorem: Arena Lifetime Non-Escape

Values allocated in a scratch arena cannot be returned, cached, or stored in long-lived runtime ABI state unless copied into a result/persistent lane.

Assumptions:

- Scratch, result, persistent, and runtime-owned lanes are documented per subsystem.
- Long-lived metadata stores stable indexes or owned copies, not scratch pointers.

Current evidence:

- Arena direction is fixed as `Arena + Index reference + lane-specific arena separation`.
- Several semantic and backend scratch/result paths have been split.
- Stable runtime string ABI exports are documented as `runtime-borrowed string` values: the caller must not free them, and they are valid until the next mutation of the corresponding runtime registry or snapshot.
- `runtime-abi-lifetime-test-smoke` verifies that stable intent/authority string export functions return borrowed runtime state and do not allocate or free in the export body.

Remaining proof obligation:

- Extend the same lifetime gate to additional helper payloads and runtime-owned handles as they become beta-stable.

## Theorem: ABI Ownership Parity

C and LLVM must agree on who owns every stable runtime value returned through the ABI.

Current evidence:

- ABI same-process and backend compare tests cover many current runtime paths.
- Runtime-borrowed string exports for intent observability and authority failure snapshots now have an explicit smoke gate.

Remaining proof obligation:

- Add explicit ownership assertions for helper payloads and runtime-owned handles beyond the current string export surface.
