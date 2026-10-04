#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_LIMIT="${PRODUCTION_HEADER_MAX_LINES:-600}"

cd "$ROOT_DIR"

violations="$(
    find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp \
        -name '*.h' -type f -print0 \
        | python3 "$ROOT_DIR/scripts/source_size_count.py" --paths0 --rows \
        | awk -v limit="$DEFAULT_LIMIT" '$1 > limit { print $1, $2, ">", limit }'
)"

if [ -n "$violations" ]; then
    echo "[production-header-size] header owner size violation(s):" >&2
    printf '%s' "$violations" >&2
    echo "Split by feature owner instead of growing behavior-heavy headers." >&2
    exit 1
fi

echo "[production-header-size] production headers stay within owner-size caps"
