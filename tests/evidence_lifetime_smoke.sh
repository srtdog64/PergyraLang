#!/usr/bin/env bash
#
# evidence_lifetime_smoke.sh — WO-A3: every AIREvidenceKind must have a
# lifetime contract row (docs/semantics/evidence_kind_manifest.md) and every
# row must correspond to a live enum entry and existing producer/gate files.
#
# The check is a two-way exact correspondence, so it bites in both
# directions: a new evidence kind added to air.h without a manifest row is
# RED (the ratchet that forces lifetime declarations), and a stale row for a
# removed kind is RED (no paperwork rot). The script ends with a built-in
# RED self-test: it re-runs the correspondence check against a mutilated
# manifest copy and fails unless that check fails.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AIR_H="$ROOT_DIR/src/compiler/air.h"
MANIFEST="$ROOT_DIR/docs/semantics/evidence_kind_manifest.md"

fail() { echo "[evidence-lifetime] FAIL: $*" >&2; exit 1; }

[[ -f "$AIR_H" ]] || fail "missing $AIR_H"
[[ -f "$MANIFEST" ]] || fail "missing $MANIFEST"

extract_enum_kinds() {
    # AIR_EVIDENCE_* enum entries, excluding the COUNT sentinel and the
    # provider/subject enums (their entries carry PROVIDER_/SUBJECT_).
    sed -n '/typedef enum/,/AIREvidenceKind;/p' "$AIR_H" \
        | grep -oE 'AIR_EVIDENCE_[A-Z0-9_]+' \
        | grep -v 'KIND_COUNT' \
        | grep -v 'PROVIDER_' \
        | grep -v 'SUBJECT_' \
        | sort -u
}

extract_manifest_kinds() {
    local manifest="$1"
    sed -n '/BEGIN evidence-kind-manifest/,/END evidence-kind-manifest/p' \
        "$manifest" \
        | grep -oE '^\| AIR_EVIDENCE_[A-Z0-9_]+' \
        | sed 's/^| //' \
        | sort -u
}

correspondence_check() {
    # args: <enum-list-file> <manifest-path>; returns nonzero on mismatch.
    local enum_file="$1" manifest="$2"
    local manifest_file
    manifest_file="$(mktemp)"
    extract_manifest_kinds "$manifest" > "$manifest_file"

    local missing_rows stale_rows
    missing_rows="$(comm -23 "$enum_file" "$manifest_file")"
    stale_rows="$(comm -13 "$enum_file" "$manifest_file")"
    rm -f "$manifest_file"

    if [[ -n "$missing_rows" ]]; then
        echo "[evidence-lifetime] evidence kinds without a lifetime row:" >&2
        echo "$missing_rows" >&2
        return 1
    fi
    if [[ -n "$stale_rows" ]]; then
        echo "[evidence-lifetime] manifest rows without a live enum kind:" >&2
        echo "$stale_rows" >&2
        return 1
    fi
    return 0
}

ENUM_FILE="$(mktemp)"
trap 'rm -f "$ENUM_FILE"' EXIT
extract_enum_kinds > "$ENUM_FILE"

ENUM_COUNT="$(wc -l < "$ENUM_FILE" | tr -d ' ')"
[[ "$ENUM_COUNT" -ge 1 ]] || fail "extracted no evidence kinds from air.h"

correspondence_check "$ENUM_FILE" "$MANIFEST" \
    || fail "enum/manifest correspondence broken (see above)"

# Every producer file and every referenced gate must exist.
while IFS= read -r producer; do
    [[ -f "$ROOT_DIR/$producer" ]] \
        || fail "manifest names missing producer file: $producer"
done < <(sed -n '/BEGIN evidence-kind-manifest/,/END evidence-kind-manifest/p' \
            "$MANIFEST" \
         | grep -oE 'src/compiler/[a-z0-9_]+\.c' | sort -u)

for gate in \
    "tests/air_json_schema_smoke.sh" \
    "tests/air_drift_smoke.sh" \
    "tests/evidence_lifetime_smoke.sh"; do
    grep -Fq "$gate" "$MANIFEST" || fail "manifest missing gate link: $gate"
    [[ -f "$ROOT_DIR/$gate" ]] || fail "manifest names missing gate: $gate"
done

# Contract vocabulary: the manifest must speak docs/09's language.
for term in \
    "last consumer" \
    "summarize" \
    "docs/semantics/09"; do
    grep -Fq "$term" "$MANIFEST" || fail "manifest missing term: $term"
done

# RED self-test: drop one row and require the correspondence check to fail.
RED_MANIFEST="$(mktemp)"
grep -v 'AIR_EVIDENCE_HIR_ROUTINE' "$MANIFEST" > "$RED_MANIFEST"
if correspondence_check "$ENUM_FILE" "$RED_MANIFEST" 2>/dev/null; then
    rm -f "$RED_MANIFEST"
    fail "RED self-test did not fire: a missing row must break correspondence"
fi
rm -f "$RED_MANIFEST"

echo "[evidence-lifetime] $ENUM_COUNT evidence kinds have lifetime rows; enum<->manifest correspondence exact; RED self-test fired"
