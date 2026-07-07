# ABI Niche And Explicit Layout Policy

Status: beta ABI contract. Niche optimization and user-directed explicit layout
are intentionally not implemented as general source features.

Current implementation status: Pergyra does not currently expose source-level
bitpacking, packed fields, explicit field offsets, union overlap, or niche
optimized `Option<T>` layout. Runtime-internal ABI structs are frozen through
the ABI spec, but user-visible layout control is still a future raw/extern
capability surface.

Detailed layer and language-gap matrix: `docs/145_bit_layout_boundary_matrix.md`.

Bit reinterpretation policy: Pergyra must not hide a wire-order convention
behind a generic "logical bits" default. Bit order is a domain fact, not a
compiler preference. A future safe `bits(...)` value conversion must require an
explicit order parameter such as `LSB-first` or `MSB-first`. A future
`reinterpret(...)` operation is different: it crosses a world/ABI boundary and
must name the layout, endianness, and ABI evidence that make the physical
representation meaningful. In short:

- `bits(value, order = ...)` is a pure value conversion with explicit bit-order
  convention.
- `reinterpret(value, layout = ..., endian = ..., abi = ...)` is a boundary
  operation. The world/layout fact is visible at the source and carried into
  AIR/MIR evidence.
- No safe surface may default to hidden little-endian, hidden LSB-first, or
  backend-local memory retyping.
- Slot, SecureSlot, DeviceSlot, Pin, authority tokens, and capability handles
  are not bit-conversion subjects. Only an authorized payload value read from
  such a boundary may later participate in `bits(...)`, and only if the payload
  type itself has a bit-layout fact.

Short answer for `let mut` plus bitpacking: the current compiler can preserve
and check runtime ABI layout, but it cannot safely expose source-level
bitpacked fields yet. `let mut` only proves that a local binding may be
mutated. It does not prove that a partial-width field is addressable,
borrowable, atomic, or safe to update by read-modify-write. Source-level
bitpacking must wait for `LayoutFact` evidence and matching C/LLVM ABI golden
fixtures.

Pergyra currently uses an explicit tagged representation for `Option<T>`:
`{ tag: int32_t, value: T }` plus target padding. The source of truth is
`src/runtime/pgy_abi_spec.h`, and `src/runtime/pgy_abi_spec_asserts.h` freezes
the current sizes and offsets. `src/compiler/mir_abi_layout.c` mirrors that
runtime ABI into `MIRTypeLayout` facts. This means:

- `Option<Int>`, `Option<Float>`, and `Option<Bool>` are 8 bytes today.
- `Option<Long>`, `Option<Double>`, and `Option<String>` are 16 bytes today on
  the supported 64-bit targets.
- The MIR representation fact is `MIR_ABI_REPR_EXPLICIT_TAG` with
  `discriminant_field_name = "tag"`, `primary_tag_value = 0`, and
  `secondary_tag_value = 1`.
- `niche_none_pattern` is `NULL` for the current ABI.
- ABI lookup is exact-row only. Runtime function spelling is payload carried by
  a `MIRTypeLayout` row, not an alternate key that can reconstruct or select a
  layout.
- Slot-like MIR resource operations use `mir_abi_resource_runtime_fn(...)` over
  explicit ABI rows. The C MIR resource-op emitter and LLVM Slot/SecureSlot/
  DeviceSlot claim/read/write/release declaration registries and LLVM slot
  builtin calls, method calls, and identifier read emission must not synthesize
  `pgy_read_*`, `pgy_write_*`, or `pgy_release_*` names from a type suffix. Pin
  declarations, C direct source emitters, and async device submit remain
  separate ABI projections until their rows are cut over.

Rust-style niche encoding such as `Option<NonZeroU32>` fitting in 32 bits is
not implemented.

Beta-closure decision: do not add niche optimization before the proof surface
exists. The cheap implementation would be a backend-local layout shortcut; the
Pergyra implementation must instead make the invariant part of the language
and MIR ABI facts. `NonZero<T>`, `NonNull<T>`, and `NonEmpty<T>` are proof-type
candidates, not aliases for existing primitives.

## Frozen Beta Layout Facts

The current beta ABI intentionally spends the tag word. This table is the
source-level promise that the runtime ABI spec, MIR ABI facts, and both
backends must preserve:

