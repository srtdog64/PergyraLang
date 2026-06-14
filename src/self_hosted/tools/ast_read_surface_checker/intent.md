# AST Read Surface Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-06-15). This tool mirrors
`tests/ast_read_surface_smoke.sh` in Pergyra so the CFG/MIR source-of-truth
ratchet is checked by both the shell gate and a self-hosted audit tool.

## Intent

The source-of-truth migration should only move backend decisions from AST
readers to MIR-owned facts. The AST-read surface may shrink, but it must not
grow. The manifest owns the current ceilings and the C files that currently
carry each measured marker.

## Input Contract

- **manifest_owner:** `tests/ast_read_surface_manifest.txt`
- Format: one UTF-8 line per measured C file:
  `metric|literal_pattern|ceiling|scope|repo_relative_path`
- The Pergyra tool reads each listed file and counts literal, non-overlapping
  occurrences of `literal_pattern`.
- Directory coverage is checked by the shell parity harness because the current
  Pergyra subset has no directory traversal primitive.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.ast-read-surface.v1`:

```json
{
  "schema": "pgy.selfhost.ast-read-surface.v1",
  "ok": true,
  "source": {
    "manifest_owner": "tests/ast_read_surface_manifest.txt"
  },
  "counts": {
    "enum": 17,
    "source_ast_codegen": 0,
    "source_ast_compiler": 2,
    "source_decl_codegen": 2,
    "source_decl_compiler": 1,
    "routine_source_decl_codegen": 5,
    "violations": 0
  },
  "findings": []
}
```

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell oracle is `tests/ast_read_surface_smoke.sh`. The parity rung asserts:

- the shell smoke passes against the shared manifest;
- the Pergyra tool exits `0` on the clean repo;
- emitted JSON byte-matches `expected/clean.json`;
- `enum`, `source_ast_codegen`, `source_ast_compiler`, `source_decl_codegen`,
  `source_decl_compiler`, and `routine_source_decl_codegen` counts match shell
  literal grep over the same manifest entries;
- a synthetic source_ast growth fixture exits `1` with a
  `"kind":"surface_growth"` finding.

## Not In Scope

- Classifying provenance vs semantic readers.
- Directory walking in Pergyra; the shell parity harness keeps manifest
  coverage closed.
- Changing the ratchet values without retiring readers first.
