# ArrayString multiple straight-line owner moves — 2026-09-03

Status: `IMPLEMENTATION CANDIDATE — LOCAL GREEN, PUBLICATION PENDING`

Exact base: `b026f5a6eadb2e0ed3bf4ad684f47fc2d81d39e0` on `origin/main`.

This directive coordinates one reached executable prerequisite of
`abi.mir_array_string_layout_projection`. It does not close that registry row
or admit general control-flow ownership analysis.

## Shared objective card

- Objective: replace the scalar owner-handle `Array<String>` move receipt with
  one sealed row set so two or more independent entrypoint locals may each move
  exactly once into admitted direct-call parameters.
- Priority: exact caller/operation/expression/local/callable/parameter and ABI
  identity, per-local last-use proof, duplicate rejection, target-neutral
  carriage, C/LLVM cleanup consumption, negative ratchet, then patch size.
- Fact owner: `DirectMirScalarProgramOwnedArrayStringMoveFact` owns the ordered
  parallel rows and their digest. It is produced once from admitted callable,
  graph-storage, expression, and ArrayString ABI facts.
- Last legitimate consumer: the shared ArrayString cleanup policy may suppress
  caller cleanup only for an exact `(routine, local)` row; C and LLVM emitters
  must not infer moves independently.
- Forbidden fallback: type/name inference in a backend, source/MIR rescan,
  caller cleanup after transfer, whole-fact invalidation merely because a
  second independent row exists, duplicate local retirement, later use,
  missing/drifted ABI identity, or widening to conditional and loop flow.
- Verification gate:
  `tests/self_hosted/parity/direct_mir_scalar_owned_array_string_parameter_owner.sh`
  executes the single- and two-move forms in C and LLVM, checks exact output and
  terminal drops, and rejects use-after-move plus forged carriage/pass/layout/
  target and duplicate-local paths without artifacts.

## Scope and integration

- Allowed edits are the move fact/admission/readiness, shared cleanup policy,
  focused fixture/gate, structural inventory, registry residual wording, and
  current handoff/progress records.
- Conditional, loop, member, parameter, fresh-result, literal, owned-return,
  syntax, and runtime changes are forbidden overlap.
- The primary task owns integration. The focused gate has a five-minute budget;
  static owner gates have a 60-second budget. Broader CI is run only after the
  focused slice is stable.
- The observed opening falsifier is source-to-MIR success followed by C and
  LLVM `direct MIR scalar program extension is invalid: code=19` and no target
  artifact for the two-move fixture.

## Local result

- The scalar move receipt is now an ordered, digest-sealed parallel row set.
  Every row carries caller, operation, expression, local, callable, parameter,
  and ArrayString ABI identities; readiness rejects cardinality drift,
  duplicate `(caller, local)` retirement, and per-row ABI mismatch.
- The existing single-move C/LLVM runtime and mutation negatives remain green.
  The new two-move fixture emits both transfers, removes both caller cleanups,
  and prints exactly `released-two` in C and LLVM. Reusing the same local for a
  second transfer fails with extension code 19 and publishes no artifact.
- Owned return, by-value, and value-result ArrayString C/LLVM gates are green.
  The value-result gate had retained a pre-target-projection named-type string;
  it now checks the owner-issued literal aggregate and alignment actually
  consumed by the LLVM emitter.
- The focused Make target, component contract, SoT edge/single-owner protocol,
  hard self-host contract, and build-source inventory are locally green. A
  current Pergyra-built DRV-2 was regenerated successfully. Commit, push, and
  exact-head CI remain pending; census stays `CLOSED=55 BRIDGE=32 ACTIVE=1`.
