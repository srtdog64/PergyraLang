#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

inc_files=()
while IFS= read -r -d '' inc_file; do
    inc_files+=("$inc_file")
done < <(
    cd "$ROOT_DIR"
    find src/runtime src/codegen src/compiler -name '*.inc' -type f -print0
)

if ((${#inc_files[@]} > 0)); then
    echo "runtime/codegen/compiler production .inc files are not allowed:" >&2
    printf '  %s\n' "${inc_files[@]}" >&2
    exit 1
fi

large_impl_headers="$(
    cd "$ROOT_DIR"
    find src -name '*.h' -type f \
        ! -path 'src/tests/*' \
        -print0 | python3 "$ROOT_DIR/scripts/source_size_count.py" --paths0 --rows \
        | awk '$2 != "total" && $1 > 600 { print }'
)"
if [ -n "$large_impl_headers" ]; then
    echo "production implementation-style headers exceeded the 600 LOC signal:" >&2
    printf '%s\n' "$large_impl_headers" >&2
    exit 1
fi

echo "[backend-inc-size] runtime/codegen/compiler production .inc files = 0; src production headers <= 600 LOC"
