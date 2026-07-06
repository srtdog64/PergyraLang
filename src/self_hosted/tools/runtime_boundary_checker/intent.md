# Runtime Boundary Checker Intent

## Intent

Guard the hard-self-host runtime boundary. The checker prevents the docs from
claiming a full Pergyra runtime replacement while allowing portable runtime
policy work to move into Pergyra.

## Input Contract

The tool reads these repository files from the current working directory:

- `src/self_hosted/runtime/README.md`
- `src/self_hosted/PROGRESS.md`
- `src/self_hosted/README.md`
- `docs/self_hosted/05_compiler_core_gap_analysis.md`

## Output Contract

The tool emits one JSON line with schema
`pgy.selfhost.runtime-boundary.v1`. It exits `0` when every required boundary
term is present and exits `1` with `findings[]` when a term or input file is
missing.

## Oracle

`tests/self_hosted/parity/runtime_boundary_checker_parity.sh` treats the
Pergyra checker JSON as the oracle: the clean verdict byte-matches
`expected/clean.json`, and a synthetic missing-term fixture byte-matches
`expected/missing_term.json`. Shell may execute the compiled tool, read its
`--terms` manifest to construct the scratch fixture, and assert the negative
fixture exits `1`; it must not re-grep the clean documents as a second semantic
implementation.
