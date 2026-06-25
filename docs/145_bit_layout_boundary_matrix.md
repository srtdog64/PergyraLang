# Bit Layout Boundary Matrix

Status: design contract for future bit conversion, packed layout, and
reinterpretation work. This document does not add a beta source feature.

Related owners:

- `docs/136_abi_niche_and_explicit_layout.md`
- `docs/semantics/04_ownership_abi.md`
- `docs/125_source_of_truth_spine.md`
- `tests/abi_ownership_shape_smoke.sh`

## Thesis

Pergyra must not copy a hidden "logical bits" default. Bit order is a domain and
world fact: sometimes it is a wire position, sometimes a file-format bit order,
sometimes a CPU ABI storage convention, and sometimes an MMIO register field.
Treating that as a compiler-private default erases the thing Pergyra is trying
to model.

The split is:

```text
bits(value, order = LSB-first | MSB-first | named-order)
  pure value conversion
  no world boundary crossing
  bit order is an explicit parameter

reinterpret(value, layout = ..., endian = ..., abi = ..., world = ...)
  physical representation operation
  crosses a world/ABI/raw boundary
  layout, endianness, ABI, and world evidence are visible facts
```

There is no default bit order. There is no backend-local memory retyping in the
safe surface.

## Layer-Width Contract

The full width of the feature spans these layers. A future implementation must
name the owner at each layer before parser acceptance.

| Layer | Owner | Required fact | Forbidden shortcut |
| --- | --- | --- | --- |
| Source surface | Parser + grammar docs | `bits(..., order = ...)` and `reinterpret(..., layout/endian/abi/world = ...)` are different spellings | one `bitCast` spelling that hides order or boundary |
| Type/Semantic | Type checker / DAG | payload is a value with a bit-layout fact; handles/resources/tokens are rejected | treating Slot/SecureSlot/DeviceSlot/Pin/token as bit-conversion subjects |
| World boundary | Intent/zone/world evidence | `reinterpret` names the boundary being crossed | pretending physical bytes are a pure value transform |
| AIR | Evidence append owner | layout/endian/world evidence is recorded with provider and subject provenance | reconstructing boundary facts from backend strings |
| MIR ABI | `MIRTypeLayout` / `LayoutFact` | storage width, logical bit width, order, byte offset, bit offset, masks, signedness, extension policy | C/LLVM inventing mask/shift or field order locally |
| Runtime ABI | `pgy_abi_spec.h` and asserts | stable external shape for runtime-owned values | build-mode or macro-selected ABI aliases |
| C backend | MIR ABI consumer | emits explicit shifts/masks/loads from `LayoutFact` | C bitfields, union punning, compiler-specific packing |
| LLVM backend | MIR ABI consumer | lowers through explicit integer ops and ABI-sized storage as facts require | arbitrary-width memory lowering because LLVM happens to accept it |
| Diagnostics | structured diagnostic owner | missing order/boundary/layout fact produces a named error and Fix | silent fallback to target endian or host layout |
| Self-hosted tools | oracle/parity owner | self-hosted analyzers read the same facts and compare C/LLVM output | parsing emitted C or LLVM text to recover semantic layout |

## Slot Boundary Rule

Slot is a resource boundary, not a bitstring.

Forbidden subjects:

- `Slot<T>`
- `SecureSlot<T>`
- `DeviceSlot<T>`
- `Pin`
- `ReadView<T>` / `WriteView<T>`
- authority tokens and capability/evidence handles
- `own` / `ref` resource carriers

Allowed shape:

```text
payload = SlotRead(slot)          // authority/generation/pin checks happen here
bits(payload, order = MSB-first)  // only if payload type has a bit-layout fact
```

For `DeviceSlot`, MMIO, packet, protocol, and file-format work, the right model
is not `Slot<T> bitCast`. The right model is a world-boundary value with explicit
layout facts:

