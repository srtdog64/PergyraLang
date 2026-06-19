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
#   A. every docs/42 axis keyword is recognized by the compiler        (2 subset 3)
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
AXIS_OF_ROWS=()        # rows are "keyword:axis"; keep bash 3.2 compatibility.
for pair in "Resource:Resource" "Execution:Execution" "Domain:Domain" "Type/Contract:TypeContract"; do
    label="${pair%%:*}"
    short="${pair##*:}"
    while IFS= read -r kw; do
        [[ -z "$kw" ]] && continue
        AXIS_OF_ROWS+=("$kw:$short")
    done < <(doc_axis_keywords "$label")
done

axis_of_keyword() {
    local needle="$1"
    local row
    local kw
    for row in "${AXIS_OF_ROWS[@]}"; do
        kw="${row%%:*}"
        if [[ "$kw" == "$needle" ]]; then
            printf '%s\n' "${row#*:}"
            return 0
        fi
    done
    return 1
}

axis_keywords() {
    local row
    for row in "${AXIS_OF_ROWS[@]}"; do
        printf '%s\n' "${row%%:*}"
    done | sort -u
}

coq_axis_name() {
    case "$1" in
        Resource) printf '%s\n' "AxResource" ;;
        Execution) printf '%s\n' "AxExecution" ;;
        Domain) printf '%s\n' "AxDomain" ;;
        TypeContract) printf '%s\n' "AxTypeContract" ;;
        *) return 1 ;;
    esac
}

echo "docs/42 axis keywords parsed: ${#AXIS_OF_ROWS[@]}"

# --- check A: every docs/42 axis keyword is recognized by the compiler -------
echo "== A. design (docs/42) keywords subset compiler recognition =="
for kw in $(axis_keywords); do
    axis="$(axis_of_keyword "$kw")"
    if reserved_has "$kw"; then
        kind="reserved"
    elif contextual_has "$kw"; then
        kind="contextual"
    else
        echo "  FAIL: docs/42 lists '$kw' ($axis) but the compiler does not recognize it"
        fail=1
        continue
    fi
    printf '  ok   %-12s %-12s %s\n' "$kw" "$axis" "$kind"
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

echo "== B. Coq keyword_axis (AxisOwnership.v section 8) = docs/42 axis =="
for row in "${COQ_MIRROR[@]}"; do
    read -r ctor kw axis <<<"$row"
    if ! grep -qE "\b$ctor\b" "$AXIS_COQ"; then
        echo "  FAIL: Coq constructor '$ctor' not found in AxisOwnership.v (mirror is stale)"
        fail=1
        continue
    fi
    doc_axis="$(axis_of_keyword "$kw" || printf '%s\n' "<unclassified>")"
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

# --- check C: intent clause -> owner checker (StepBy / write-attribution) ----
# docs/42 section 2 says each intent clause's fact has one final owner. AxisOwnership.v
# encodes those facts (FWho/FWhere/FRequires/FAuthorizedBy/FCauses) and the axis
# that owns each (Owns). This check binds that ownership to the REAL compiler:
# every clause must be parsed, modeled as a Coq Fact, owned by the matching axis
# in Owns, and routed to the semantic checker for that owner subsystem. If the
# compiler moved a clause to a different checker (silent re-attribution), or the
# Coq Owns axis no longer matches, this fails.
#
#   "<clause> <coq fact> <axis> <owner checker file> <owner token>"
CLAUSE_MAP=(
    "who        FWho          Domain       type_checker_intent_participants.c     participant"
    "within     FWhere        Domain       type_checker_intent_binding_context.c  zone"
    "requires   FRequires     TypeContract type_checker_intent_ability.c          ability"
    "authorized FAuthorizedBy Domain       type_checker_intent_authority.c        authorit"
    "causes     FCauses       Domain       type_checker_effect_decl.c             effect"
)

echo "== C. intent clause -> Coq fact/axis -> owner checker (write attribution) =="
for row in "${CLAUSE_MAP[@]}"; do
    read -r clause fact axis ofile otok <<<"$row"
    checker="$ROOT_DIR/src/semantic/$ofile"
    axis_ctor="$(coq_axis_name "$axis")"
    if ! grep -rqE "\"$clause\"" "$PARSER_DIR" 2>/dev/null; then
        echo "  FAIL: intent clause '$clause' not recognized by the parser"; fail=1; continue
    fi
    if ! grep -qE "\b$fact\b" "$AXIS_COQ"; then
        echo "  FAIL: clause '$clause' has no Coq fact '$fact' in AxisOwnership.v"; fail=1; continue
    fi
    if ! grep -qE "Owns ${axis_ctor}[[:space:]]+$fact\b" "$AXIS_COQ"; then
        echo "  FAIL: Coq Owns does not put '$fact' on ${axis_ctor} (clause '$clause')"; fail=1; continue
    fi
    if [[ ! -e "$checker" ]] || ! grep -qiE "$otok" "$checker"; then
        echo "  FAIL: clause '$clause' owner checker $ofile missing or lacks '$otok'"; fail=1; continue
    fi
    printf '  ok   %-11s %-14s %-12s %s\n' "$clause" "$fact" "$axis" "$ofile"
