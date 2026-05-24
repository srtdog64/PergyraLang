# Required Language Surface

This list defines what Pergyra must support before self-hosting becomes
practical. It is not a request to add new syntax before beta.

## Already Directionally Aligned

- `Result<T, E>` and explicit failure handling.
- `slot` as runtime-validated handle rather than Rust-style lifetime annotation.
- `extern "C"` and C backend output.
- Stable generics subset with ability bounds.
- Module resolver direction.
- AIR JSON graph dump.
- MIR dump/validation direction.

## Required Before Soft Self-Host

- Stable file I/O surface for tool inputs.
- Stable JSON parse/emit support or a small self-host-friendly JSON module.
- Stable CLI argument handling.
- Deterministic output ordering.
- Stable diagnostic code vocabulary.
- Stable module import path rules.

## Required Before Partial Self-Host

- Intent compression for common compiler-tool workflows.
- Better collection ergonomics for maps/lists over strings and small records.
- Stable string/unicode comparison policy.
- Stable package/module manifest reader.
- Error reporting with source spans, reason, fix, and machine-readable code.

## Required Before Hard Self-Host

- Stable graph-heavy compiler data structures: deterministic maps, ordered
  traversal, symbol tables, worklists, and graph diagnostics.
- Stable collection ergonomics for compiler-scale `List`, `Set`, and
  `HashMap` usage over strings, symbols, small records, and handles.
- Arena-backed scratch/result/persistent allocation lanes that are pleasant
  enough for compiler passes without Rust-style lifetime annotations.
- Debuggable generated C and LLVM output.
- Stable FFI boundary.
- Runtime-none/minimal-runtime policy for compiler tools.
- Scoped unsafe/raw escape policy for system-level interop.
- Deterministic codegen and stable IR dump schema.
- Adequate standard library for filesystem, paths, JSON, process execution, and tests.

## Explicitly Not Required

- Quantum model.
- Rust-style lifetime annotations.
- HKT/Functor full FP model.
- Native LLVM WASM backend.
- WebGL/render APIs in core language.
