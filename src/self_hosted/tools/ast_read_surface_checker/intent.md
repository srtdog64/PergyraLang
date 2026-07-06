# AST Read Surface Checker -- Intent / Contract

**Status:** *rung-2 DirWalk-owned* (2026-07-06). This tool owns the
CFG/MIR source-of-truth ratchet report inside Pergyra. The shell
`tests/ast_read_surface_smoke.sh` remains a separate production ratchet gate,
not the clean oracle for this self-hosted parity rung.

## Intent

The source-of-truth migration should only move backend decisions from AST
readers to MIR-owned facts. The AST-read surface may shrink, but it must not
grow. The ratchet spec owns the current ceilings and metric scopes; live file
enumeration is owned by `DirWalk(scope)`.

## Input Contract

- **ratchet_owner:** `tests/ast_read_surface_ratchet.txt`
- Format: one UTF-8 line per measured metric:
  `metric|literal_pattern|ceiling|scope`
- The Pergyra tool calls `DirWalk(scope)`, filters `.c` files, and counts
  literal, non-overlapping occurrences of `literal_pattern`.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.ast-read-surface.v1`:

```json
{
  "schema": "pgy.selfhost.ast-read-surface.v1",
  "ok": true,
  "source": {
    "ratchet_owner": "tests/ast_read_surface_ratchet.txt",
    "inventory_owner": "DirWalk(scope)"
  },
  "counts": {
    "enum": 0,
    "source_ast_codegen": 0,
    "source_ast_compiler": 0,
    "source_decl_codegen": 0,
    "source_decl_compiler": 0,
    "routine_source_decl_codegen": 0,
    "violations": 0
  },
  "findings": []
}
```

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The clean oracle is the committed expected JSON artifact
`expected/clean.json`; the synthetic growth oracle is the committed expected
JSON artifact `expected/growth_source_ast_codegen.json`. The parity rung
asserts:

- the Pergyra tool exits `0` on the clean repo;
- emitted JSON byte-matches `expected/clean.json` through the shared
  ArtifactZone/TestHarness comparator path;
- a synthetic source_ast growth fixture exits `1` and byte-matches
  `expected/growth_source_ast_codegen.json`;
- both C and LLVM legs compile the same self-hosted checker.

The parity rung must not recompute the clean counts in shell. If the count
algorithm is wrong, fix the self-hosted owner or add a negative fixture that
forces the owner to fail closed.

## Not In Scope

- Classifying provenance vs semantic readers.
- Changing the ratchet values without retiring readers first.
