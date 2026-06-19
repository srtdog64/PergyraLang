#!/usr/bin/env bash
#
# Binary adequacy for the axis fact-ownership proof
# (docs/semantics/proofs/AxisOwnership.v section 8).
#
# AxisOwnership.v proves, INSIDE Coq, that the keyword->axis table is
# consistent with the fact-ownership relation (keyword_axis_sound). That
# theorem constrains the *model*. It cannot, on its own, know whether the
# model still matches the *real compiler*. This is the differential test that
# closes that gap: it pins three layers against each other so drift in any one
# of them fails the gate.
#
#   (1) Coq    keyword_axis   (AxisOwnership.v section 8, mirrored below)
#   (2) Design docs/42 section 0 axis -> surface keyword table
#   (3) Impl   the keywords the compiler actually recognizes
#              (reserved in src/lexer/lexer_keywords.c, or contextual in
#               src/parser/**)
#
# Checks:
#   A. every docs/42 axis keyword is recognized by the compiler        (2 ⊆ 3)
#   B. every Coq-mirrored keyword sits on the same axis in docs/42     (1 = 2)
#
# This is a pure source-consistency test (no coqc); it complements the Coq
# proof rather than re-checking it.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC42="$ROOT_DIR/docs/42_keyword_orthogonality.md"
KEYWORDS_C="$ROOT_DIR/src/lexer/lexer_keywords.c"
PARSER_DIR="$ROOT_DIR/src/parser"
AXIS_COQ="$ROOT_DIR/docs/semantics/proofs/AxisOwnership.v"

for f in "$DOC42" "$KEYWORDS_C" "$AXIS_COQ"; do
    [[ -e "$f" ]] || { echo "missing required file: $f" >&2; exit 1; }
done

fail=0

# --- layer 3a: reserved keywords from the lexer table ------------------------
reserved_has() {
    grep -qE "\{\"$1\"," "$KEYWORDS_C"
}

# --- layer 3b: contextual keywords recognized in the parser ------------------
contextual_has() {
    grep -rqE "\"$1\"" "$PARSER_DIR" 2>/dev/null
}

recognized() {
    reserved_has "$1" || contextual_has "$1"
}

# --- layer 2: docs/42 section 0 axis -> keyword list -------------------------
# Each axis row's Surface cell lists the keywords in backticks. We read the row
# for one axis name and emit its backticked lowercase identifiers.
doc_axis_keywords() {
    local axis_label="$1"
    grep -E "^\| $axis_label " "$DOC42" | grep -oE '`[a-z]+`' | tr -d '`' | sort -u
}

# docs/42 axis label -> canonical short name used below.
declare -A AXIS_OF        # keyword -> docs/42 axis short name
for pair in "Resource:Resource" "Execution:Execution" "Domain:Domain" "Type/Contract:TypeContract"; do
    label="${pair%%:*}"
    short="${pair##*:}"
    while IFS= read -r kw; do
        [[ -z "$kw" ]] && continue
        AXIS_OF["$kw"]="$short"
    done < <(doc_axis_keywords "$label")
done

echo "docs/42 axis keywords parsed: ${#AXIS_OF[@]}"

# --- check A: every docs/42 axis keyword is recognized by the compiler -------
echo "== A. design (docs/42) keywords ⊆ compiler recognition =="
for kw in $(printf '%s\n' "${!AXIS_OF[@]}" | sort); do
    if reserved_has "$kw"; then
        kind="reserved"
    elif contextual_has "$kw"; then
        kind="contextual"
    else
        echo "  FAIL: docs/42 lists '$kw' (${AXIS_OF[$kw]}) but the compiler does not recognize it"
        fail=1
        continue
    fi
    printf '  ok   %-12s %-12s %s\n' "$kw" "${AXIS_OF[$kw]}" "$kind"
done

# --- check B: Coq keyword_axis mirror agrees with docs/42 --------------------
# Mirror of AxisOwnership.v section 8 keyword_axis / keyword_fact. Format:
#   "<coq constructor> <surface keyword> <axis>"
# The test also confirms each constructor still exists in the .v, so an enum
# rename here is caught instead of silently skipped.
COQ_MIRROR=(
    "KwSubject subject Domain"
    "KwIntentWho intent Domain"
    "KwZone zone Domain"
    "KwAuthority authority Domain"
    "KwEffect effect Domain"
    "KwAbility ability TypeContract"
    "KwSlot slot Resource"
    "KwParallel parallel Execution"
)

echo "== B. Coq keyword_axis (AxisOwnership.v §8) = docs/42 axis =="
for row in "${COQ_MIRROR[@]}"; do
    read -r ctor kw axis <<<"$row"
    if ! grep -qE "\b$ctor\b" "$AXIS_COQ"; then
        echo "  FAIL: Coq constructor '$ctor' not found in AxisOwnership.v (mirror is stale)"
        fail=1
        continue
    fi
    doc_axis="${AXIS_OF[$kw]:-<unclassified>}"
    if [[ "$doc_axis" != "$axis" ]]; then
        echo "  FAIL: Coq puts '$kw' on $axis but docs/42 puts it on $doc_axis"
        fail=1
        continue
    fi
    if ! recognized "$kw"; then
        echo "  FAIL: Coq-mirrored keyword '$kw' is not recognized by the compiler"
        fail=1
        continue
    fi
    printf '  ok   %-12s %-12s %s\n' "$kw" "$axis" "$ctor"
done

if [[ "$fail" -ne 0 ]]; then
    echo "axis keyword adequacy: FAILED"
    exit 1
fi

echo "axis keyword adequacy: ok (Coq §8 ⟷ docs/42 §0 ⟷ compiler keywords consistent)"
