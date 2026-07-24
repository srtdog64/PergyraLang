#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_enum_fact_owner.pgy"
NORMALIZER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"

[[ -f "$OWNER" && -f "$NORMALIZER" ]] || {
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

echo "[self-host-parity:semantic-enum-lifetime] enum facts reuse owned ranges"
