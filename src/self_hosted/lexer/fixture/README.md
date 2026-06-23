# Lexer Parity Fixtures

This directory holds baselines for `src/self_hosted/parity/lexer_parity.sh`.
Each row in that script's `SOURCE_PAIRS` array is
`"<source path>:<base>_tokens.txt"`, and the parity test asserts that running
the Pergyra-written lexer on `<source>` produces output byte-equal to
`fixture/<base>_tokens.txt`.

## File Role

`<base>_tokens.txt` is the committed expected output, a frozen snapshot of
`pgy --tokens <source>`. The source itself lives outside this directory under
`examples/` or `tests/cases/backend_compare/`, and the `SOURCE_PAIRS` row
points at it.

To add a new pair:

1. Pick a source under `examples/` or `tests/cases/backend_compare/`.
2. Generate the baseline:
   `pgy --tokens <source>.pgy > <base>_tokens.txt`
   (LF-normalized; the parity script `tr -d '\r'`s on both sides).
3. Append `"<source>.pgy:<base>_tokens.txt"` to the parity script
   `SOURCE_PAIRS`.

The lexer fixture directory does not carry `.pgy` sources of its own; all
sources are external. Parser fixtures behave differently; see
`src/self_hosted/parser/fixture/README.md`.

## Legacy Fallback

`source.txt` is retained only as a local manual fallback. The normal parity and
scale-probe paths pass the source through `Args()[0]`.
