# Binary projection preflight probe

## Intent

Execute the first generation-bound immutable snapshot and binary projection
admission slice through Pergyra-built C and LLVM programs. Runtime Slot
generation identity has priority, followed by existing MIR ABI layout identity,
an explicit endian boundary, and only then projection ergonomics.

## Input Contract

- Runtime `SlotHandle` owns slot identity and generation advancement.
- `MirAbiLayoutRowCapture` and `MirAbiLayoutIdFromCapture` own size,
  alignment, field offsets, field sizes, field alignment, and layout identity.
- The projection boundary must supply endianness explicitly.
- A ticket is admitted only with the currently observed slot and generation.

## Output Contract

`BinaryProjectionPreflight` is the sole admission entrypoint and returns an
`Option<BinaryProjectionReceipt>`. Consumers never receive a raw unchecked
projection capability. Missing or mismatched facts return `None`.

The forbidden fallbacks are a host-endian default, a second offset calculation,
source/type-name-only compatibility, and a raw unchecked projection entrypoint.

## Oracle

The native runtime `SlotHandle` generation contract and the existing MIR ABI
layout identity owner are the semantic oracle. The probe requires byte-equal
success output from Pergyra-built C and LLVM executables. Its falsifiers are:

- a generation N ticket after the same slot reaches N+1;
- the same type and field facts with a changed field offset;
- an otherwise identical request with changed or missing endianness.

This probe does not expose DataView, SharedArrayBuffer, Atomics, pointer32, or a
mutable rebase view, and it does not add a new Slot allocator.

**Parity owner:** `tests/self_hosted/parity/binary_projection_preflight_probe_parity.sh`
