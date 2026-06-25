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

`tests/self_hosted/parity/runtime_boundary_checker_parity.sh` compares the
Pergyra result against shell `grep` checks for the same required terms, verifies
the committed clean JSON, and checks a synthetic missing-term fixture.
