#!/usr/bin/env bash
#
# Binary adequacy for the axis fact-ownership proof
# (docs/semantics/proofs/AxisOwnership.v section 8).
#
# AxisOwnership.v proves, INSIDE Coq, that the keyword->axis table is
# consistent with the fact-ownership relation (keyword_axis_sound). That
# theorem constrains eight representative semantic USES in the *model*, not
# all 147 primary-category labels. Source consistency is a bounded binding
# check, not a verified extraction or whole-language refinement proof.
#
#   (1) Coq    actual keyword_axis arms (AxisOwnership.v section 8)
#   (2) Design docs/42 semantic examples and primary-category assertions
#   (3) Impl   language words declared by LanguageKeywordRegistry; the parser
#              still owns where contextual/soft rows are grammatically valid
#
# Checks:
#   A/B. actual registry category values, native enum/generator identities,
#        docs projections and actual representative Rocq arms agree.
#        Mutation controls reject old misassignments, missing inputs and
#        changed model values. No literal axis mirror is accepted.
#
# This is a pure source-consistency test (no coqc); it complements the Coq
# proof rather than re-checking it.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC42="$ROOT_DIR/docs/42_keyword_orthogonality.md"
KEYWORD_REGISTRY="$ROOT_DIR/src/lexer/language_keyword_registry.def"
AXIS_COQ="$ROOT_DIR/docs/semantics/proofs/AxisOwnership.v"

for f in "$DOC42" "$KEYWORD_REGISTRY" "$AXIS_COQ"; do
    [[ -e "$f" ]] || { echo "missing required file: $f" >&2; exit 1; }
done

fail=0

# Contextual/soft membership is still required by the C-F clause checks.
contextual_has() {
    grep -qE "^[[:space:]]*\"$1\",[[:space:]]+PGY_KEYWORD_CLASS_(CONTEXTUAL|SOFT)," \
        "$KEYWORD_REGISTRY"
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

# --- checks A/B: actual category values and modeled uses, no literal mirror --
PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "axis category consistency requires python3/python" >&2
        exit 1
    fi
fi
echo "== A/B. Coq keyword_axis (AxisOwnership.v section 8) = docs/42 axis; actual registry categories =="
PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B \
    "$ROOT_DIR/tests/language_keyword_axis_contract.py" "$ROOT_DIR" --selftest

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
    if ! contextual_has "$clause"; then
        echo "  FAIL: intent clause '$clause' is absent from the language-word registry"; fail=1; continue
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
# provenance and a typed provider/subject class. That typed class is not full
# proof-object extraction; it is the implementation boundary that prevents AIR
# evidence from being accepted as anonymous string-only facts.
#
#   "<AIR evidence kind> <coq fact> <axis> <air vocabulary name> <provider kind> <subject kind>"
AIR_H="$ROOT_DIR/src/compiler/air.h"
AIR_EVIDENCE_C="$ROOT_DIR/src/compiler/air_evidence_node.c"
AIR_VOCAB="$ROOT_DIR/src/compiler/air_vocabulary.c"
EVIDENCE_MAP=(
    "AIR_EVIDENCE_RIR_AUTHORITY          FAuthorizedBy Domain       rir_authority          AIR_EVIDENCE_PROVIDER_RIR AIR_EVIDENCE_SUBJECT_AUTHORITY"
    "AIR_EVIDENCE_RIR_EFFECT_PROPAGATION FCauses       Domain       rir_effect_propagation AIR_EVIDENCE_PROVIDER_RIR AIR_EVIDENCE_SUBJECT_EFFECT_PROPAGATION"
    "AIR_EVIDENCE_DAG_ABILITY            FRequires     TypeContract dag_ability            AIR_EVIDENCE_PROVIDER_DAG AIR_EVIDENCE_SUBJECT_ABILITY"
)

echo "== D. AIR evidence kind -> Coq fact/axis (runtime write attribution) =="
if ! grep -qF "requires non-empty provider and subject provenance" "$AIR_EVIDENCE_C"; then
    echo "  FAIL: AIR dropped the provider-required guard (anonymous evidence now possible)"; fail=1
else
    echo "  ok   runtime guard: evidence append requires provider+subject provenance"
fi
if ! grep -qF "air_evidence_node_has_declared_kind_facts" "$AIR_EVIDENCE_C"; then
    echo "  FAIL: AIR dropped typed evidence class validation"; fail=1
else
    echo "  ok   runtime guard: evidence node validates typed provider/subject class"
fi
for row in "${EVIDENCE_MAP[@]}"; do
    read -r kind fact axis vocab provider_kind subject_kind <<<"$row"
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
    if ! grep -qF "$provider_kind" "$AIR_EVIDENCE_C" || ! grep -qF "$subject_kind" "$AIR_EVIDENCE_C"; then
        echo "  FAIL: AIR evidence kind '$kind' is not bound to typed provider/subject class"; fail=1; continue
    fi
    printf '  ok   %-35s %-14s %-12s %s %s/%s\n' "$kind" "$fact" "$axis" "$vocab" "$provider_kind" "$subject_kind"
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

# --- check F: strict evidence is the DEFAULT (relaxed mode is opt-in) ---------
# PGY_AIR_STRICT_EVIDENCE=0 relaxes evidence checking for build smoke-tests; a
# relaxed binary is NOT domain-safety-checked (see AxisOwnership.md §7.5). This
# guards the safety-critical default: air_strict_evidence_enabled() must return
# true when the env var is unset/empty, so nobody can silently flip the default
# to relaxed without this gate failing.
AIR_C="$ROOT_DIR/src/compiler/air.c"
echo "== F. strict-evidence default (relaxed is opt-in, not the default) =="
body="$(awk '/air_strict_evidence_enabled\(void\)/{f=1} f{print} /^}/{if(f)exit}' "$AIR_C" 2>/dev/null || true)"
if [[ -z "$body" ]]; then
    echo "  FAIL: air_strict_evidence_enabled() not found in air.c"; fail=1
elif ! printf '%s' "$body" | grep -q 'value == NULL' || ! printf '%s' "$body" | grep -q 'return true'; then
    echo "  FAIL: strict evidence is no longer the default (unset env must return true)"; fail=1
else
    echo "  ok   air_strict_evidence_enabled() defaults to true when env unset"
fi

if [[ "$fail" -ne 0 ]]; then
    echo "axis keyword adequacy: FAILED"
    exit 1
fi

echo "axis keyword adequacy: ok (147 category rows + actual representative model -> docs/42; clauses + AIR evidence + append API + strict-default; not whole-language proof)"
