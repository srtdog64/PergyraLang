# Parser Substitution Track

Pergyra-written parser that emits the same compact text tree `pgy --ast`
produces, byte-for-byte. Mirrors C-side `src/parser/`. The goal is byte-equal
AST output for a growing source subset, so parity can be checked without
inventing a second AST serialization format.

- `main.pgy` - entrypoint only. It delegates all CLI mode selection to
  `run_owner.pgy`.
- `run_owner.pgy` - parser run-boundary owner. It selects normal parse mode
  vs `--fixture-manifest`, then delegates source selection and parsing.
- `program_parse_owner.pgy` - root Program SoT. Owns root source reads, root
  cursor initialization, top-level declaration parse invocation, and final
  compact AST `Program:` assembly.
- `source_path_owner.pgy` - parser input/import-path SoT. Owns the argv/default
  source path, imported-source read marker, and import graph membership fact.
  Source dirname and import joins consume `SelfHostPath` instead of
  reimplementing path facts locally.
- `fixture_manifest_owner.pgy` - parser parity source/fixture inventory owner.
  The shell runner consumes its `--fixture-manifest` output instead of owning
  source/fixture rows.
- `expr_primary_owner.pgy` - primary expression root owner. Owns literals,
  identifiers, lambdas, grouped expressions, array literals, and expression
  block shims before postfix consumption.
- `expr_postfix_owner.pgy` - postfix expression chain owner. Owns calls,
  indexes, member access, postfix try, object-init syntax, and call-only
  turbofish consumption.
- `stmt_owner.pgy` - statement dispatch and block parsing owner. It delegates
  branch-specific statement syntax to statement owners instead of carrying every
  statement shape itself.
- `stmt_loop_owner.pgy` - loop statement syntax owner. Owns `while`, `loop`,
  and `for` compact AST header/block emission.
- `fixture/` - committed `<base>.pgy` sources and `<base>_ast.txt` baselines
  used by the 187-fixture parity harness.
- `expected/clean.txt` - expected stdout when run on the default source.
- `intent.md` - contract, current grammar surface, and latest fixture/scale
  coverage result.

Run: `bash tests/self_hosted/parity/parser_parity.sh`
