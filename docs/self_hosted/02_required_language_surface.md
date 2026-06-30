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
- Stable file preflight surface (`FileExists(String)`) so missing inputs do not
  collapse into empty-file payloads.
- Stable tool-exit surface (`Exit(Int)`) so JSON `ok:false` can become a
  process-level failure in CI.
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

- Stable arbitrary data tree representation for compiler AST-like shapes:
  user-defined records/classes, nested generic containers, and explicit tagged
  child families must be enough to express mixed node trees without falling back
  to untyped raw pointers.
- Stable graph-heavy compiler data structures: deterministic maps, ordered
  traversal, symbol tables, worklists, and graph diagnostics.
- Stable collection ergonomics for compiler-scale `List`, `Set`, and
  `HashMap` usage over strings, symbols, small records, and handles.
- Arena-backed scratch/result/persistent allocation lanes that are pleasant
  enough for compiler passes without Rust-style lifetime annotations.
- Debuggable generated C and LLVM output.
- Stable FFI boundary that is separate from normal domain code and does not
  reopen raw pointer access by default.
- Runtime-none/minimal-runtime policy for compiler tools.
- Stable scoped unsafe/raw escape policy for system-level interop.
- Deterministic codegen and stable IR dump schema.
- Adequate standard library for filesystem, paths, JSON, process execution, and tests.

## Current Substrate Contract

- Deterministic iteration is a READY substrate for compiler scalar keys:
  `MapKeys` and `SetValues` are gated for `String`, `Int`, `Long`, and `Bool`,
  and compiler-facing symbol, record, and handle identities are normalized to
  canonical strings or stable integer/long IDs before insertion.
- Raw/FFI is deliberately split. Scoped `unsafe` syntax exists, but raw pointer
  helpers remain runtime-internal and normal code still receives structured
  diagnostics instead of raw access. Self-hosted compiler passes must use file,
  process, or stable ABI boundaries until a real FFI contract lands.
- Arbitrary tree representation is partially proven by parser/backend fixtures
  over user classes and nested generics. The first self-hosted compiler AST
  model contract now lives in
  `src/self_hosted/codegen/typed_ast_node_skeleton.pgy`: it owns the flat typed
  arena vocabulary, explicit child lookup, atom lookup, and a small traversal
  payload contract. Hard self-host still cannot claim AST replacement until
  parser/codegen consume that typed arena with oracle parity; current
  parser/codegen rungs still consume text AST artifacts.

## Explicitly Not Required

- Quantum model.
- Rust-style lifetime annotations.
- HKT/Functor full FP model.
- Native LLVM WASM backend.
- WebGL/render APIs in core language.
