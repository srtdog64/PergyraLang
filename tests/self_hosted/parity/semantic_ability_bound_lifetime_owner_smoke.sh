#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_ability_generic_bound_verdict_owner.pgy"
NORMALIZER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"

[[ -f "$OWNER" && -f "$NORMALIZER" ]] || {
    echo "[self-host-parity:semantic-ability-bound-lifetime] owner or normalizer is missing" >&2
    exit 1
}

for fact in \
    'import "expression_normalization_owner.pgy";' \
    'SemanticTrimSourceReuse(' \
    'bounds[bound_index]'; do
    grep -Fq "$fact" "$OWNER" || {
        echo "[self-host-parity:semantic-ability-bound-lifetime] missing owner fact: $fact" >&2
        exit 1
    }
done

if grep -Fq 'StringTrim(' "$OWNER"; then
    echo "[self-host-parity:semantic-ability-bound-lifetime] trim-copy fallback remains" >&2
    exit 1
fi

grep -Fq 'func SemanticTrimSourceReuse(' "$NORMALIZER" || {
    echo "[self-host-parity:semantic-ability-bound-lifetime] source-reuse owner is missing" >&2
    exit 1
}

echo "[self-host-parity:semantic-ability-bound-lifetime] ability bounds reuse source strings"
