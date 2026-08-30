# ArrayString value-result target projection — 2026-08-30

Status: `PUBLISHED — CI GREEN`

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

## Local result

- Implementation checkpoint:
  `4a66f127f906a8514ac780b0afac5539169d7a37`.
- A fresh Pergyra-built DRV-2 was installed from the typed source graph.
- The focused mixed ArrayString/ArrayInt value-result gate passed C/LLVM
  runtime parity and all artifact-negative mutations.
- The four-routine scalar control passed with no ArrayString projection,
  proving canonical `None` remains accepted when no ArrayString row exists.
- Hard contract, SoT authority edge, and single Gate-SoT checks passed. The
  full component contract was interrupted after exceeding the local static
  budget and is not reported green; publication CI owns its full execution.

## Publication result

- Implementation checkpoint:
  `4a66f127f906a8514ac780b0afac5539169d7a37`; final structural-cap repair:
  `7a61294ac752aad6ef0fdb4d44f5f2e7b03207a7`.
- The first publication attempts exposed shrink-only owner caps rather than a
  semantic failure: the projection owner was `53/50`, followed by the C
  value-result owner at `74/70`. The final repair kept every cap unchanged and
  compressed only blank lines, wrapping, and equivalent string assembly.
- The final affected owner counts are C emission `309/310`, C signature
  `155/155`, LLVM emission `360/360`, projection `50/50`, C value-result
  `70/70`, and LLVM value-result `88/90`. A fresh typed-source DRV-2 rebuilt
  and installed after that repair; focused mixed C/LLVM parity plus negatives
  and the no-ArrayString control both passed again.
- Exact-head CI run `33286454027` completed GREEN 30/30. `build-linux` passed
  in 27m26s, including the full component/Markdown contracts that exceeded
  the local budget; full self-host passed in 41m06s, sanitizers in 12m46s,
  codegen bootstrap in 8m42s, Windows in 9m06s, backend toolchain in 11m13s,
  and backend comparison 20/20.
- Publication changes neither the `CLOSED=55 / BRIDGE=32 / ACTIVE=1` census,
  hard `SUBSTITUTING` progress, nor the 83% project forecast. The separately
  approved C++-class binary reconstruction-resistance target remains an open
  acceptance gate and is not claimed as current binary behavior.
