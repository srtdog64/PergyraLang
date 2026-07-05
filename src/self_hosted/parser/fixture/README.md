# Parser Parity Fixtures

This directory holds inputs and baselines for
`tests/self_hosted/parity/parser_parity.sh`. The source/fixture row inventory
is owned by `../fixture_manifest_owner.pgy`, emitted through
`--fixture-manifest`, and consumed by the shell runner. Each row is
`"<source path>:<base name>"`; the parity test asserts that running the
Pergyra-written parser on `<source>` produces output byte-equal to
`fixture/<base>_ast.txt`.

## File Roles

Each file in this directory plays exactly one role.

### 1. Test Pair

`<base>.pgy` plus `<base>_ast.txt`. The `.pgy` is the input source and the
`_ast.txt` is the committed expected output, a frozen snapshot of
`pgy --ast <source>`. Example: `arith_let.pgy` plus `arith_let_ast.txt`.

To add a new test pair:

1. Write the `.pgy` source.
2. Generate the baseline: `pgy --ast <path>.pgy > <base>_ast.txt`
   (LF-normalized; the parity script `tr -d '\r'`s on both sides).
3. Ensure `fixture_manifest_owner.pgy` can discover the pair. Ordinary
   in-directory pairs are discovered automatically; external-source baselines
   need an explicit owner row.

### 2. External-Source Baseline

`<base>_ast.txt` only, no companion `.pgy`. The source lives outside this
directory, typically under `examples/`, and the manifest owner emits its row.
Example: `hello_ast.txt` is the baseline for `examples/hello.pgy:hello`, which
is emitted by `fixture_manifest_owner.pgy`.

### 3. Support File

`<base>.pgy` only, no companion `_ast.txt`. The file is not tested directly;
another test pair imports it. Example: `math_lib.pgy` exists because
`import_simple.pgy` has `import "math_lib.pgy";` and the parser's import
resolver reads it from the source's directory.

## Legacy Fallback

`source.txt` is retained only as a local fallback for older probes. The normal
parity path passes the source through `Args()[0]`.

## Why Not Bundle All Baselines Into One File?

Per-file pairs keep `git diff` pointing at the exact fixture that drifted, let
the parity script print fixture-specific byte-drift messages, and let new
fixtures land without delimiter-heavy edits.
