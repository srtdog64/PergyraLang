# ArrayString scalar preamble target projection — 2026-08-29

Status: `DONE — PUBLISHED GREEN`

Exact base: `9619f9078fe5795a4e97a7bace64a8f039b62fe7` on `origin/main`.

This directive coordinates one executable prerequisite. It does not close or
reclassify `abi.mir_array_string_layout_projection`, and it is not semantic
authority or progress evidence by itself.

## Shared objective card

- Objective: make the production scalar-program C and LLVM collection
  preambles derive one target-qualified `DirectMirArrayStringAbiProjection`
  from the carried `DirectMirScalarProgramArrayStringAbiFact`, then consume
  that projection instead of reconstructing the four-field storage layout in
  backend-local text.
- Priority: exact admitted layout identity, target capability binding,
  fail-closed projection readiness, C/LLVM execution parity, complete valid-row
  cross-family rejection, old literal-layout ratchet, then patch size.
- Fact owner: `DirectMirArrayStringCapturedAbiReady` owns admitted ArrayString
  layout identity. `DirectMirScalarProgramArrayStringAbiFact` carries the
  program receipt. `DirectMirArrayStringAbiProjection` owns selected-target
  storage spelling and indices.
- Last legitimate consumers: for this rung only,
  `DirectMirScalarProgramCStringCollectionMaterialization` and
  `DirectMirScalarProgramLlvmStringCollectionMaterialization`, reached by the
  installed scalar-program C/LLVM emission entrypoints.
- Forbidden fallback: a materializer-local four-field aggregate literal,
  hard-coded storage offsets or LLVM field indices, layout lookup by type
  string, a second backend-local projection, accepting an ArrayInt row and ID
  under an ArrayString type, or changing the private `pgy_as`/public
  `PgyArray_String` ownership boundary.
- Verification gate:
  `direct_mir_scalar_bool_two_array_string_two_array_int_value_result_owner.sh`
  must execute both backends and reject a complete ArrayInt row-plus-ID
  crosswire with no artifact. The component contract must reject reintroduced
  scalar-preamble aggregate/offset/index literals. The rebuilt installed DRV-2
  is reused for the focused gate.

## Edit scope and overlap boundary

- Allowed implementation scope: the ArrayString target projection owner, the
  C/LLVM scalar String collection materializers, their two emission call sites,
  the focused mutation/gate, the structural ratchet, and coordination/registry
  notes that describe this exact prerequisite.
- The primary task is the sole integration owner. There are no parallel
  implementation tracks on this rung.
- Other ArrayString foreach, indexed collection, parameter, mutation,
  process/directory, join, expression, and cleanup consumers remain outside
  this rung. Their existence keeps the top-level ABI registry row `BRIDGE`.

## Commands and budgets

- Static owner and structural checks target 60 seconds.
- The focused C/LLVM parity and negative gate targets five minutes.
- Rebuild the installed DRV-2 once after source edits, then reuse it. Full CI
  remains the exact-head publication boundary; no timeout, cache, or shard
  change is admitted.

## Local implementation evidence

- A current-source Pergyra-built DRV-2 is installed; after the final
  emission-root line-cap compaction, the build re-emitted the typed source and
  reused the identical fingerprinted binary.
- The mixed two-ArrayString/two-ArrayInt copyout gate is GREEN in C and LLVM
  and rejects the complete ArrayInt row-plus-ID crosswire with no artifact.
  The String collection builtin, Args, and DirWalk C/LLVM gates are also GREEN.
- The full component inventory is GREEN. C/LLVM orchestration owners are
  77/110 and 112/115; the target-projection/storage owners are 35/50, 76/90,
  and 139/160. No cap was raised.
- SoT edge is GREEN at 88 authorities, 182 carriers, and `55/32/1`. The
  likeness ratchet is GREEN at `result_use=4493/4493`. Local SoT adequacy
  declares the missing Coq/Rocq prover and still passes live binding and
  negative mutations; it does not claim the formal model executed.
- First publication `c554f007` reached the native-oracle compiler in run
  `33235581429`, which correctly rejected a local `storage` binding derived
  from the borrowed `projection` parameter. The C materializer now consumes
  `projection.storage` directly. Direct native-oracle emission then completed
  with zero errors and the focused plus structural gates remained GREEN.
- Repair `616f0ff892943f243914f32a1dd3940d10a3f6b2` is on `origin/main`.
  Exact-head run `33236522678` completed GREEN 30/30, including full
  self-host, Linux/Windows/macOS, sanitizers, Rocq, codegen bootstrap, and
  backend parity 20/20. The implementation lease is released.

## Output classification

This bounded prerequisite is published executable evidence. It does not own
or imply whole-row closure. The census remains `55/32/1`.
