#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMIT="${SEMANTIC_INC_MAX_LINES:-800}"

violations="$(
    cd "$ROOT_DIR"
    find src/semantic -name '*.inc' -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$LIMIT" '$2 != "total" && $1 > limit { print }'
)"

if [[ -n "$violations" ]]; then
    echo "semantic .inc size violations; limit is ${LIMIT} LOC:" >&2
    echo "$violations" >&2
    exit 1
fi

echo "[semantic-inc-size] all semantic .inc files <= ${LIMIT} LOC"
