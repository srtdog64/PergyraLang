# Parser Substitution Track

Pergyra-written parser that emits the same compact text tree `pgy --ast`
produces, byte-for-byte. Mirrors C-side `src/parser/`. The goal is byte-equal
AST output for a growing source subset, so parity can be checked without
inventing a second AST serialization format.

- `main.pgy` - entry point. Reads `Args()[0]` when present, falls back to
  `fixture/source.txt` for older probes, then defaults to `examples/hello.pgy`.
- `fixture/` - committed `<base>.pgy` sources and `<base>_ast.txt` baselines
  used by the 188-source parity harness.
- `expected/clean.txt` - expected stdout when run on the default source.
- `intent.md` - contract, current grammar surface, and latest fixture/scale
  coverage result.

Run: `bash src/self_hosted/parity/parser_parity.sh`
