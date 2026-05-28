# Parser Substitution Track

Pergyra-written parser that emits the same compact text-tree `pgy --ast`
produces, byte-for-byte. Mirrors C-side `src/parser/`. The goal is
byte-equal AST output for a growing source subset, so parity can be
checked without inventing a second AST serialization format.

- `main.pgy` — entry point. Reads `fixture/source.txt` when present
  (parity-script-managed override) or defaults to `examples/hello.pgy`.
- `fixture/` — committed `<base>.pgy` sources + `<base>_ast.txt`
  baselines used by the parity harness.
- `expected/clean.txt` — expected stdout when run on the default source.
- `intent.md` — contract + current grammar surface.

Run: `bash src/self_hosted/parity/parser_parity.sh`
