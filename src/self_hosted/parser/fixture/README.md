# Parser Parity Fixtures

This directory holds inputs and baselines for
`src/self_hosted/parity/parser_parity.sh`. Each row in that script's
`SOURCE_PAIRS` array is `"<source path>:<base name>"` and the parity
test asserts that running the Pergyra-written parser on `<source>`
produces output byte-equal to `fixture/<base>_ast.txt`.

## File roles

Each file in this directory plays exactly one of the roles below.

### 1. Test pair (most files)

`<base>.pgy` + `<base>_ast.txt`. The `.pgy` is the input source and
the `_ast.txt` is the committed expected output (a frozen snapshot of
`pgy --ast <source>`). Example: `arith_let.pgy` + `arith_let_ast.txt`.

To add a new test pair:

1. Write the `.pgy` source.
2. Generate the baseline: `pgy --ast <path>.pgy > <base>_ast.txt`
   (LF-normalized — the parity script `tr -d '\r'`s on both sides).
3. Append `"<src path>:<base>"` to the parity script `SOURCE_PAIRS`.

### 2. External-source baseline

`<base>_ast.txt` only, no companion `.pgy`. The source lives outside
this directory (typically under `examples/`) and the `SOURCE_PAIRS`
row points at it. Example: `hello_ast.txt` is the baseline for
`examples/hello.pgy:hello`.

### 3. Support file (imported by a test pair)

`<base>.pgy` only, no companion `_ast.txt`. The file isn't tested
directly; another test pair `import`s it. Example: `math_lib.pgy`
exists because `import_simple.pgy` has `import "math_lib.pgy";` and
the parser's import resolver reads it from the source's directory.

## Live-managed file (not a fixture)

`source.txt` is the parity-script-managed pointer to the current
test's source path; the parser binary reads it to decide what file to
parse. Gitignored, overwritten on each test, removed by the parity
script's `trap` on exit. **Never edit it by hand and never commit
it.**

## Why not bundle all baselines into one file?

Considered and rejected. Per-file pairs keep `git diff` pointing at
the exact fixture that drifted, let the parity script print
fixture-specific BYTE-DRIFT messages, and let new fixtures land
without surgical edits inside a delimiter-heavy bundle.
