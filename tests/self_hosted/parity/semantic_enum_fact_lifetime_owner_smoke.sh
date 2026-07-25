#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_enum_fact_owner.pgy"
NORMALIZER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"
MIR_OWNER="$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
ASSIGNMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
BODY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"

[[ -f "$OWNER" && -f "$NORMALIZER" && -f "$MIR_OWNER" &&
    -f "$ASSIGNMENT_OWNER" && -f "$BODY_OWNER" ]] || {
    echo "[self-host-parity:semantic-enum-lifetime] owner or normalizer is missing" >&2
    exit 1
}

for fact in \
    'import "expression_normalization_owner.pgy";' \
    'SemanticTrimSourceRangeReuse(' \
    'variant, 0, paren' \
    'SemanticDelimitedRangeText(' \
    'let param_type_name: String ='; do
    grep -Fq "$fact" "$OWNER" || {
        echo "[self-host-parity:semantic-enum-lifetime] missing owner fact: $fact" >&2
        exit 1
    }
done

if grep -Fq 'StringTrim(' "$OWNER" ||
    grep -Fq 'StringTrim(Substring(' "$OWNER"; then
    echo "[self-host-parity:semantic-enum-lifetime] trim-copy fallback remains" >&2
    exit 1
fi

grep -Fq 'func SemanticTrimSourceRangeReuse(' "$NORMALIZER" || {
    echo "[self-host-parity:semantic-enum-lifetime] range-reuse owner is missing" >&2
    exit 1
}

grep -Fq 'let enum_facts: SemanticAstEnumFacts = analysis.enums;' "$MIR_OWNER" || {
    echo "[self-host-parity:semantic-enum-lifetime] MIR declaration projection lost the analysis enum owner" >&2
    exit 1
}

if grep -Fq 'SemanticAstEnumFactsFromArtifact(' "$MIR_OWNER"; then
    echo "[self-host-parity:semantic-enum-lifetime] MIR declaration projection rescans the AST for enum facts" >&2
    exit 1
fi

grep -Fq 'enum_facts: SemanticAstEnumFacts,' "$ASSIGNMENT_OWNER" || {
    echo "[self-host-parity:semantic-enum-lifetime] assignment type owner lost enum fact input" >&2
    exit 1
}

grep -Fq '!SemanticAstEnumFactsMatchArtifact(' "$ASSIGNMENT_OWNER" || {
    echo "[self-host-parity:semantic-enum-lifetime] assignment type owner lost enum fact validation" >&2
    exit 1
}

if grep -Fq 'SemanticAstEnumFactsFromArtifact(artifact)' "$ASSIGNMENT_OWNER"; then
    echo "[self-host-parity:semantic-enum-lifetime] assignment type owner rescans the AST for enum facts" >&2
    exit 1
fi

grep -Fq 'analysis.assignments, analysis.enums, function_tables' "$BODY_OWNER" || {
    echo "[self-host-parity:semantic-enum-lifetime] body bundle lost analysis enum carriage" >&2
    exit 1
}

echo "[self-host-parity:semantic-enum-lifetime] enum facts reuse owned ranges"
