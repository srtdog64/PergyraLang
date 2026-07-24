#!/usr/bin/env bash
# Owns the no-copy trim boundary used by graph expression validation.
# Forbidden fallback: semantic_validation_trim_copy.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/expr_validation_owner.pgy"
NORMALIZER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"

[[ -f "$OWNER" && -f "$NORMALIZER" ]] || {
    echo "[self-host-parity:semantic-validation] owner is missing" >&2
    exit 1
}

grep -Fq 'SemanticTrimSourceReuse(' "$OWNER" || {
    echo "[self-host-parity:semantic-validation] trim reuse consumer is missing" >&2
    exit 1
}
grep -Fq 'func SemanticTrimSourceReuse' "$NORMALIZER" || {
    echo "[self-host-parity:semantic-validation] trim reuse owner is missing" >&2
    exit 1
}

if grep -Fq 'Trim(' "$OWNER"; then
    echo "[self-host-parity:semantic-validation] allocation-returning trim fallback remains" >&2
    exit 1
fi

echo "[self-host-parity:semantic-validation] graph validation reuses normalized source"
