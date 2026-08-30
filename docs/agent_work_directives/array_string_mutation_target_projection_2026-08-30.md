# ArrayString Mutation Target Projection — 2026-08-30

Status: `PUBLISHED — EXACT-HEAD CI GREEN`

Exact base: `a8f255cb47e8ff565521c7f4c084e77667d257bb` on
`origin/main`.

This directive coordinates one executable ABI prerequisite. It does not close
or reclassify `abi.mir_array_string_layout_projection`, change a project
percentage, or create another ArrayString layout owner.

## Objective card

- Objective: make the installed direct-MIR C/LLVM ArrayString Set helper and
  LLVM ArrayString Pop consume the target-qualified
  `DirectMirArrayStringAbiProjection` already derived at the scalar-program
  emission root, instead of rebuilding the carrier, aggregate, field indices,
  and alignment in mutation materializers.
- Priority order: admitted layout identity; exact target binding; fail-closed
  absence; C/LLVM runtime parity; old-read ratchet; then patch size.
- Production entrypoint: installed
  `pgy-self-driver --mir-json-backend=c|llvm` over
  `direct_mir_array_mutation.pgy`, including local and value-result String
  Set/Pop execution.
- Direct bypass to delete: C Set reads
  `CompilerAbiLayoutArrayStringCValueType()` beside the admitted projection;
  LLVM Set and Pop spell `%pgy.array.string`, indices `0`/`1`, and storage
  `align 8` locally.
- Fact owner: `DirectMirScalarProgramArrayStringAbiFact` carries the admitted
  program receipt. `DirectMirArrayStringAbiProjection` owns the selected-target
  C carrier, LLVM aggregate, field indices, and storage alignment.
- Last legitimate consumers:
  `DirectMirScalarProgramCLocalArraySetMaterialization`,
  `DirectMirScalarProgramLlvmLocalArraySetMaterialization`, and the
  ArrayString Pop branch of `DirectMirScalarProgramLlvmArrayMutation`.
- Forbidden fallback: global ArrayString C-type lookup in the Set helper;
  literal `%pgy.array.string`, field indices, or `align 8` in LLVM String
  Set/Pop; deriving a second projection below the emission root; accepting a
  String mutation when the target projection is absent; or changing the public
  runtime ABI.
- Verification gates: `direct_mir_scalar_array_mutation_owner.sh` executes
  local/value-result Push/Set/Pop through both backends and preserves its MIR
  negative matrix; `self_hosted_component_contract_smoke.sh` structurally
  rejects the retired C/LLVM reads in the named consumers.

## Fresh falsifier

The exact-base installed driver passes the existing mutation gate, proving
runtime behavior is currently stable. Its generated LLVM artifact nevertheless
contains a locally spelled `%pgy.array.string` Set helper with hard-coded data
and length indices plus `align 8`; ArrayString Pop repeats the aggregate,
length index, and alignment. The C Set helper still calls the global
`CompilerAbiLayoutArrayStringCValueType()` rather than the carried target
projection. This is the reached production bypass for this rung.

## Scope and budget

- Allowed edits: the named C/LLVM mutation owners, their emission/operation
  callers, the existing focused gate and component ratchet, the registry
  residual note, and current coordination/handoff documents.
- Out of scope: ArrayString return ownership, conditional or multiple moves,
  literal/fresh-result ownership, general cleanup, other collection families,
  source semantics, a generic ABI query layer, and registry reclassification.
- Budget: static owner/gate checks under 60 seconds, focused installed-driver
  parity under five minutes, one current-source installed-driver rebuild, then
  bounded publication CI.

## Output classification

Success is one executable consumer-migration delta. It does not decrement
`BRIDGE=32`, increase hard `SUBSTITUTING`, close
`selfhost.semantic_artifact_admission`, or prove the wider C++-class release
target.

## Local result

- C ArrayString Set now receives the target-qualified projection from the
  scalar-program emission root and obtains its carrier only through the
  existing projection validator. The mutation owner no longer reads the
  global C carrier directly.
- LLVM ArrayString Set and Pop now consume the carried aggregate spelling,
  data/length indices, and storage alignment through
  `direct_mir_scalar_program_array_string_mutation_projection_owner.pgy`.
  The old literal aggregate and alignment are absent from the named mutation
  consumers; ArrayInt and ArrayBool remain outside this rung.
- Fresh current-source installed DRV-2 SHA-256 is
  `10EB93502E8A941E4F1D8BF9AED2C8D27B0A9435EEE348F3B0375FF42DDECB53`.
  The focused local/value-result Push/Set/Pop C/LLVM gate, the full component
  structural contract, hard substitution contract, SoT authority edge, Gate
  single-owner, and protocol registry are GREEN. SoT evidence is 88
  authorities, 183 derived carriers, and `CLOSED=55 BRIDGE=32 ACTIVE=1`.
- Implementation `d0b40821da73df7714de7e5bcbd7ccc572483dcc` is on
  `origin/main`. Exact-head run `33306318796` completed GREEN 30/30 in 40m22s;
  `build-linux` was GREEN in 28m09s, full self-host in 40m04s, and codegen
  fixed point in 8m50s. The Linux log observed the component contract, hard
  contract, and SoT edge at 88 authorities / 183 carriers / `55/32/1`.
  No row status or project percentage changed in this prerequisite migration.
