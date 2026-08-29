# ArrayString scalar preamble projection result — 2026-08-29

This is an implementation result audit, not semantic authority or a whole-row
closure claim. Current source, the SoT registry, and executable gates override
it.

## Basis and bounded claim

- Exact base: `9619f9078fe5795a4e97a7bace64a8f039b62fe7` on
  `origin/main`.
- Objective: replace backend-local four-field ArrayString layout construction
  in the production scalar-program C/LLVM collection preamble with one
  target-qualified projection derived from the carried program ABI fact.
- Census before and after: `CLOSED=55 BRIDGE=32 ACTIVE=1`. This prerequisite
  does not close `abi.mir_array_string_layout_projection`.

## Replaced path

- `DirectMirScalarProgramArrayStringAbiProjectionFromFact` now admits canonical
  absence explicitly and otherwise derives one
  `DirectMirArrayStringAbiProjection` from the carried layout ID, size,
  alignment, and four offsets.
- The projection carries the layout ID, pointer element size/alignment, target
  capability fingerprint, storage fields, and LLVM indices. The C and LLVM
  emission roots derive it once per semantic program and pass it to the
  collection preamble.
- The private C `pgy_as` name remains distinct from public
  `PgyArray_String`. Its struct field names, size, alignment, and offset asserts
  now come from the target projection. LLVM aggregate spelling, GEP/extract
  indices, element size, and alignment now come from the same projection.
- Responsibility-named C and LLVM storage materialization owners keep the
  orchestration owners below their existing caps. No cap was raised.

## Falsifiers and observed evidence

- Baseline before edits:
  `direct_mir_scalar_bool_two_array_string_two_array_int_value_result_owner.sh`
  passed in 6.5 seconds.
- A fresh Pergyra-built installed DRV-2 was generated after the source edits.
  The focused mixed-copyout gate passed C/LLVM execution and now rejects a
  complete ArrayInt ABI row plus ID carried under ArrayString type, with no
  artifact in either backend.
- `one_mir_string_collection_builtin_projection.sh` passed Split/Join,
  ArrayString length/index, C/LLVM compilation, execution, and its mutation
  set. `direct_mir_scalar_process_args_direct_call_owner.sh` and
  `direct_mir_scalar_dir_walk_direct_call_owner.sh` also passed with the same
  installed driver.
- `tests/self_hosted_component_contract_smoke.sh` passed its full structural
  inventory. It now bans the retired scalar-preamble C struct literal, LLVM
  aggregate literal, and hard-coded data index, and tracks the new owners at
  35/50, 77/90, and 139/160 lines. The existing collection orchestration owners
  are 77/110 and 112/115.

## Incidental stale-gate repair

- The Args and DirWalk gates still looked for expression IDs 94/95 in the old
  monolithic kind owner, although current source owns them in
  `direct_mir_scalar_program_external_runtime_expression_kind_owner.pgy`.
  Their canaries now follow the actual owner.
- The String collection gate still pinned the MIR hash from 2026-08-23. Current
  source has emitted `binding_syntax_id` since the 2026-08-27 callable identity
  work, so a freshly rebuilt current-source DRV-2 produced a different stable
  hash. The ratchet now pins that current artifact; the gate's semantic and
  display-only mutation checks remain intact.

## Remaining BRIDGE inventory

This rung removes only the shared scalar collection preamble reconstruction.
LLVM StringJoin, process/directory adapters, parameter binding, value-result
transfer, owned return, local mutation, cleanup, and other expression
materializers still contain independent target-layout consumers. They require
separate executable prerequisites and a complete consumer migration before the
ABI row can move from `BRIDGE` to `CLOSED`.
