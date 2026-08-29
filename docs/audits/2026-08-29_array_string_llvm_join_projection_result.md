# ArrayString LLVM Join projection result — 2026-08-29

This is an implementation result audit, not semantic authority or a whole-row
closure claim. Current source, the SoT registry, and executable gates override
it.

## Basis and bounded claim

- Exact base: `18a0b6666ef3b6119a1c0482693a12bffe1f3798` on
  `origin/main`.
- Objective: delete LLVM `StringJoin`'s local ArrayString data/length field-
  index literals and consume the target projection already derived by the
  production scalar LLVM emission root.
- Census before and after: `CLOSED=55 BRIDGE=32 ACTIVE=1`. This prerequisite
  does not close `abi.mir_array_string_layout_projection`.

## Replaced path

- `DirectMirScalarProgramLlvmStringCollectionMaterialization` already requires
  and cross-seals the program's target-qualified ArrayString projection. It now
  passes that same projection into the Join body.
- `DirectMirScalarProgramLlvmStringJoinBlock` emits the aggregate data and
  length reads from `abi.data_index` and `abi.length_index`. Literal
  `extractvalue` indices 0/1 are deleted and rejected by both focused and
  component structural ratchets.
- Runtime-call identity, Join argument order, allocation, copying, ownership,
  and the C path are unchanged. No Join-local projection or new authority was
  introduced.

## Falsifiers and observed evidence

- Baseline `one_mir_string_collection_builtin_projection.sh` passed before the
  edit in 24 seconds.
- A fresh Pergyra-built current-source DRV-2 was installed after the source
  edit. The focused gate then passed C/LLVM Split/Join and ArrayString access,
  semantic-change execution, and all existing no-artifact negatives in 25
  seconds.
- `tests/self_hosted_component_contract_smoke.sh` passed its full structural
  inventory. The Join owner is 18/20 and its collection orchestration caller is
  115/115; no cap was raised.
- Native-oracle driver C emission completed with zero errors. SoT live binding
  and negative mutations, edge census 88/182/`55/32/1`, single-owner, hard
  contract, likeness `4493/4493`, documentation quality, and diff checks are
  GREEN. Local Coq/Rocq is explicitly unavailable; no local formal execution is
  claimed.

## Publication result

- Implementation `fef8fb1fd10b1d32e3f49962c857089da662fd5e` is published
  on `origin/main`. Exact-head run `33238680231` completed GREEN 30/30.
- `build-linux` took 24m15s and full self-host took 34m05s. The run also passed
  Rocq, sanitizers, Windows/macOS, codegen bootstrap, and backend parity 20/20.
- The evidence publishes this one consumer migration only. Registry status
  remains `CLOSED=55 BRIDGE=32 ACTIVE=1`.

## Remaining BRIDGE inventory

Parameter binding, value-result transfer, owned return, mutation, cleanup,
process/directory adapters, and remaining expression materializers still own
independent ArrayString target-layout reads. They require executable consumer
migrations before the ABI row can move from `BRIDGE` to `CLOSED`.
