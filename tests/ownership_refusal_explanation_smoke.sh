#!/usr/bin/env bash
# Bounded model, proposition consumers and freshly extracted reference checker.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
[[ "$(uname -s)" == Linux ]] || { echo '[refusal-explanation] Linux is required' >&2; exit 1; }
pgy_rocq_require
command -v ocamlopt >/dev/null 2>&1 || { echo 'ocamlopt is required' >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
receipt_base="$ROOT_DIR/.tmp/ownership-refusal-explanation"
mkdir -p "$receipt_base"
receipt="$(mktemp -d "$receipt_base/run.XXXXXX")"
proofs=(
  docs/semantics/proofs/OwnershipCleanCore.v
  docs/semantics/proofs/OwnershipRefusalExplanation.v
  tests/coq/OwnershipRefusalExplanationAudit.v
  tests/coq/OwnershipRefusalExplanationExtraction.v
)
inputs=("${proofs[@]}" tests/ocaml/ownership_refusal_driver.ml
  tests/ownership_refusal_explanation_smoke.sh tests/coq_kernel_check.sh
  scripts/rocq_toolchain_owner.sh scripts/run_rocq_toolchain.sh)
(cd "$ROOT_DIR" && sha256sum "${inputs[@]}") >"$receipt/source.sha256"
for input in "${inputs[@]}"; do
  cp "$ROOT_DIR/$input" "$work/"
  expected="$(awk -v path="$input" '$2 == path { print $1 }' "$receipt/source.sha256")"
  observed="$(sha256sum "$work/$(basename "$input")")"
  [[ "$expected" == "${observed%% *}" ]] || {
    echo "[refusal-explanation] source changed while snapshotting: $input" >&2; exit 1;
  }
done
# Run the captured driver/owner, not a possibly changed source generation.
# coq_kernel_check.sh derives ROOT_DIR from its location; give it its layout.
mkdir -p "$work/tests" "$work/scripts" "$work/docs/semantics/proofs"
cp "$work/coq_kernel_check.sh" "$work/tests/"
cp "$work/rocq_toolchain_owner.sh" "$work/scripts/"
PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' PGY_ROCQ_EXTRACT_DIR="$work" \
  bash "$work/tests/coq_kernel_check.sh" >"$receipt/kernel.log" 2>&1 || {
    cat "$receipt/kernel.log" >&2; exit 1;
  }
(cd "$work" && ocamlopt -o ownership_refusal_driver \
  ownership_refusal.mli ownership_refusal.ml ownership_refusal_driver.ml)
"$work/ownership_refusal_driver" --selftest | tee "$receipt/controls.log"
if "$work/ownership_refusal_driver" --unknown >"$receipt/invalid-argument.log" 2>&1; then
  echo '[refusal-explanation] driver accepted an unknown command' >&2; exit 1
fi
grep -Fq 'usage:' "$receipt/invalid-argument.log"
(cd "$work" && sha256sum ownership_refusal.mli ownership_refusal.ml ownership_refusal_driver) \
  >"$receipt/artifacts.sha256"
cp "$work/ownership_refusal.mli" "$work/ownership_refusal.ml" "$work/ownership_refusal_driver" "$receipt/"
(cd "$ROOT_DIR" && sha256sum --check "$receipt/source.sha256") >"$receipt/source-after.log" 2>&1 || {
  cat "$receipt/source-after.log" >&2
  echo '[refusal-explanation] source generation changed during this run' >&2; exit 1
}
tail -n 2 "$receipt/kernel.log"
echo "[refusal-explanation] receipt: $receipt"
echo '[refusal-explanation] PASS (bounded model/checker; production export, source binding and DRV-2 coverage OPEN)'
