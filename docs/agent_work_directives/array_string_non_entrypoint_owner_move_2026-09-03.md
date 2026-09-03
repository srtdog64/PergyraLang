# ArrayString non-entrypoint straight-line owner move — 2026-09-03

Status: `IMPLEMENTATION COMPLETE — EXACT CI GREEN`

Exact base: `fabf8ede617f4539d042ccf5ff8dc35cc4f1cd70` on `origin/main`.

This directive coordinates one reached executable prerequisite of
`abi.mir_array_string_layout_projection`. It does not close that registry row
or admit general control-flow ownership analysis.

## Shared objective card

- Objective: let a named `Array<String>` local owned by any admitted
  single-block routine move exactly once at its last use into an admitted
  `owner-handle` parameter, rather than limiting the existing move receipt to
  the entrypoint routine.
- Priority: preserve exact caller/operation/expression/local/callable/parameter
  and ABI identity; consume the routine/block inventory already admitted by the
  graph-storage owner; retain per-routine last-use proof and duplicate
  rejection; keep C/LLVM cleanup target-neutral; then minimize the patch.
- Fact owner: `DirectMirScalarProgramOwnedArrayStringMoveFact` remains the sole
  ordered digest-sealed move receipt. Its producer may carry the already
  resolved caller routine instead of requiring routine zero.
- Last legitimate consumer: the shared ArrayString cleanup policy suppresses
  caller cleanup only for the exact `(routine, local)` row consumed by both C
  and LLVM emitters.
- Forbidden fallback: routine or callable name matching, source/MIR rescan,
  backend-local move inference, unconditional caller cleanup suppression,
  whole-program single-block assumptions, later use, duplicate local
  retirement, ABI drift, or widening to conditional, loop, member, parameter,
  literal, or fresh-result moves.
- Verification gate:
  `make self-host-direct-mir-scalar-owned-array-string-parameter-test-smoke`
  executes the entrypoint, non-entrypoint, and multiple-move focused owners in
  C and LLVM, proves terminal cleanup retirement, and rejects later use,
  duplicate retirement, and forged move carriage without publishing an
  artifact.

## Opening falsifier and edit scope

- The installed Pergyra-built DRV-2 produces a verified 11,538-byte MIR for a
  three-routine source whose middle routine creates and finally moves one
  `Array<String>` local. Both direct C and LLVM projections then fail closed
  with `direct MIR scalar program extension is invalid: code=19`.
- Allowed implementation is the existing move fact/readiness, its last-use
  proof, focused fixtures/gate, structural ratchets, registry residual wording,
  and current coordination/handoff records.
- Conditional CFG ownership, loop flow, member moves, moving an owned formal,
  unnamed values, semantic named-boundary policy, syntax, runtime, and other
  SoT rows are forbidden overlap.
- The primary task owns integration. Static owner gates have a 60-second
  budget, the focused parity gate has a five-minute budget, and broader gates
  run only after that slice is stable.

## Local result

- The existing row set now consumes routine-owned operation, block, and local
  flat partitions. Readiness no longer restricts `caller_routines` to zero;
  last-use proof still requires exactly one block with no successor,
  condition, return expression, or later local use.
- A current-source Pergyra-built DRV-2 emits C and LLVM for the three-routine
  fixture. Both artifacts transfer the exact local, omit caller cleanup, retain
  callee terminal cleanup, compile, run, and print `nested-released`.
- The same shape with `ArrayLength(values)` after transfer fails both backend
  projections and publishes no artifact. Existing entrypoint single/multiple
  move, carriage/pass/layout/target mutations, owned return, by-value, and
  value-result regressions remain green.
- The last-use owner is exactly 100 lines; the entrypoint and non-entrypoint
  focused gates are 132 and 98 lines. Component, SoT edge, Gate single-owner,
  protocol registry, hard contract, and build-source inventory are green.
  Registry census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`.
- Implementation commit `eed7f7229699770bcda656e7a2947a5f043cbfcc`
  is on `origin/main`. Exact push run `33720973406` completed 30/30 green in
  35m36s: full self-host 35m15s, build-linux 21m49s, sanitizers 12m55s,
  codegen bootstrap 8m57s, Windows 9m00s, Rocq 9, TSan, macOS, and all twenty
  backend-compare shards. The lease is released.
