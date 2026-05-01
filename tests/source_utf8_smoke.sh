#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[source-utf8] $*" >&2
    exit 1
}

if ! command -v iconv >/dev/null 2>&1; then
    fail "missing iconv"
fi

while IFS= read -r -d '' path; do
    rel="${path#"$ROOT_DIR/"}"
    iconv -f UTF-8 -t UTF-8 "$path" >/dev/null \
        || fail "$rel is not valid UTF-8"
    if LC_ALL=C grep -q $'\357\277\275' "$path"; then
        fail "$rel contains Unicode replacement characters"
    fi
done < <(find "$ROOT_DIR/src" -type f \( -name '*.c' -o -name '*.h' \) -print0)

echo "[source-utf8] src .c/.h files are valid UTF-8"
