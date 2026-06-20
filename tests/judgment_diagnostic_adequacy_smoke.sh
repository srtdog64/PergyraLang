#!/usr/bin/env bash
#
# Judgment -> diagnostic-code adequacy.
#
# Binds the prose domain judgments (the `|-` rules in docs/semantics/0x) to the
# real compiler diagnostic codes via the manifest in
# docs/semantics/judgment_diagnostic_map.md. For each manifest row it asserts:
#   - the diagnostic code is a real entry in src/semantic/diag_codes.h
#   - the named checker exists and actually emits that code
# and it reports how many judgments have a *dedicated* code (the 1:1 ideal) vs
# how many are *folded*/*split* (the measured gap). It does NOT claim 1:1
# completeness; it measures the distance to it and fails on drift.
#
# Pure source-consistency (no coqc).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/semantics/judgment_diagnostic_map.md"
DIAG_CODES="$ROOT_DIR/src/semantic/diag_codes.h"
SEM_DIR="$ROOT_DIR/src/semantic"

for f in "$MANIFEST" "$DIAG_CODES"; do
    [[ -e "$f" ]] || { echo "missing required file: $f" >&2; exit 1; }
done

fail=0
rows=0
dedicated=0

# Extract the manifest rows between the markers, dropping the ``` fences.
manifest_rows() {
    awk '/<!-- BEGIN judgment-diagnostic-manifest -->/{f=1;next}
         /<!-- END judgment-diagnostic-manifest -->/{f=0}
         f && $0 !~ /^```/ && NF' "$MANIFEST"
}

echo "== judgment -> diagnostic code (manifest vs compiler) =="
while read -r judgment code status checker; do
    [[ -z "$judgment" ]] && continue
    rows=$((rows + 1))
    [[ "$status" == "dedicated" ]] && dedicated=$((dedicated + 1))
    checker_path="$SEM_DIR/$checker"

    if ! grep -qE "\b$code\b" "$DIAG_CODES"; then
        echo "  FAIL: $judgment -> $code is not declared in diag_codes.h"; fail=1; continue
    fi
    if [[ ! -e "$checker_path" ]]; then
        echo "  FAIL: $judgment checker $checker does not exist"; fail=1; continue
    fi
    if ! grep -qE "\b$code\b" "$checker_path"; then
        echo "  FAIL: checker $checker does not emit $code (manifest stale for '$judgment')"; fail=1; continue
    fi
    printf '  ok   %-14s %-34s %-10s %s\n' "$judgment" "$code" "$status" "$checker"
done < <(manifest_rows)

echo "-- coverage: $dedicated/$rows judgments have a dedicated 1:1 code (rest folded/split; see manifest) --"

if [[ "$rows" -eq 0 ]]; then
    echo "judgment diagnostic adequacy: FAILED (no manifest rows parsed)"
    exit 1
fi
if [[ "$fail" -ne 0 ]]; then
    echo "judgment diagnostic adequacy: FAILED"
    exit 1
fi

echo "judgment diagnostic adequacy: ok (every mapped code is real and emitted by its checker)"
