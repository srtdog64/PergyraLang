# Public array storage release

Status: bounded public release implemented; private and installed pair verified.
Base: `aed8a904992465b14adf5a1b29d7fcd9789f0a7d`.
Pre-existing unrelated dirty changes belong to their current owners and are
preserved. The shared ArrayDrop diagnostic mapping is part of this release.

## Shared objective card

- Objective: make `ArrayDrop` a public, consuming storage-release operation
  for plain value arrays, without framework-specific routes or dependencies.
- Priority: ownership identity, element-lifetime proof, explicit refusal,
  executable native/self-host C and LLVM evidence, then patch size.
- Fact owner: Pergyra resolved semantic binding/type facts; the native semantic
  release owner is the bootstrap counterpart. Runtime only releases admitted
  storage and does not decide element ownership.
- Last legitimate consumer: C/LLVM storage-drop materialization.
- Forbidden fallback: remove the internal caller restriction, infer element
  ownership from `Array<T>`, accept borrowed parameters, or deep-free unknown
  elements. No change to the 24 BRIDGE rows is implied.
- Integration gate: `tests/self_hosted/parity/public_array_drop.sh`; positive
  execution and negative source/MIR admission are separate evidence.
- Falsifiers: use after drop, double drop, borrowed/inout receiver, live Slice,
  alias/escaped descriptor, nested owned resource, and private-builtin spoofing.

## Scope and execution

Root is the single implementation editor for this release boundary. No parallel
implementation track is opened. The user's explicit final reply selected root
as the sole validation/installation/commit/push owner. Main handed off the
already-running final build and stopped; root independently validated its
source graph and both executable pairs. Main's unrelated CFG/canonical/driver
comments and existing handoff content remain preserved. Static checks have a
60-second budget, focused parity a 5-minute budget, integration a 30-minute
budget. Private compiler pairs precede any installed-driver change.

This explicit user request reopens public release as the current executable
boundary; aggregate-formal deep-string proof is not silently declared complete.
Evidence and remaining broader-gate limits are recorded in
`docs/audits/public_array_storage_release_2026-10-01.md`. The fact-family registry
still reports `CLOSED=69 BRIDGE=24 ACTIVE=2`; no row is promoted by this directive.
