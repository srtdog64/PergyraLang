#!/usr/bin/env bash
#
# Abstraction-loss-contract adequacy.
#
# Binds the prose loss boundaries (docs/semantics/09_abstraction_loss_contracts.md)
# to the compiler via the manifest docs/semantics/loss_contract_manifest.md. For
# each boundary it asserts:
#   - the stage artifact (the compiler stage the boundary connects) exists
#   - if the boundary claims enforcement, the named gate script exists
# and it reports how many boundaries are gate-enforced vs documentation-only.
# It does NOT claim every loss rule is mechanically checked; it measures the gap
# and fails on drift (a moved stage or a deleted gate).
#
# Pure source-consistency (no coqc).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/semantics/loss_contract_manifest.md"
PROSE="$ROOT_DIR/docs/semantics/09_abstraction_loss_contracts.md"

for f in "$MANIFEST" "$PROSE"; do
    [[ -e "$f" ]] || { echo "missing required file: $f" >&2; exit 1; }
done

fail=0
rows=0
enforced=0

manifest_rows() {
    awk '/<!-- BEGIN loss-contract-manifest -->/{f=1;next}
         /<!-- END loss-contract-manifest -->/{f=0}
         f && $0 !~ /^```/ && NF' "$MANIFEST"
}

echo "== abstraction-loss boundary -> stage + enforcement gate =="
while read -r boundary stage gate status; do
    [[ -z "$boundary" ]] && continue
    rows=$((rows + 1))

    if [[ ! -e "$ROOT_DIR/$stage" ]]; then
        echo "  FAIL: boundary $boundary stage artifact '$stage' does not exist"; fail=1; continue
    fi
    if [[ "$status" == "enforced" ]]; then
        if [[ "$gate" == "documented" ]]; then
            echo "  FAIL: $boundary marked enforced but names no gate"; fail=1; continue
        fi
        if [[ ! -e "$ROOT_DIR/$gate" ]]; then
            echo "  FAIL: $boundary enforcement gate '$gate' does not exist"; fail=1; continue
        fi
        enforced=$((enforced + 1))
    fi
    printf '  ok   %-20s %-28s %-10s %s\n' "$boundary" "$stage" "$status" "$gate"
done < <(manifest_rows)

echo "-- coverage: $enforced/$rows boundaries are gate-enforced (rest documentation-only; see manifest) --"

if [[ "$rows" -eq 0 ]]; then
    echo "loss contract adequacy: FAILED (no manifest rows parsed)"
    exit 1
fi
if [[ "$fail" -ne 0 ]]; then
    echo "loss contract adequacy: FAILED"
    exit 1
fi

echo "loss contract adequacy: ok (every boundary stage exists; every enforced boundary names a live gate)"
