#!/usr/bin/env bash
# Prove that weakening the checker or a theorem breaks the independent gate.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
[[ "$(uname -s)" == Linux ]] || { echo 'Linux is required' >&2; exit 1; }
pgy_rocq_require
command -v python3 >/dev/null 2>&1 || { echo 'python3 is required' >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$ROOT_DIR/.tmp/ownership-refusal-explanation"
receipt="$(mktemp -d "$ROOT_DIR/.tmp/ownership-refusal-explanation/selftest.XXXXXX")"
inputs=(docs/semantics/proofs/OwnershipCleanCore.v
  docs/semantics/proofs/OwnershipRefusalExplanation.v tests/coq/OwnershipRefusalExplanationAudit.v
  tests/ownership_refusal_explanation_selftest.sh scripts/rocq_toolchain_owner.sh)
(cd "$ROOT_DIR" && sha256sum "${inputs[@]}") >"$receipt/source.sha256"
for input in "${inputs[@]}"; do
  cp "$ROOT_DIR/$input" "$work/"
  expected="$(awk -v path="$input" '$2 == path { print $1 }' "$receipt/source.sha256")"
  observed="$(sha256sum "$work/$(basename "$input")")"
  [[ "$expected" == "${observed%% *}" ]] || { echo "snapshot changed: $input" >&2; exit 1; }
done
(cd "$work" && "${PGY_ROCQ_COMPILE[@]}" -q -Q . '' OwnershipCleanCore.v) >"$receipt/core.log" 2>&1
python3 - "$work" <<'PY'
import pathlib, re, sys
base = pathlib.Path(sys.argv[1])
source = (base / 'OwnershipRefusalExplanation.v').read_text()
mutations = {
    'unrelated-rule': ('member edge_eq_dec e (edges p) &&', 'true &&'),
    'unrelated-reader': ('StaleRead r _ => Nat.eqb reader r', 'StaleRead r _ => true'),
}
for label, (old, new) in mutations.items():
    if source.count(old) != 1:
        raise SystemExit(f'{label}: expected one mutation site')
    folder = base / label
    folder.mkdir()
    (folder / 'OwnershipRefusalExplanation.v').write_text(source.replace(old, new))
weakened, count = re.subn(
    r'Theorem remote_break_attributed :.*?\nQed\.',
    'Theorem remote_break_attributed : True.\nProof. exact I. Qed.', source, flags=re.S)
if count != 1:
    raise SystemExit('weakened-theorem: expected one theorem')
folder = base / 'weakened-theorem'
folder.mkdir()
(folder / 'OwnershipRefusalExplanation.v').write_text(weakened)
PY
for label in unrelated-rule unrelated-reader weakened-theorem; do
  cp "$work/OwnershipCleanCore.vo" "$work/$label/"
  cp "$work/OwnershipRefusalExplanationAudit.v" "$work/$label/"
  if [[ "$label" == weakened-theorem ]]; then
    (cd "$work/$label" && "${PGY_ROCQ_COMPILE[@]}" -q -Q . '' OwnershipRefusalExplanation.v) \
      >"$receipt/$label-owner.log" 2>&1
    rejected=OwnershipRefusalExplanationAudit.v
  else
    rejected=OwnershipRefusalExplanation.v
  fi
  if (cd "$work/$label" && "${PGY_ROCQ_COMPILE[@]}" -q -Q . '' "$rejected") \
      >"$receipt/$label.log" 2>&1; then
    echo "[refusal-explanation-selftest] FAIL: $label was accepted" >&2; exit 1
  fi
  grep -Fq "$rejected" "$receipt/$label.log"
  grep -Fq 'Error:' "$receipt/$label.log"
  echo "[refusal-explanation-selftest] refused: $label ($rejected)"
done
(cd "$ROOT_DIR" && sha256sum --check "$receipt/source.sha256") >"$receipt/source-after.log" 2>&1
echo "[refusal-explanation-selftest] PASS (3 planted regressions; receipt: $receipt)"
