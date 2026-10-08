#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
snapshot="$(mktemp -d)"
trap 'rm -rf "$snapshot"' EXIT
for owner in GuardWitnessBinding IRMinimality IntentObligations WholeProgramCore AIRBinding BinaryAdequacy; do
    cp "$ROOT_DIR/docs/semantics/proofs/$owner.v" "$snapshot/"
done
cp "$ROOT_DIR/tests/coq/ProofRedteamMainRegression.v" "$snapshot/"
PGY_COQ_PROOFS_DIR="$snapshot" PGY_COQ_EXPECTED_AXIOMS='' \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
echo '[proof-redteam-main] arithmetic/IR/emission/config regressions PASS'
