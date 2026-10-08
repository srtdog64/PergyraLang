#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
fail() { echo "[proof-carrying-adequacy] $*" >&2; exit 1; }

if ! command -v rocq >/dev/null 2>&1; then
    if [ "${PGY_ALLOW_MISSING_COQ:-0}" = 1 ]; then
        echo "[proof-carrying-adequacy] DECLARED SKIP: no prover; no checker adequacy was established"
        exit 0
    fi
    fail "no Coq/Rocq prover; keyword presence cannot establish adequacy"
fi
pgy_rocq_require
command -v ocamlc >/dev/null 2>&1 || fail "ocamlc is required to execute the extracted checker"
PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "Python is required for executable admission comparison"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
cp "$ROOT_DIR/docs/semantics/proofs/ProofCarryingIR.v" "$WORK_DIR/"
cp "$ROOT_DIR/tests/coq/ProofCarryingIRExtraction.v" "$WORK_DIR/"
# The existing kernel-policy owner compiles the fresh model/extraction and
# verifies this isolated core has no assumptions. No cached .vo is consumed.
PGY_COQ_PROOFS_DIR="$WORK_DIR" PGY_COQ_EXPECTED_AXIOMS="" PGY_ROCQ_EXTRACT_DIR="$WORK_DIR" \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
cp "$ROOT_DIR/tests/coq/proof_certificate_checker_driver.ml" "$WORK_DIR/"
(
    cd "$WORK_DIR"
    ocamlc -c proof_certificate_checker.mli
    ocamlc -c proof_certificate_checker.ml
    ocamlc -o checker proof_certificate_checker.cmo proof_certificate_checker_driver.ml
)
PYTHONPATH="$ROOT_DIR/scripts${PYTHONPATH:+:$PYTHONPATH}" \
    "$PYTHON_BIN" "$ROOT_DIR/tests/proof_certificate_adequacy.py" "$WORK_DIR/checker"
echo "[proof-carrying-adequacy] fresh kernel-checked extraction and executable envelope admission agree"
