# Lexer Parity Fixtures

This directory holds baselines for
`src/self_hosted/parity/lexer_parity.sh`. Each row in that script's
`SOURCE_PAIRS` array is `"<source path>:<base>_tokens.txt"` and the
parity test asserts that running the Pergyra-written lexer on
`<source>` produces output byte-equal to `fixture/<base>_tokens.txt`.

## File role

`<base>_tokens.txt` — committed expected output, a frozen snapshot of
`pgy --tokens <source>`. The source itself lives outside this
directory (under `examples/`) and the `SOURCE_PAIRS` row points at it.

To add a new pair:

1. Pick a source under `examples/`.
2. Generate the baseline:
   `pgy --tokens examples/<file>.pgy > <base>_tokens.txt`
   (LF-normalized — the parity script `tr -d '\r'`s on both sides).
3. Append `"examples/<file>.pgy:<base>_tokens.txt"` to the parity
   script `SOURCE_PAIRS`.

The lexer fixture directory does **not** carry `.pgy` sources of its
own; all sources are external. Parser fixtures behave differently —
see `src/self_hosted/parser/fixture/README.md`.

## Live-managed file (not a fixture)

`source.txt` is the parity-script-managed pointer to the current
test's source path; the lexer binary reads it to decide what file to
lex. Gitignored, overwritten on each test, removed by the parity
script's `trap` on exit. **Never edit it by hand and never commit
it.**
