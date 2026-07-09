# Completeness Impact Planner

## Intent

Own the runnable self-hosted check that turns completeness impact facts into a
proof-gate plan for an explicit list of changed paths.

## Input Contract

Arguments are repo-relative changed paths. Git state and timestamps are not
read here; callers provide the changed path list.

## Output Contract

The tool emits one JSON object with schema
`pgy.selfhost.completeness-impact-planner.v1`, counts, proof-gate names, and
unmatched-path findings. Any unmatched path exits non-zero.

## Oracle

The parity script compiles this Pergyra tool through the C backend and, when
available, the LLVM backend. Both outputs are compared against committed JSON
artifacts through the backend-output comparator.
