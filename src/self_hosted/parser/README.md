# Parser Substitution Track

Pergyra-written parser that emits the same compact text tree `pgy --ast`
produces, byte-for-byte. Mirrors C-side `src/parser/`. The goal is byte-equal
AST output for a growing source subset, so parity can be checked without
inventing a second AST serialization format.

- `main.pgy` - entry point orchestration only. It delegates source path/default
  selection and source-relative import path resolution to
  `source_path_owner.pgy`.
- `program_parse_owner.pgy` - root Program SoT. Owns root source reads, root
  cursor initialization, top-level declaration parse invocation, and final
  compact AST `Program:` assembly.
- `source_path_owner.pgy` - parser input-path SoT. Owns the argv/default source
  path, source-dir extraction, import path resolution, and imported-source
  marker.
- `fixture/` - committed `<base>.pgy` sources and `<base>_ast.txt` baselines
  used by the 188-source parity harness.
- `expected/clean.txt` - expected stdout when run on the default source.
- `intent.md` - contract, current grammar surface, and latest fixture/scale
  coverage result.

Run: `bash src/self_hosted/parity/parser_parity.sh`
