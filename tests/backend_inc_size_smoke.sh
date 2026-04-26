#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mapfile -d '' inc_files < <(
    cd "$ROOT_DIR"
    find src/runtime src/codegen src/compiler -name '*.inc' -type f -print0
)

if ((${#inc_files[@]} > 0)); then
    echo "runtime/codegen/compiler production .inc files are not allowed:" >&2
    printf '  %s\n' "${inc_files[@]}" >&2
    exit 1
fi

echo "[backend-inc-size] runtime/codegen/compiler production .inc files = 0"
