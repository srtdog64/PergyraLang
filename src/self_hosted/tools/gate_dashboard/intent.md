# Gate Dashboard - Intent / Contract

**Status:** active self-host operational owner.

## Intent

Expose the active hard self-host gate frontier from Pergyra code. The
dashboard separates declared gate state from observed run state so an
unexecuted gate can never appear green.

## Input Contract

The canonical manifest has no file input. Optional result input must use the
`pgy.selfhost.gate-results.v1` line schema and reference only manifest gate IDs.

## Ownership

- `compiler/gate_dashboard_owner.pgy` owns gate identity, Make target, tier,
  time budget, declared state, blocking policy, and related owner fact.
- It is the only Gate SoT. No shell script, golden JSON, result TSV, or
  architectural document may define a second gate list, status, tier, budget,
  or current-health authority.
- `result_owner.pgy` validates the runner artifact. Unknown IDs, duplicate IDs,
  malformed rows, and invalid durations fail closed.
- `report_owner.pgy` owns stable JSON projection and summary derivation.
- Shell may execute manifest targets and record process outcomes. It may not
  reconstruct tiers, budgets, owner facts, or dashboard health.

## CLI

- `--manifest`: emit the canonical line-oriented execution plan.
- no arguments: emit a snapshot with every run state set to `NOT_RUN`.
- `--results PATH`: validate a `pgy.selfhost.gate-results.v1` artifact and
  combine it with the owner manifest.

The tool exits nonzero for malformed result artifacts and for observed `FAIL`
or over-budget rows. `NOT_RUN` remains visible but does not make snapshot
generation fail.

## Output Contract

Manifest mode emits seven tab-separated owner fields per gate. JSON mode emits
the stable dashboard schema, a derived summary, and one declared/run row per
gate. Missing observations remain `NOT_RUN`; budget excess remains explicit.
The process bridge must consume the manifest budget through
`pgy_run_with_timeout`; a budget is an execution bound, not display metadata.

## Oracle

`tests/self_hosted/parity/gate_dashboard_parity.sh` pins the JSON golden,
validates every Make target, rejects unknown and duplicate result IDs, and
rejects over-budget results, verifies the portable timeout binding, and
compares C/LLVM projection output.

## Progress Rule

This dashboard is supporting evidence. Its source, tests, and LOC do not count
as self-host substitution progress. Only a Pergyra implementation replacing a
real C-owned compiler path changes the substitution ledger.
