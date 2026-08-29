# ArrayString process Args target projection result — 2026-08-29

Status: `LOCAL IMPLEMENTATION CANDIDATE`

Exact base: `3beff662f2f536ba2bb45f1ab056584f2d5158b1` on
`origin/main`.

This audit records evidence for one reached executable consumer family. It
does not own compiler semantics, registry status, or a successor rung.

## Result

The scalar C and LLVM process-Args adapters now receive the target-qualified
`DirectMirArrayStringAbiProjection` already derived by their program emission
roots. Both cross-seal it against the carried
`DirectMirScalarProgramArrayStringAbiFact` before materializing Args.

LLVM no longer writes literal ArrayString storage alignment in the Args
alloca, zero initialization, and result load. Those three rows derive
`projection.storage.align`. The C adapter continues to use the projected
storage owner's private `pgy_as` API. argc/argv capture and copied-string
ownership are unchanged.

## Falsifying evidence

- `direct_mir_scalar_process_args_direct_call_owner.sh` executes the nested
  direct-call program through C and LLVM and checks equal output.
- It rejects Args target-name drift, Args call-syntax drift, outer call-syntax
  drift, and the carried ArrayString parameter-layout alignment drift. Each
  requested artifact remains absent on failure.
- The focused gate and component contract require projection readiness and
  carriage in both backends, require projected LLVM storage alignment, and
  reject restoration of all three literal storage-alignment rows.
- Generated LLVM still spells alignment `8`, which is the admitted canonical
  value; current source derives it from the projection rather than owning it.

## Observed local gates

- current-source Pergyra-built DRV-2 installation: PASS
- native-oracle `driver_bootstrap_main.pgy` C emission: PASS, zero errors and
  three pre-existing redundant-`who` warnings
- focused C/LLVM Args parity and four negative mutations: PASS
- full self-host component contract: PASS
- SoT authority edge: PASS, `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS; Coq/Rocq explicitly skipped because unavailable
- single-owner, hard-contract, likeness `4493/4493`, and documentation quality:
  PASS

The first component run correctly rejected a 33-line LLVM Args owner against
its 30-line cap. The same implementation was compressed to 29 lines; the cap
was not raised and the full component contract then passed.

## Registry boundary

The reached program-extension and ArrayString ABI rows now record both Args
adapters, the focused gate, projection-less Args fallback, and literal LLVM
storage-alignment fallback. The ArrayString ABI row remains `BRIDGE`: parameter
binding, value-result transfer, owned return, mutation, cleanup, and other
expression materializers still require consumer migration. No `56/31/1`
closure is claimed.

Commit, push, and exact-head CI remain the publication boundary.
