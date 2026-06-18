# ABI Niche And Explicit Layout Policy

Status: design contract, not an implemented optimization.

Pergyra currently uses an explicit tagged representation for `Option<T>`:
`{ tag: int32_t, value: T }` plus target padding. The source of truth is
`src/runtime/pgy_abi_spec.h`, and `src/runtime/pgy_abi_spec_asserts.h` freezes
the current sizes and offsets. This means `Option<Int>` is 8 bytes today and
`Option<Long>` / `Option<String>` are at least 16 bytes. Rust-style niche
encoding such as `Option<NonZeroU32>` fitting in 32 bits is not implemented.

Beta-closure decision: do not add niche optimization before the proof surface
exists. The cheap implementation would be a backend-local layout shortcut; the
Pergyra implementation must instead make the invariant part of the language
and MIR ABI facts. `NonZero<T>`, `NonNull<T>`, and `NonEmpty<T>` are proof-type
candidates, not aliases for existing primitives.

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
   `tag_strategy = niche`, `none_pattern = 0`, and the payload ABI type.
3. C and LLVM lower from the MIR ABI fact. They must not locally infer that
   `0`, `NULL`, or an empty length is available.
4. Static ABI tests and backend-compare fixtures prove the optimized and
   non-optimized representations behave the same at the source level.

Until that chain exists, `Option<T>` stays explicitly tagged.

Value-invariant proof types are prerequisite. A backend may not infer a niche
from a spelling convention, a pointer-looking payload, or a local C/LLVM layout
choice. The MIR ABI fact must be the only backend input that authorizes
`None`-as-zero, `None`-as-null, or any other reused bit pattern.

## Explicit Layout Gate

User-directed explicit layout, field offsets, packed structs, and union-style
field overlap are not part of the general struct/class surface. They are valid
future tools only at an interop or system boundary, such as an extern "C" ABI
declaration, a scoped `unsafe(raw)` block, or a future ABI descriptor.

General Pergyra aggregates remain ownership-aware values. Allowing arbitrary
field overlap inside ordinary structs would let users create aliasing that the
slot/capability model cannot prove. Therefore explicit layout must be:

- boundary-scoped, never the default aggregate model;
- source-gated through syntax that says it is ABI/raw interop;
- rejected outside that scope with a structured diagnostic;
- backed by static `sizeof`/`offsetof` assertions and C/LLVM parity fixtures.

This keeps low-level layout power available without turning ordinary Pergyra
data modeling into unchecked aliasing.
