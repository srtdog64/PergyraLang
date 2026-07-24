#!/usr/bin/env bash
# Owns the allocation-free character-view contract for semantic normalization.
# Forbidden fallbacks: semantic_normalization_char_at, semantic_normalization_trim_copy.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:semantic-normalization] owner is missing" >&2
    exit 1
}

grep -Fq 'func SemanticStripOuterParens' "$OWNER" || {
    echo "[self-host-parity:semantic-normalization] normalization owner is missing" >&2
    exit 1
}
grep -Fq 'SourceByteAt(' "$OWNER" || {
    echo "[self-host-parity:semantic-normalization] byte-view owner is missing" >&2
    exit 1
}
grep -Fq 'SourceByteIsWhitespace(' "$OWNER" || {
    echo "[self-host-parity:semantic-normalization] whitespace byte owner is missing" >&2
    exit 1
}
grep -Fq 'if start != 0 || end != source_length' "$OWNER" || {
    echo "[self-host-parity:semantic-normalization] source-string reuse guard is missing" >&2
    exit 1
}

if grep -Fq 'SourceCharAt(' "$OWNER" ||
    grep -Fq 'CharAtN(' "$OWNER" ||
    grep -Fq 'Trim(expr)' "$OWNER"; then
    echo "[self-host-parity:semantic-normalization] allocation-returning character/trim fallback remains" >&2
    exit 1
fi

echo "[self-host-parity:semantic-normalization] byte-view normalization and source reuse are wired"
