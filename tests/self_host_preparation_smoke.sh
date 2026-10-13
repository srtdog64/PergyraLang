#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-preparation] $*" >&2
    exit 1
}

while IFS= read -r parser_owner; do
    owner_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$parser_owner")" || fail "source-size measurement failed"
    [[ "$owner_lines" -le 600 ]] ||
        fail "${parser_owner#$ROOT_DIR/} exceeds parser owner LOC cap: $owner_lines > 600"
done < <(find "$ROOT_DIR/src/self_hosted/parser" -maxdepth 1 -name '*.pgy' -type f | sort)

echo "[self-host-preparation] hard self-host guard and soft-entry criteria ok"
