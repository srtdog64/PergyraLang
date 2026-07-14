#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMIT="${SEMANTIC_TU_MAX_LINES:-599}"

cd "$ROOT_DIR"

violations="$(
    find src/semantic -maxdepth 1 -type f -name '*.c' -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$LIMIT" \
            '$2 != "total" && $1 > limit { print $1, $2, ">", limit }'
)"

if [ -n "$violations" ]; then
    echo "[semantic-tu-size] semantic TU size violation(s):" >&2
    printf '%s\n' "$violations" >&2
    echo "Split by owner axis instead of moving multiple behavior families into one TU." >&2
    exit 1
fi

echo "[semantic-tu-size] semantic .c owners stay below 600 LOC"
