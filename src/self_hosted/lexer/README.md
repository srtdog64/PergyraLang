# Lexer Substitution Track

Pergyra-written lexer that emits the same text `pgy --tokens` produces,
byte-for-byte. Mirrors C-side `src/lexer/`. The goal is byte-equal output,
not a full compiler rewrite.

- `main.pgy` - entry point. Reads `Args()[0]` when present, falls back to
  `fixture/source.txt` for older probes, then defaults to `examples/hello.pgy`.
- `fixture/` - committed `<base>_tokens.txt` baselines for the 6-source
  parity harness (`hello`, `array_literal`, `break_continue`, `basic`,
  `heap`, `binary_search`).
- `expected/clean.txt` - expected stdout when run on the default source.
- `intent.md` - contract.

Run: `bash src/self_hosted/parity/lexer_parity.sh`
