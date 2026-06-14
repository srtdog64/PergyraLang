# Lexer -- Intent / Contract

**Status:** *rung-1 minimal* (2026-05-27). First Pergyra-origin tool that
**substitutes a compiler-internal component** (the C-side
`src/lexer/lexer.c`) instead of observing the compiler from outside.

## Intent

The 10 previous self-host tools were all *peripheral*: they read text, count
things, validate invariants, but never *replaced* any component of the C
compiler. This tool is the first that does. It reimplements (a tiny subset
of) the Pergyra lexer in Pergyra itself, emitting output byte-equal to
`pgy --tokens` for a known-good input.

This is the genuine first step toward *Stage 3 / 4* of the staged self-host
roadmap (`docs/self_hosted/01_staged_roadmap.md`): "bounded compiler
owner reimplemented in Pergyra, both implementations run side-by-side,
output compared, rollback to C trivial".

## Input Contract

- **source_owner**: `examples/hello.pgy` and `examples/array_literal.pgy`
  (text, UTF-8). The parity script passes the source path through
  `Args()[0]`; `fixture/source.txt` is retained only as a local fallback for
  older probes.
- The lexer subset handles only the token families needed by these
  fixtures: common keywords, identifiers, decimal integer literals,
  double-quoted string literals, punctuation, simple operators, `//`
  line comments, whitespace skip, and EOF.

A more general lexer (escaped strings, multiline strings, full numeric
literal families, diagnostics, and every keyword/operator combination) is
**not in scope** here -- this is a proof that Pergyra can substitute compiler
internals, not a full production-quality lexer.

## Output Contract

Plain text matching the `pgy --tokens` token payload byte-for-byte:

```
=== tokens: examples/hello.pgy ===
   1  Token{type: FUNC, text: "func", line: 7, col: 1}
  ...
  14  Token{type: EOF, text: "", line: 10, col: 1}
14 tokens total
```

Exit code: `0` on success, `1` if the input source cannot be read.

## Oracle

The C-side reference is `pgy --tokens <source>` for each source/fixture
pair. The parity rung asserts the Pergyra lexer's token payload is byte-equal
to the C lexer's token payload. The harness strips runner-level CRLF and
`pgy: compiled ...` noise before comparison.

The parity rung (`src/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- The Pergyra origin's stdout byte-equals the committed fixture for each
  source pair (`hello_tokens.txt`, `array_literal_tokens.txt`), each generated
  by `pgy --tokens`.
- A live-drift guard re-runs `pgy --tokens` and confirms the committed
  fixture still matches (graceful-skip when the sandboxed shell cannot
  launch the pgy subprocess, same pattern as the AIR validator).

## Why Now

The BDFL course-corrected the loop (2026-05-27 directive): stop building
peripheral tools that watch the C compiler, start substituting C/LLVM
components with Pergyra-written equivalents where currently feasible.
The lexer is the smallest compiler-internal component with a clean
input/output boundary (source text in, token list out), no AST or MIR
dependencies, and trivial rollback (the C lexer stays in tree). It
proves Pergyra can do *real* compiler work, not just audits.

## Not In Scope

- Lexing arbitrary Pergyra source. This subset handles only the committed
  source pairs listed in the parity script.
- Escaped strings, multiline strings, non-decimal numeric literals, and full
  lexical diagnostic recovery.
- Error recovery / diagnostics on bad input.
- Performance parity with C lexer.
