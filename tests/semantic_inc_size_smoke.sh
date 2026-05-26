#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

inc_files=()
while IFS= read -r -d '' inc_file; do
    inc_files+=("$inc_file")
done < <(
    cd "$ROOT_DIR"
    find src/semantic -name '*.inc' -type f -print0
)

if ((${#inc_files[@]} > 0)); then
    echo "semantic production .inc files are not allowed:" >&2
    printf '  %s\n' "${inc_files[@]}" >&2
    exit 1
fi

echo "[semantic-inc-size] semantic production .inc files = 0"