| Source type | Representation | Size | Align | `Some` tag | `None` tag | Payload offset | Niche |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Option<Int>` | explicit tag | 8 | 4 | 0 | 1 | 4 | none |
| `Option<Long>` | explicit tag | 16 | 8 | 0 | 1 | 8 | none |
| `Option<Float>` | explicit tag | 8 | 4 | 0 | 1 | 4 | none |
| `Option<Double>` | explicit tag | 16 | 8 | 0 | 1 | 8 | none |
| `Option<Bool>` | explicit tag | 8 | 4 | 0 | 1 | 4 | none |
| `Option<String>` | explicit tag | 16 | 8 | 0 | 1 | pointer-size padding | none |

The table is not an optimization target. It is the frozen representation until
the language has proof-carrying value types and a MIR ABI fact that explicitly
names a reserved niche pattern.

## Current Golden Gates

The current contract is executable:

- `make test-abi` compiles `src/test_abi_spec.c` and checks the exact
  `Option<T>` tag/value offsets, tag values, and runtime shape for the active
  runtime Option specializations.
- `src/runtime/pgy_abi_spec_asserts.h` statically rejects an accidental
  shrink from the explicit tagged layout to a backend-local niche layout.
- `make test-mir` checks that `mir_abi_lookup("Option<Int>")`,
  `mir_abi_lookup("Option<Float>")`, `mir_abi_lookup("Option<Double>")`, and
  `mir_abi_lookup("Option<String>")` expose explicit-tag MIR facts, not niche
  facts.
- `make abi-ownership-shape-test-smoke` keeps this document, the runtime ABI
  header, and the MIR ABI fact wording tied together, and rejects runtime-symbol
  fallback lookup inside the MIR ABI owner and C MIR resource-op emission.

## Niche Optimization Gate

Niche optimization may be added only after the language has proof-carrying value
types such as `NonZero<T>`, `NonNull<T>`, or `NonEmpty<T>`. The invariant must
not be a backend guess. The semantic/DAG layer must prove the invariant, MIR
must carry an ABI fact that names the niche bit pattern, and C/LLVM backends
must consume that same MIR fact.

Required source-of-truth chain:

1. Semantic/DAG proves the value invariant, for example "this `NonZero<Int>` can
   never be zero".
2. MIR ABI facts record the representation policy, for example
   `MIR_ABI_REPR_NICHE_RESERVED`, `niche_none_pattern = "0"`, and the payload
   ABI type.
3. C and LLVM lower from the MIR ABI fact. They must not locally infer that
   `0`, `NULL`, or an empty length is available.
4. Static ABI tests and backend-compare fixtures prove the optimized and
   non-optimized representations behave the same at the source level.

Until that chain exists, `Option<T>` stays explicitly tagged.

Value-invariant proof types are prerequisite. A backend may not infer a niche
from a spelling convention, a pointer-looking payload, or a local C/LLVM layout
choice. The MIR ABI fact must be the only backend input that authorizes
`None`-as-zero, `None`-as-null, or any other reused bit pattern.

The promotion ladder is deliberately ordered:

1. introduce the proof type and semantic/DAG invariant;
2. prove construction and flow preservation for the invariant;
3. add the MIR ABI niche fact;
4. lower C and LLVM from that fact;
5. only then change the ABI table and golden tests.

## Packed Field Gate

Packed fields and bit-level layout are a LayoutFact problem, not a C backend
syntax shortcut. Pergyra must not delegate user-visible bit packing to C
bitfields because bitfield order, padding, signedness, and addressability are
target/compiler policy. The C backend and LLVM backend must instead consume the
same ABI layout fact rows and emit equivalent mask/shift operations.

The minimum fact shape for a future packed field is:

- storage unit type and width;
- byte offset of the storage unit;
- bit offset and bit width inside that unit;
- signedness and extension policy;
- read/write mask policy;
- whether the field may be addressed, borrowed, atomically updated, or exposed
  through an extern/raw ABI boundary.

Mutable packed fields are not ordinary field stores. `x.f = value` becomes a
read-modify-write operation over the containing storage unit. That means the
layout verifier must reject or explicitly own aliasing and concurrency cases
before the feature is source-visible. In particular, a packed mutable field may
not be lowered while another live reference can observe or mutate the same
storage unit unless a later atomic/borrow rule proves that access pattern.

`let mut` and `inout` do not weaken this rule. A partial-width packed field is
not an addressable lvalue, so it cannot be passed as `inout`, borrowed by
reference, or treated as a pointer-like bit slice until a dedicated layout
effect owner proves the read-modify-write sequence.

Rule: `let mut` is local-storage mutability, not permission to take an address
of a bit slice or to bypass the layout/effect owner.

Implementation note: `let mut` is already valid Pergyra syntax for mutable
locals and fields. That existing syntax is not a layout capability. If a future
packed-field or bit-slice spelling reaches the parser, the first compiler
behavior must still be a structured semantic reject until a `LayoutFact` owner
proves storage unit, offsets, masks, shift, aliasing, and read-modify-write
policy.

Future negative fixtures must reject `let mut` / `inout` access to partial-width packed
fields, reject address-like treatment of bit slices, and require the
diagnostic to name the missing `LayoutFact` owner rather than silently lowering
through C bitfields.

The future ABI golden class must compare C and LLVM against the same
`LayoutFact` rows: storage unit type, byte offset, bit offset, bit width, read
mask, write mask, and shift. Backend output equality alone is not enough; both
backends must consume the same fact row before source-level bitpacking can be
enabled.

Slot, SecureSlot, DeviceSlot, Pin, and capability/evidence handles are excluded
from packing by default. They may become packable only through a dedicated
layout owner that proves their ABI shape, lifetime behavior, and security
invariants. General-purpose packing must not silently overlap or truncate
capability-bearing state.

## Explicit Layout Gate

User-directed explicit layout, field offsets, packed structs, and union-style
field overlap are not part of the general struct/class surface. They are valid
future tools only at an interop or system boundary, such as an extern "C" ABI
declaration, a scoped `unsafe(ffi, layout)` / `unsafe(raw, layout)` block, or a
future ABI descriptor.

General Pergyra aggregates remain ownership-aware values. Allowing arbitrary
field overlap inside ordinary structs would let users create aliasing that the
slot/capability model cannot prove. Therefore explicit layout must be:

- boundary-scoped, never the default aggregate model;
- source-gated through syntax that says it is ABI/raw interop;
- rejected outside that scope with a structured diagnostic;
- visible as AIR evidence for the capability that accepted it;
- backed by static `sizeof`/`offsetof` assertions and C/LLVM parity fixtures.

This keeps low-level layout power available without turning ordinary Pergyra
data modeling into unchecked aliasing.

Until scoped layout capability evidence exists, explicit layout is not a
language feature. Runtime-internal C structs may use normal C layout techniques
behind the ABI spec, but source-level Pergyra structs/classes may not request
`packed`, field offsets, or union overlap.
