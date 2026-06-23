# Lexer Substitution Track

Pergyra-written lexer that emits the same text `pgy --tokens` produces,
byte-for-byte. Mirrors C-side `src/lexer/`. The goal is byte-equal output,
not a full compiler rewrite.

- `main.pgy` - entry point. Reads `Args()[0]` when present, then defaults to
  `examples/hello.pgy` for no-arg manual probes.
- `fixture/` - committed `<base>_tokens.txt` baselines for the 7-source
  parity harness (`hello`, `array_literal`, `break_continue`, `basic`,
  `heap`, `binary_search`).
- `expected/clean.txt` - expected stdout when run on the default source.
- `intent.md` - contract.

Run: `bash src/self_hosted/parity/lexer_parity.sh`
