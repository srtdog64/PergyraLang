# ArrayString value-result target projection — 2026-08-30

Status: `ACTIVE IMPLEMENTATION`

Exact base: `660943081ce73c4e1ac8b645e38998ab34a1f348` on
`origin/main`.

This directive coordinates one executable ABI prerequisite. It does not close
or reclassify `abi.mir_array_string_layout_projection`, change a project
percentage, or make a second ArrayString layout owner.

## Shared objective card

- Objective: make the installed direct-MIR C/LLVM ArrayString value-result
  copy-in and copy-out boundary consume the target-qualified
  `DirectMirArrayStringAbiProjection` already derived at the emission root,
  instead of rebuilding `pgy_as`, `%pgy.array.string`, and alignment literals
  inside backend-local materializers.
- Priority: admitted layout identity; exact target binding; fail-closed absence;
  C/LLVM runtime parity; complete cross-family row rejection; old-read ratchet;
  then patch size.
- Production entrypoint: `pgy-self-driver --mir-json-backend=c|llvm` for the
  installed scalar-program path exercised by the mixed ArrayString/ArrayInt
  value-result program.
- Direct bypass to delete: backend-local ArrayString aggregate spelling and
  alignment inside the value-result copy boundary, plus the C signature's
  global type-spelling fallback for an admitted ArrayString formal.
- Fact owner: `DirectMirScalarProgramArrayStringAbiFact` carries the admitted
  program receipt. `DirectMirArrayStringAbiProjection` owns selected-target
  aggregate spelling, storage alignment, indices, and capability fingerprint.
  `DirectMirScalarProgramCArrayStringCarrierType` is the target-checked
  projection from that receipt to the private scalar C carrier; the public
  `PgyArray_String` spelling remains a separate FFI boundary.
- Last legitimate consumers for this rung:
  `DirectMirScalarProgramCArrayStringValueResultCopyIn`,
  `DirectMirScalarProgramCArrayStringValueResultCopyOut`,
  `DirectMirScalarProgramLlvmArrayStringValueResultCopyIn`,
  `DirectMirScalarProgramLlvmArrayStringValueResultCopyOut`, and the C callable
  signature type projection reached by those value-result formals. The payload
  enum preamble is a pass-through caller of that shared C type projection and
  must carry the same receipt rather than restoring the deleted fallback.
- Forbidden fallback: literal `pgy_as` or `%pgy.array.string` in the copy
  owners, literal copy alignment, `CompilerAbiLayoutArrayStringCValueType()` in
  the callable signature, type-name lookup beside the admitted projection,
  accepting `None` when a value-result row exists, or changing the public
  `PgyArray_String`/private runtime ABI.
- Verification gate:
  `direct_mir_scalar_bool_two_array_string_two_array_int_value_result_owner.sh`
  executes both backends, checks early and final copy-out, rejects complete
  cross-family row/ID mutation without an artifact, and structurally rejects
  the old reads. Component inventory must keep the same ratchet.

## Edit scope and overlap boundary

- Allowed implementation scope: the C/LLVM ArrayString value-result owners,
  their C/LLVM emission call sites, the C callable signature owner, its payload
  enum pass-through caller, the focused gate and component ratchet, this
  directive, the registry note, collaboration ledger, and final
  handoff/progress notes.
- ArrayString literals, foreach, indexed mutation, Args, DirWalk, StringJoin,
  readonly-ref, owned-return, conditional-move, and general cleanup consumers
  are outside this rung.
- The primary task is the sole integration owner. No parallel implementation
  track is open.

## Commands and budgets

- Static owner, shell syntax, component slice, and SoT edge: 60 seconds each.
- Focused mixed value-result C/LLVM parity: five minutes.
- Rebuild the installed DRV-2 once after implementation and reuse it. Full CI
  remains the publication boundary; no cache, shard, timeout, or test-count
  expansion is admitted.

## Output classification

The intended output is one executable prerequisite delta. Even when green, it
does not by itself decrement `BRIDGE=32`, increase hard `SUBSTITUTING`, or close
the C++-class binary reconstruction-resistance target.
