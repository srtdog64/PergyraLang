#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_LIMIT="${PRODUCTION_HEADER_MAX_LINES:-600}"

cd "$ROOT_DIR"

violations="$(
    find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp \
        -name '*.h' -type f -print0 \
    | awk -v RS='\0' -v limit="$DEFAULT_LIMIT" '
        length($0) == 0 { next }
        {
            n = 0
            while ((getline line < $0) > 0)
                n++
            close($0)
            if (n > limit)
                printf "%d %s > %d\n", n, $0, limit
        }
    '
)"

if [ -n "$violations" ]; then
    echo "[production-header-size] header owner size violation(s):" >&2
    printf '%s' "$violations" >&2
    echo "Split by feature owner instead of growing behavior-heavy headers." >&2
    exit 1
fi

echo "[production-header-size] production headers stay within owner-size caps"
