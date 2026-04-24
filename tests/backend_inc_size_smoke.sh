#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMIT="${BACKEND_INC_MAX_LINES:-1000}"

violations="$(
    cd "$ROOT_DIR"
    find src/runtime src/codegen src/compiler -name '*.inc' -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$LIMIT" '$2 != "total" && $1 > limit { print }'
)"

if [[ -n "$violations" ]]; then
    echo "runtime/codegen/compiler .inc size violations; limit is ${LIMIT} LOC:" >&2
    echo "$violations" >&2
    exit 1
fi

echo "[backend-inc-size] runtime/codegen/compiler .inc files <= ${LIMIT} LOC"
