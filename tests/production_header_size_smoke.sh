#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_LIMIT="${PRODUCTION_HEADER_MAX_LINES:-600}"

cd "$ROOT_DIR"

violations="$(
    while IFS= read -r -d '' file; do
        lines="$(wc -l < "$file")"
        lines="${lines//[[:space:]]/}"
        if [ "$lines" -gt "$DEFAULT_LIMIT" ]; then
            printf "%d %s > %d\n" "$lines" "$file" "$DEFAULT_LIMIT"
        fi
    done < <(find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp \
        -name '*.h' -type f -print0)
)"

if [ -n "$violations" ]; then
    echo "[production-header-size] header owner size violation(s):" >&2
    printf '%s' "$violations" >&2
    echo "Split by feature owner instead of growing behavior-heavy headers." >&2
    exit 1
fi

echo "[production-header-size] production headers stay within owner-size caps"
