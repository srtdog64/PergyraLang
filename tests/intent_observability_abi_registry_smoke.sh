#!/usr/bin/env bash
set -euo pipefail

# CLOSED fallback identities: native_literal_observability_table,
# selfhost_literal_observability_signature_table, stale_selfhost_projection.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/common/intent_observability_abi.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lib/intent_observability_abi_projection_owner.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/intent_observability_abi_registry"
PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${CC:-cc}"

mkdir -p "$BUILD_DIR"

"$PYTHON_BIN" "$ROOT_DIR/scripts/render_intent_observability_abi.py" \
    "$REGISTRY" "$PROJECTION" --check

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "scripts"))
import render_intent_observability_abi as registry

rows = registry.load_rows(root / "src/common/intent_observability_abi.def")
assert len(rows) == 51
assert [row.stable_id for row in rows] == list(range(1, 52))
assert [row.source_name for row in rows] == sorted(
    row.source_name for row in rows
)
history = next(row for row in rows if row.source_name == "IntentHistoryCount")
assert history.stable_id == 25
assert history.runtime_name == "pgy_intent_history_count_export"
assert history.arg_count == 0
assert history.return_type == "Int"
PY

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/intent_observability_abi_registry_probe.c" \
    "$ROOT_DIR/src/common/intent_observability_abi.c" \
    -o "$BUILD_DIR/intent_observability_abi_registry_probe.exe"
"$BUILD_DIR/intent_observability_abi_registry_probe.exe"

grep -Fq '#include "intent_observability_abi.def"' \
    "$ROOT_DIR/src/common/intent_observability_abi.c"
grep -Fq 'import "../lib/intent_observability_abi_projection_owner.pgy";' \
    "$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"
grep -Fq 'IntentObservabilityAbiSignatureRows()' \
    "$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"

if grep -Fq '"IntentHistoryCount"' \
        "$ROOT_DIR/src/common/intent_observability_abi.c"; then
    echo "[intent-observability-abi] native literal table returned" >&2
    exit 1
fi
if grep -Eq '"Intent[A-Za-z0-9_]*\^(Int|Bool|String)\^' \
        "$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"; then
    echo "[intent-observability-abi] self-host literal signature table returned" >&2
    exit 1
fi

echo "[intent-observability-abi] 51 native/self-host registry rows: ok"
