# Lexer Substitution Track

Pergyra-written lexer that emits the same text `pgy --tokens` produces,
byte-for-byte. Mirrors C-side `src/lexer/`. The goal is byte-equal
output, not a full compiler rewrite.

- `main.pgy` — entry point. Reads `fixture/source.txt` when present
  (parity-script-managed override) or defaults to `examples/hello.pgy`.
- `fixture/` — committed `<base>_tokens.txt` baselines used by the
  parity harness.
- `expected/clean.txt` — expected stdout when run on the default source.
- `intent.md` — contract.

Run: `bash src/self_hosted/parity/lexer_parity.sh`
