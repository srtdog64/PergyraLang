#!/usr/bin/env bash
# Canonical registry -> live authority-edge gate. The registry owns the rows;
# this wrapper owns no copied owner list, status count, or fallback inventory.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python >/dev/null 2>&1 \
        && python -c 'import sys; raise SystemExit(sys.version_info < (3, 9))'; then
        PYTHON_BIN="$(command -v python)"
    elif command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    else
        echo "[sot-authority-edge] Python is required" >&2
        exit 1
    fi
fi

REGISTRY="$ROOT_DIR/docs/semantics/sot_owner_spine_registry.md"
"$PYTHON_BIN" "$ROOT_DIR/scripts/sot_registry_gate.py" "$ROOT_DIR"

mkdir -p "$ROOT_DIR/.tmp"
MUTATED_REGISTRY="$(mktemp "$ROOT_DIR/.tmp/sot-owner-cap.XXXXXX.md")"
CAP_STDERR="$(mktemp "$ROOT_DIR/.tmp/sot-owner-cap.XXXXXX.err")"
trap 'rm -f "$MUTATED_REGISTRY" "$CAP_STDERR"' EXIT

"$PYTHON_BIN" - "$REGISTRY" "$MUTATED_REGISTRY" <<'PY'
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
begin_marker = "<!-- BEGIN sot-owner-spine-registry -->"
marker = "<!-- END sot-owner-spine-registry -->"
before, after = source.split(marker, 1)
owner_block = before.split(begin_marker, 1)[1]
rows = [line for line in owner_block.splitlines()
        if line.strip() and not line.lstrip().startswith("```")]
if len(rows) != 95:
    raise SystemExit(f"expected the canonical 95 owner rows, found {len(rows)}")
duplicate = rows[0]
pathlib.Path(sys.argv[2]).write_text(
    before + duplicate + "\n" + marker + after,
    encoding="utf-8",
    newline="\n",
)
PY

if "$PYTHON_BIN" "$ROOT_DIR/scripts/sot_registry_gate.py" "$ROOT_DIR" \
        --registry "$MUTATED_REGISTRY" 2>"$CAP_STDERR"; then
    echo "[sot-authority-edge] 96th owner row was accepted" >&2
    exit 1
fi
grep -Fq 'owner registry exceeds cap: 96 > 95' "$CAP_STDERR" || {
    cat "$CAP_STDERR" >&2
    echo "[sot-authority-edge] owner cap did not fail before duplicate checks" >&2
    exit 1
}
echo "[sot-authority-edge] 95-row cap and 96-row refusal: PASS"
