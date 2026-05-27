# Runtime Panic Contract

This is the implementation-facing companion for the `Runtime Panic Parity`
proof obligation.

This document is the beta source of truth for unrecoverable runtime failures.
Recoverable language failures must use `Bool`, `Result<T>`, or queryable runtime
state. Runtime invariant breaks must hard-fail through the same panic contract in
the C and LLVM paths.

## Stable Panic Classes

- `oom`: allocation failure where no recovery value is part of the API contract.
- `divide-by-zero`: integer division or modulo with a zero divisor.
- `out-of-bounds`: array, slice, list, or map-index access outside the valid range.
- `released-slot`: read/write after slot, secure slot, or device slot release.
- `double-release`: releasing a slot, secure slot, or device slot more than once.
- `invalid-secure-token`: secure slot token mismatch or denied capability.
- `authority-mismatch`: authority boundary/token mismatch that violates the
  hard-fail authority contract.
- `internal-invariant`: compiler/runtime invariant break or impossible ABI state.

## Current Implementation Contract

- `src/runtime/pgy_runtime_panic_contract.h` owns the panic class vocabulary and
  the shared `PGY_RUNTIME_PANIC` emitter.
- Inline runtime `PGY_PANIC` delegates to the shared contract.
- LLVM exported typed slot read/write hard-fail on released-slot access instead
  of logging and returning a default value.
- LLVM exported secure slot read/write/release hard-fail on released secure slot,
  invalid token, and denied token capability.
- Inline and LLVM exported device slot read/write/release hard-fail on
  released device slot and double-release instead of silently ignoring the
  operation or returning a default value.
- `runtime-panic-contract-test-smoke` prevents the exported slot paths from
  reintroducing silent fallback for hard-fail classes.
- `runtime-panic-abi-test-smoke` compiles and executes inline-runtime and
  exported-runtime harnesses, then verifies the released-slot,
  invalid-secure-token, double-release, out-of-bounds, authority-mismatch, oom,
  and divide-by-zero classes actually abort with the shared panic prefix.
- The same ABI smoke verifies plain-slot, secure-slot, and device-slot
  `double-release`
  panics in both inline and exported runtime paths.
- The same ABI smoke verifies inline array out-of-bounds, exported array slice
  out-of-bounds, and inline/exported authority hard-fail checks use the shared
  `out-of-bounds` and `authority-mismatch` classes.
- Inline hard-fail allocator paths and exported array allocation overflow/failure
  paths use the shared `oom` class.
- Integer `/` and `%` lower through checked C/LLVM helper calls and use the
  shared `divide-by-zero` class when the divisor is zero.
- Generated C and LLVM array/slice indexing (`arr[index]`, `slice[index]`,
  and temporary `Words()[index]` / `Words().Slice(...)[index]`) and `ArraySet`
  lower through checked runtime helpers instead of direct `.data[index]`
  access for the stable `Array<T>` / `Slice<T>` surface.
- Generated C and LLVM `Slice<T>.Slice(start, len)` use the same subtract-form
  bounds check and the shared `out-of-bounds` panic class. Empty slice results
  are null-backed instead of deriving a pointer from the backing storage.
- Stable value-demanding collection APIs hard-fail instead of silently returning
  defaults: `ListGet` out-of-range, `QueuePop` on an empty queue, and `MapGet`
  on a missing key all use the shared `out-of-bounds` class. Query before use
  with `ListSize`, `QueueEmpty`, and `MapHas` when absence is expected.
- Stable mutation APIs also hard-fail instead of silently no-oping:
  `ListSet`, `ListRemove`, and `MapRemove` invalid access all use the shared
  `out-of-bounds` class.
- `Unwrap(result)` on `Err` and `UnwrapOption(option)` on `None` are sharp
  hard-fail boundaries. Generated C uses the inline runtime unwrap helpers, and
  generated LLVM now emits an explicit tag guard that calls the shared
  `internal-invariant` panic export instead of extracting the value field.
- `runtime-panic-codegen-test-smoke` compiles and runs generated Pergyra
  programs through both C and LLVM to verify integer divide/modulo by zero and
  stable collection out-of-bounds plus unwrap invariant misuse reach the shared
  panic classes.

## Deliberately Not Panic By Default

- Intent enter failure is recoverable and must remain queryable through runtime
  observability.
- Zone authority validation has both queryable validation and hard-fail check
  entry points. The check entry point aborts; the validation entry point records
  `last_*` reason state.
- Collection probe APIs (`MapHas`, `QueueEmpty`, size queries) remain
  recoverable/queryable. Value-demanding APIs (`ListGet`, `QueuePop`, `MapGet`)
  are hard-fail at the invalid access boundary.

## Remaining Beta Work

- Add C/LLVM executable parity regressions for any new panic class before it is
  exposed in beta-stable syntax.
- Released-slot, invalid-secure-token, double-release, device-slot release
  violations, inline array and exported array-slice out-of-bounds,
  inline/exported authority mismatch, inline allocator OOM, exported array OOM,
  checked integer divide/modulo helper paths, generated C/LLVM
  `Array<T>`/`Slice<T>` indexing plus `ArraySet` out-of-bounds, and stable
  `ListGet`/`QueuePop`/`MapGet`/`ListSet`/`ListRemove`/`MapRemove` invalid
  access plus `Unwrap(Err)` / `UnwrapOption(None)` internal-invariant misuse
  have executable smoke coverage.
- Keep generated-program negative smoke coverage in CI for every panic class
  whose failure is reachable through stable language syntax.
- Audit generated authority hard-fail call sites to ensure they all lower to
  `PGY_ZONE_AUTHORITY_CHECK` or `pgy_zone_authority_check_export`.
