#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMIT="${TEST_CASE_INCLUDE_MAX_LINES:-990}"

violations="$(
    cd "$ROOT_DIR"
    find src/tests -name '*.cases.h' -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$LIMIT" '$2 != "total" && $1 > limit { print }'
)"

if [[ -n "$violations" ]]; then
    echo "test case include size violations; limit is ${LIMIT} LOC:" >&2
    echo "$violations" >&2
    exit 1
fi

echo "[test-inc-size] src/tests .cases.h files <= ${LIMIT} LOC"