```text
reinterpret(device_value,
  world = DeviceWorld,
  layout = Register24,
  endian = little,
  abi = mmio32)
```

## Language Comparison And Pergyra Gaps

This table records what Pergyra should learn from each language and what is
still missing before Pergyra can claim the comparable capability.

| Language | Useful lesson | Do not copy | Pergyra missing piece |
| --- | --- | --- | --- |
| Zig | Legalization passes can reduce hard-to-lower bit operations into backend-simple forms; arbitrary-width integer work should use ABI-sized memory storage when needed | hidden logical-bit order as a safe default; general `@bitCast` feel for resource boundaries | `bits(order=...)`, `reinterpret(layout/endian/abi/world=...)`, and a MIR legalization pass that lowers from explicit facts |
| Rust | `NonZero`/`NonNull` proof-like value types can authorize niche layout; `repr(C)`/`repr(transparent)` are explicit representation promises | safe transmute-like surface; claiming Rust-level lifetime safety from Slot | proof-carrying value invariants, niche MIR facts, and negative diagnostics for handle/resource bit conversion |
| C | ABI interop is unavoidable and predictable only when the ABI is named | C bitfields, unions, implementation-defined packing, and host-endian assumptions as source semantics | extern/raw layout descriptors plus C/LLVM parity fixtures for every accepted layout row |
| C++ | `std::bit_cast` demonstrates value-copy representation conversion with type restrictions | importing C++ object-representation rules into ordinary Pergyra aggregates | a copy-only representation conversion rule that still requires explicit order/boundary facts |
| C# | `StructLayout`, `FieldOffset`, `fixed`, and marshaling show how explicit layout is useful at interop boundaries | making explicit layout an ordinary class/struct feature | scoped `unsafe(ffi, layout)` capability, field-offset diagnostics, and pinned interop views that do not bypass Slot rules |
| Swift | value semantics plus explicit unsafe-byte APIs keep normal values and raw bytes distinct | ARC/reference semantics as implicit resource proof | a value/bytes boundary API that is explicit about order, layout, and ownership evidence |
| Go | `encoding/binary` style APIs make byte order explicit at the library boundary | `unsafe.Pointer` as the normal way to model protocol or device data | stdlib helpers for endian-explicit binary/protocol parsing backed by `LayoutFact`, not ad hoc casts |
| WebAssembly | portable memory and sandboxing force explicit boundary thinking | assuming Wasm memory makes unsafe layout sound automatically | Wasm/backend layout gates that preserve the same MIR facts rather than inventing target-local byte order |

## Feature Admission Checklist

A bit/layout feature is not ready when it parses. It is ready only when all rows
below have an owner:

1. Grammar differentiates `bits` from `reinterpret`.
2. Safe `bits` requires an explicit order parameter.
3. `reinterpret` requires world, layout, endian, and ABI evidence.
4. Slot/capability/resource handles are rejected as conversion subjects.
5. Payload conversion after an authorized read is allowed only for copyable
   values with a bit-layout fact.
6. MIR carries one `LayoutFact` row consumed by both C and LLVM.
7. C backend avoids C bitfields and union punning for user-visible semantics.
8. LLVM backend avoids backend-local arbitrary-width memory semantics.
9. Diagnostics name the missing order, layout, endian, world, or ABI fact.
10. Self-hosted tools consume the same facts and pass oracle parity.

## Current Status

Current beta state:

- No user-visible `bits(...)` source surface.
- No user-visible `reinterpret(...)` source surface.
- No source-level packed fields, explicit offsets, union overlap, or niche
  optimized `Option<T>`.
- Current `Option<T>` ABI remains explicit-tagged.
- Explicit layout remains future `unsafe(ffi, layout)` / `unsafe(raw, layout)`
  boundary work.

Therefore the correct current behavior is fail-closed: do not accept source
spelling for safe bit conversion or physical reinterpretation until the owners
above exist.
