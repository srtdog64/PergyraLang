#!/usr/bin/env bash
# A shrink-only residue ratchet, not an executable ownership proof.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
inventory="$ROOT_DIR/tests/fixtures/ownership_clean_retired_owners.txt"
command -v grep >/dev/null 2>&1 || { echo 'grep is required for the residue ratchet' >&2; exit 1; }
count=0
patterns="$(mktemp)"
trap 'rm -f "$patterns"' EXIT
while IFS= read -r rel; do
    [[ "$rel" =~ ^src/self_hosted/semantic/[a-z_]+\.pgy$ ]] || { echo 'invalid retired-owner inventory row' >&2; exit 1; }
    [[ ! -e "$ROOT_DIR/$rel" ]] || { echo "superseded manual ownership owner returned: $rel" >&2; exit 1; }
    printf '%s\n' "${rel##*/}" >> "$patterns"
    count=$((count + 1))
done <"$inventory"
[[ "$count" -gt 0 ]] || { echo 'empty retired-owner inventory' >&2; exit 1; }
# One traversal owns residue discovery. Historical audits/recovery artifacts
# are excluded, and an I/O/tool failure is not the no-match outcome.
if grep -R -l -F --include='*.pgy' -f "$patterns" "$ROOT_DIR/src/self_hosted"; then
    echo 'dangling/retired source owner reference' >&2; exit 1
else
    search_status=$?
    [[ "$search_status" -eq 1 ]] || { echo "retired-owner search failed ($search_status)" >&2; exit 1; }
fi
[[ ! -e "$ROOT_DIR/docs/semantics/proofs/OwnershipCleanup.v" ]] || { echo 'second cleanup model returned' >&2; exit 1; }
[[ -f "$ROOT_DIR/docs/semantics/proofs/OwnershipCleanCore.v" ]] || { echo 'canonical cleanup core is missing' >&2; exit 1; }
echo "[ownership-clean-direction] $count retired manual-chain owners and the second cleanup model absent; structural evidence only"
