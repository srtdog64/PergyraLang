# Lexer Substitution Track

Pergyra-written lexer that emits the same text `pgy --tokens` produces,
byte-for-byte. Mirrors C-side `src/lexer/`. The goal is byte-equal output,
not a full compiler rewrite.

- `main.pgy` - entry point orchestration only. Delegates argv/default source
  path selection and file-read failure policy to `source_input_owner.pgy`, and
  lexing to `scan_owner.pgy`. It must not import scan-loop internals directly.
- `scan_owner.pgy` - scan-loop SoT. Imports `char_owner.pgy` and
  `token_owner.pgy` because it consumes character classification and token
  formatting/classification facts.
- `source_input_owner.pgy` - lexer input SoT. Owns the source path contract and
  source file read boundary consumed by `scan_owner.pgy`.
- `fixture/` - committed `<base>_tokens.txt` baselines for the 7-source
  parity harness (`hello`, `array_literal`, `break_continue`, `basic`,
  `heap`, `binary_search`).
- `expected/clean.txt` - expected stdout when run on the default source.
- `intent.md` - contract.

Run: `bash tests/self_hosted/parity/lexer_parity.sh`
