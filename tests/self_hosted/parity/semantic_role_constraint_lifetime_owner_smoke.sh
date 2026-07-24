#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_role_fact_owner.pgy"
NORMALIZER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"

[[ -f "$OWNER" && -f "$NORMALIZER" ]] || {
    echo "[self-host-parity:semantic-role-lifetime] owner or normalizer is missing" >&2
    exit 1
}

for fact in \
    'import "expression_normalization_owner.pgy";' \
    'SemanticTrimSourceRangeReuse(' \
    'row, 6, StringLength(row)' \
    'part, 0, colon' \
    'part, colon + 1, StringLength(part)'; do
    grep -Fq "$fact" "$OWNER" || {
        echo "[self-host-parity:semantic-role-lifetime] missing owner fact: $fact" >&2
        exit 1
    }
done

if grep -Fq 'StringTrim(' "$OWNER" ||
    grep -Fq 'StringTrim(Substring(' "$OWNER"; then
    echo "[self-host-parity:semantic-role-lifetime] trim-copy fallback remains" >&2
    exit 1
fi

grep -Fq 'func SemanticTrimSourceRangeReuse(' "$NORMALIZER" || {
    echo "[self-host-parity:semantic-role-lifetime] range-reuse owner is missing" >&2
    exit 1
}

echo "[self-host-parity:semantic-role-lifetime] role constraints reuse source ranges"
