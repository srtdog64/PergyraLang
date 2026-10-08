#!/usr/bin/env bash
# Fresh, isolated kernel validation of the async/reuse red-team slice.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
snapshot="$(mktemp -d)"
trap 'rm -rf "$snapshot"' EXIT
for owner in AsyncScopeCore AsyncLifecycleCore ParallelSchedulingCore \
             WitnessDataRace CoordinationCore AsyncContextCore CompensationCore; do
    cp "$ROOT_DIR/docs/semantics/proofs/$owner.v" "$snapshot/"
done
cp "$ROOT_DIR/tests/coq/AsyncReuseRedteamRegression.v" "$snapshot/"
PGY_COQ_PROOFS_DIR="$snapshot" PGY_COQ_EXPECTED_AXIOMS='' \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
echo 'async-reuse-redteam: PASS -- fixed fresh owners and regression consumer'
