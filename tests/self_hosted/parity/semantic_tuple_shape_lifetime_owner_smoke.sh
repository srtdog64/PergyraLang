#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/tuple_type_shape_owner.pgy"
NORMALIZER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"

[[ -f "$OWNER" && -f "$NORMALIZER" ]] || {
    echo "[self-host-parity:semantic-tuple-lifetime] owner or normalizer is missing" >&2
    exit 1
}

for fact in \
    'import "expression_normalization_owner.pgy";' \
    'SemanticTrimSourceReuse(type_name)' \
    'func SemanticTupleElementTypeFactFrom('; do
    grep -Fq "$fact" "$OWNER" || {
        echo "[self-host-parity:semantic-tuple-lifetime] missing owner fact: $fact" >&2
        exit 1
    }
done

if grep -Fq 'StringTrim(' "$OWNER"; then
    echo "[self-host-parity:semantic-tuple-lifetime] trim-copy fallback remains" >&2
    exit 1
fi

grep -Fq 'func SemanticTrimSourceReuse(' "$NORMALIZER" || {
    echo "[self-host-parity:semantic-tuple-lifetime] source-reuse owner is missing" >&2
    exit 1
}

echo "[self-host-parity:semantic-tuple-lifetime] tuple shape reuses source text"
