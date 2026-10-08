#!/usr/bin/env bash
# G1: kernel-checked extraction of the one Claude-owned cleanup algorithm.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
command -v ocamlopt >/dev/null 2>&1 || { echo 'ocamlopt is required' >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo 'python3 is required for cost receipts' >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp "$ROOT_DIR/docs/semantics/proofs/OwnershipCleanCore.v" "$work/"
cp "$ROOT_DIR/docs/semantics/proofs/OwnershipCleanComposition.v" "$work/"
cp "$ROOT_DIR/docs/semantics/proofs/OwnershipCleanReadOnly.v" "$work/"
cp "$ROOT_DIR/docs/semantics/proofs/OwnershipCleanExits.v" "$work/"
cp "$ROOT_DIR/tests/coq/OwnershipCleanupExtraction.v" "$work/"
cp "$ROOT_DIR/tests/ocaml/ownership_clean_driver.ml" "$work/"
cp "$ROOT_DIR/scripts/rocq_toolchain_owner.sh" "$ROOT_DIR/tests/coq_kernel_check.sh" \
    "$ROOT_DIR/scripts/ownership_clean_cost_receipt.py" "$work/"
PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' PGY_ROCQ_EXTRACT_DIR="$work" \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh" >"$work/kernel.log" 2>&1 || {
        cat "$work/kernel.log" >&2; exit 1;
    }
tail -n 1 "$work/kernel.log"
(cd "$work" && ocamlopt -O2 -o ownership_clean_driver unix.cmxa \
    ownership_clean.mli ownership_clean.ml ownership_clean_driver.ml)
"$work/ownership_clean_driver" --selftest | tee "$work/controls.log"
# The driver has no caller-selected workload or silent parse defaults.
if "$work/ownership_clean_driver" --unknown >"$work/invalid.log" 2>&1; then
    echo 'ownership driver accepted an unknown command' >&2; exit 1
fi
grep -Fq 'usage:' "$work/invalid.log"
python3 "$ROOT_DIR/scripts/ownership_clean_cost_receipt.py" --work-dir "$work" \
    --output "$ROOT_DIR/.tmp/ownership-cleanup/model-cost.json"
echo '[ownership-clean] PASS (model/extraction only; compiler and runtime refinement OPEN)'
