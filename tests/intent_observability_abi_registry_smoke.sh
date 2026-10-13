#!/usr/bin/env bash
set -euo pipefail

# CLOSED fallback identities: native_literal_observability_table,
# selfhost_literal_observability_signature_table, stable_id_from_sorted_index,
# argument_kind_from_count, parallel_scalar_observability_projection,
# stale_selfhost_projection, unknown_observability_success,
# mir_runtime_call_abi_name_reconstruction,
# native_backend_source_name_abi_lookup, emitter_source_name_abi_lookup,
# missing_carried_runtime_call_abi_success,
# valid_foreign_runtime_call_abi_success, forged_runtime_call_abi_success.

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
from tempfile import TemporaryDirectory
import bisect
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "scripts"))
import render_intent_observability_abi as registry

rows = registry.load_rows(root / "src/common/intent_observability_abi.def")

# A lexically middle append keeps every existing RuntimeCallAbiId stable.
registry_path = root / "src/common/intent_observability_abi.def"
source = registry_path.read_text(encoding="utf-8")
source_names = [row.source_name for row in rows]
insert_name = "IntentActiveContext"
insert_at = bisect.bisect_left(source_names, insert_name)
anchor = next(
    line for line in source.splitlines()
    if f'"{source_names[insert_at]}"' in line
)
inserted = (
    'PGY_INTENT_OBSERVABILITY_ABI(99, "IntentActiveContext", '
    '"pgy_intent_active_context_export", '
    'PGY_INTENT_OBSERVABILITY_PARAMS_INT, '
    'PGY_INTENT_OBSERVABILITY_RETURN_BOOL)'
)
with TemporaryDirectory() as temporary:
    path = Path(temporary) / "intent_observability_abi.def"
    path.write_text(source.replace(anchor, inserted + "\n" + anchor, 1),
                    encoding="utf-8")
    inserted_rows = registry.load_rows(path)
    assert len(inserted_rows) == len(rows) + 1
    assert next(row for row in inserted_rows
                if row.source_name == insert_name).stable_id == 99
    assert {row.source_name: row.stable_id for row in inserted_rows
            if row.source_name != insert_name} == {
                row.source_name: row.stable_id for row in rows
            }

    path.write_text(source.replace("PGY_INTENT_OBSERVABILITY_ABI(2,",
                                   "PGY_INTENT_OBSERVABILITY_ABI(1,", 1),
                    encoding="utf-8")
    try:
        registry.load_rows(path)
        raise AssertionError("duplicate stable ID was accepted")
    except ValueError as error:
        assert "duplicate stable IDs" in str(error)

    path.write_text(source.replace("PGY_INTENT_OBSERVABILITY_ABI(1,",
                                   "PGY_INTENT_OBSERVABILITY_ABI(0,", 1),
                    encoding="utf-8")
    try:
        registry.load_rows(path)
        raise AssertionError("non-positive stable ID was accepted")
    except ValueError as error:
        assert "must be positive" in str(error)
PY

projection_lines="$("$PYTHON_BIN" "$ROOT_DIR/scripts/source_size_count.py" "$PROJECTION")"
if [[ "$projection_lines" -gt 150 ]]; then
    echo "[intent-observability-abi] row projection exceeds 150 lines" >&2
    exit 1
fi

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/intent_observability_abi_registry_probe.c" \
    "$ROOT_DIR/src/common/intent_observability_abi.c" \
    -o "$BUILD_DIR/intent_observability_abi_registry_probe.exe"
"$BUILD_DIR/intent_observability_abi_registry_probe.exe"

echo "[intent-observability-abi] registry render check, loader identity negatives, projection size cap and C probe: ok"
