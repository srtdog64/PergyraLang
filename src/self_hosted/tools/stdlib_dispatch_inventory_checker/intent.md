# Stdlib Dispatch Inventory Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Reads two C-side dispatch tables
that must stay inside the documented stdlib scalar-IO inventory drift
tolerance, emits the validator schema.

## Intent

Adding a new stdlib scalar/IO builtin should update *both* the C-backend
transpiler dispatch (`transpiler_expr_stdlib_scalar_builtin.c`) and the
LLVM-backend stdlib registry (`llvm_expr_stdlib_scalar_io_calls.c`).
When the two drift (e.g. `StringJoin` was once C-only and crashed the LLVM
backend until the LLVM entry was added during the 2026-05-26 self-host
dogfood pass), backend-compare fixtures silently catch the regression but
without telling the developer *which* surface drifted. This tool surfaces
the drift directly at inventory time. The current contract allows a small
known offset between the C split-table anchors and the single LLVM table;
the long-term ratchet is `drift_tolerance = 0`.

## Input Contract

- **c_dispatch_owner**:
  `src/codegen/transpiler_expr_stdlib_scalar_builtin.c` (entries of shape
  `{ "Name", N, TRANSPILER_SCALAR_OP_... }`).
- **llvm_dispatch_owner**:
  `src/codegen/llvm_expr_stdlib_scalar_io_calls.c` (entries of shape
  `{ "Name", "stdlib family", "RuntimeSymbol", N }`).

Both paths are fixed relative to repository root. No CLI args yet.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.stdlib-dispatch-inventory.v1`:

```json
{
  "schema": "pgy.selfhost.stdlib-dispatch-inventory.v1",
  "ok": true,
  "source": {
    "c_dispatch_owner": "src/codegen/transpiler_expr_stdlib_scalar_builtin.c",
    "llvm_dispatch_owner": "src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
  },
  "counts": {
    "c_entries": 52,
    "llvm_entries": 57,
    "drift": 0
  },
  "findings": []
}
```

- `ok = (counts.drift == 0)`, where `drift` is set only when the raw
  C/LLVM count difference exceeds the checker tolerance.
- `findings[]` carries:
  - `{ "kind": "count_drift", "c_entries": <int>, "llvm_entries": <int>,
       "location": "..." }` when counts differ.
  - `{ "kind": "input_error", "key": "...", "location": "..." }` when
    either input file is missing.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell drift detector is `grep -cE` against each file with the same
patterns the Pergyra tool uses. There is no existing C-side smoke that
gates this two-table parity today; the Pergyra origin is the primary
implementation and the shell grep is the auxiliary parity backend.

The parity rung (`tests/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the live repo with matching counts.
- Emitted JSON byte-matches `expected/clean.json`.
- Counts match shell `grep -cE` ground truth.
- A synthetic drift fixture (delete one LLVM entry) yields `rc=1` with a
  `"kind":"count_drift"` finding.

## Why Now

This is the *sixth* soft self-host tool. It validates a structural
invariant (two C-side dispatch tables must agree in count) that the
StringJoin LLVM lift work directly motivated. Tools 1-5 validated
manifest- or output-shaped invariants; this one validates a *compiler
source-of-truth* invariant, broadening the dogfood surface.

## Not In Scope

- Comparing entry *names* (not just counts) between the two tables. A
  count-only check catches a missing entry but not a *renamed* entry.
  Name-set diff is the next-rung scope.
- Validating entries against runtime ABI declarations
  (`llvm_runtime_core_builtin_decl.c`) -- separate tool.
- Catching entries that drift in *argument count* without drifting in
  string-name -- separate tool.
