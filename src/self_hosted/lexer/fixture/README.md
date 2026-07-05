# Lexer Parity Fixtures

This directory holds baselines for `tests/self_hosted/parity/lexer_parity.sh`.
Each row emitted by `src/self_hosted/lexer/fixture_manifest_owner.pgy` is
`"<source path>:<base>_tokens.txt"`, and the parity test asserts that running
the Pergyra-written lexer on `<source>` produces output byte-equal to
`fixture/<base>_tokens.txt`. The shell runner consumes that manifest; it does
not own the source/fixture mapping.

## File Role

`<base>_tokens.txt` is the committed expected output, a frozen snapshot of
`pgy --tokens <source>`. The source itself lives outside this directory under
`examples/` or `tests/cases/backend_compare/`, and the manifest row points at
it.

To add a new pair:

1. Pick a source under `examples/` or `tests/cases/backend_compare/`.
2. Generate the baseline:
   `pgy --tokens <source>.pgy > <base>_tokens.txt`
   (LF-normalized; the parity script `tr -d '\r'`s on both sides).
3. Add the `"<source>.pgy:<base>_tokens.txt"` row to
   `fixture_manifest_owner.pgy`.

The lexer fixture directory does not carry `.pgy` sources of its own; all
sources are external. Parser fixtures behave differently; see
`src/self_hosted/parser/fixture/README.md`.

## Input Boundary

This directory must not carry a `source.txt` side-channel. Parity, scale probes,
and semantic selfcheck pass the source path through `Args()[0]`.
