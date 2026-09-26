# Array member and pixel storage closure directive

Status: NATIVE P0 VERIFIED; SELF-HOST OPEN
Base revision: `68a82abfe37e7c93c879baa0a6b0baae0cf73c45`
Working tree: shared and dirty; `/root` is the sole writer.

This file coordinates read-only investigation. It is not a semantic, ABI,
performance, or completion owner.

## Shared objective card

- Objective: choose an evidence-backed Pergyra CPU RGBA buffer contract and
  close the native C loss of `ArrayPush` length write-back for an exact nested
  logical-record member receiver, with C/LLVM execution parity and explicit
  unsupported-path diagnostics.
- Priority: semantic correctness and one receiver/place fact owner; fail-closed
  unsupported shapes; bounds/lifetime negatives; repeatable in-process
  performance evidence; then patch size.
- Fact owner: the existing typed receiver/place and collection mutation facts;
  the investigation must identify their exact native owner before editing.
- Last legitimate consumer: the native C array runtime-call emitter that must
  receive the address of the exact mutable member storage.
- Forbidden fallback: source-text reconstruction, name/type-only recovery,
  Alrescha-specific lowering, C/LLVM agreement as an oracle, or claiming a
  Slot/PinWrite route that the current language and backends do not execute.
- Verification gate: the Alrescha nested-member reproducer independently prints
  `MEMBER ARRAY PUSH PASS` on native C and LLVM; flat and nominal pixel
  candidates produce an independently fixed checksum; invalid bounds and
  use-after-release are rejected or trap under their owned contracts; a
  program-internal repeated benchmark reports allocation/growth conditions and
  a warm distribution without process-startup time.
- Falsifying case: `ArrayPush(frame.buffer.values, Value(42))` returns normally
  while the caller still observes length zero on native C.

## Independent read-only scopes

1. C owner map: trace semantic place/receiver facts through MIR and the native C
   emitter, compare LLVM's member-lvalue path, and identify the smallest owner
   fix. Do not edit.
2. Storage strategy map: verify which of flat `Array<Int>`, nominal element
   arrays, nested member arrays, and Slot/PinWrite are actually admitted; design
   one equal-workload in-process benchmark and independent checksum oracle. Do
   not edit.
3. Gate and safety map: locate existing bounds, release, mutation, self-host,
   and performance gates; propose the narrow integration gate and detect
   regressions hidden by backend-to-backend comparison. Do not edit.

Agents may use `rg`, inspect source/docs/tests, and run focused commands only in
uniquely named `.tmp` directories. No agent may edit, stage, commit, push,
install, clean, or reuse another run's artifact.

## Integration

Integration owner: `/root`.

One integration gate will be added or extended only after the exact owner and
supported storage shapes are observed. Agent outputs are observations and
proposals, never implementation or closure evidence. Final integration reruns
the reproducer, checksum workload, safety negatives, affected self-host gate,
and repeated benchmark from fresh artifacts.

## Integration result

- Clean pre-fix native C reproduced caller-visible length loss; LLVM passed.
- Current native C and LLVM both pass the exact Alrescha reproducer and the
  fixed nested/one-level/order oracle.
- Array temporary receivers fail closed at semantic admission.
- Bounds and release negatives pass their existing owner gates.
- The 101-sample checksum/performance comparison and its limits are recorded in
  `docs/audits/2026-09-27_array_member_pixel_storage_closure.md`.
- Installed/default self-host C still violates call argument order, so no
  self-host or SoT closure is claimed here.