done

# --- check D: AIR runtime evidence kind -> axis (StepBy / runtime write attr) -
# The AIR evidence graph IS the compiler's runtime write-attribution structure:
# every evidence node (a runtime fact) must carry a non-empty provider+subject
# (air_evidence_node.c refuses anonymous appends) -- the runtime form of the Coq
# single-writer discipline (no_silent_override: a fact is never written without
# an attributed owner). This check binds the axis-bearing evidence kinds to the
# Coq fact/axis they realize, and asserts the provider-required guard survives.
#
#   "<AIR evidence kind> <coq fact> <axis> <air vocabulary name>"
AIR_H="$ROOT_DIR/src/compiler/air.h"
AIR_EVIDENCE_C="$ROOT_DIR/src/compiler/air_evidence_node.c"
AIR_VOCAB="$ROOT_DIR/src/compiler/air_vocabulary.c"
EVIDENCE_MAP=(
    "AIR_EVIDENCE_RIR_AUTHORITY          FAuthorizedBy Domain       rir_authority"
    "AIR_EVIDENCE_RIR_EFFECT_PROPAGATION FCauses       Domain       rir_effect_propagation"
    "AIR_EVIDENCE_DAG_ABILITY            FRequires     TypeContract dag_ability"
)

echo "== D. AIR evidence kind -> Coq fact/axis (runtime write attribution) =="
if ! grep -qF "requires non-empty provider and subject provenance" "$AIR_EVIDENCE_C"; then
    echo "  FAIL: AIR dropped the provider-required guard (anonymous evidence now possible)"; fail=1
else
    echo "  ok   runtime guard: evidence append requires provider+subject provenance"
fi
for row in "${EVIDENCE_MAP[@]}"; do
    read -r kind fact axis vocab <<<"$row"
    axis_ctor="$(coq_axis_name "$axis")"
    if ! grep -qE "\b$kind\b" "$AIR_H"; then
        echo "  FAIL: AIR evidence kind '$kind' not declared in air.h"; fail=1; continue
    fi
    if ! grep -qE "Owns ${axis_ctor}[[:space:]]+$fact\b" "$AXIS_COQ"; then
        echo "  FAIL: Coq Owns does not put '$fact' on ${axis_ctor} (kind '$kind')"; fail=1; continue
    fi
    if ! grep -qF -- "\"$vocab\"" "$AIR_VOCAB"; then
        echo "  FAIL: AIR vocabulary name '$vocab' missing for '$kind'"; fail=1; continue
    fi
    printf '  ok   %-35s %-14s %-12s %s\n' "$kind" "$fact" "$axis" "$vocab"
done

# --- check E: AIR append API forces attribution (Coq Append/WellAttributed) ---
# AxisOwnership.v section 10 models an Append as carrying its provider axis
# (ap_axis); append_is_stepby proves a well-attributed append is a single StepBy.
# The real append API must match structurally: every entry point names a provider
# AND a subject, so the C signature itself forbids the un-attributed write the
# Coq model rules out (the runtime guard checked in D is the second half).
echo "== E. AIR append API forces attribution (Coq Append model) =="
for fn in air_append_evidence_node air_append_evidence_node_ex; do
    sig="$(grep -A6 -E "^${fn}\(" "$AIR_EVIDENCE_C" 2>/dev/null || true)"
    if [[ -z "$sig" ]]; then
        echo "  FAIL: append entry point '$fn' not found in air_evidence_node.c"; fail=1; continue
    fi
    if ! printf '%s' "$sig" | grep -q 'provider_name' || ! printf '%s' "$sig" | grep -q 'subject_name'; then
        echo "  FAIL: '$fn' no longer requires provider_name+subject_name (anonymous append possible)"; fail=1; continue
    fi
    printf '  ok   %-30s requires provider_name + subject_name\n' "$fn"
done

if [[ "$fail" -ne 0 ]]; then
    echo "axis keyword adequacy: FAILED"
    exit 1
fi

echo "axis keyword adequacy: ok (Coq 8/5/10 -> docs/42 0/2 -> keywords + clause checkers + AIR evidence + append API)"
