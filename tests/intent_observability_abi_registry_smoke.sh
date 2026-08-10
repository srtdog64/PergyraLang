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
from tempfile import TemporaryDirectory
import bisect
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "scripts"))
import render_intent_observability_abi as registry

rows = registry.load_rows(root / "src/common/intent_observability_abi.def")
assert len(rows) == 51
stable_ids = [row.stable_id for row in rows]
assert all(stable_id > 0 for stable_id in stable_ids)
assert len(stable_ids) == len(set(stable_ids))
assert [row.source_name for row in rows] == sorted(
    row.source_name for row in rows
)
history = next(row for row in rows if row.source_name == "IntentHistoryCount")
assert history.stable_id == 25
assert history.runtime_name == "pgy_intent_history_count_export"
assert history.parameter_shape == "PGY_INTENT_OBSERVABILITY_PARAMS_NONE"
assert history.argument_count == 0
assert history.parameter_types == ""
assert history.return_type == "Int"

active = next(row for row in rows if row.source_name == "IntentActiveConcurrent")
step = next(row for row in rows if row.source_name == "IntentActiveStepFailure")
assert active.parameter_shape == "PGY_INTENT_OBSERVABILITY_PARAMS_INT"
assert active.argument_count == 1 and active.parameter_types == "Int"
assert step.parameter_shape == "PGY_INTENT_OBSERVABILITY_PARAMS_INT_INT"
assert step.argument_count == 2 and step.parameter_types == "Int|Int"

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
    assert len(inserted_rows) == 52
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

projection = (root / "src/self_hosted/lib/"
              "intent_observability_abi_projection_owner.pgy").read_text(
                  encoding="utf-8")
assert projection.count("if index ==") == 51
assert "struct IntentObservabilityAbiRow" in projection
assert "IntentObservabilityAbiRowAt(-1).valid" in projection
assert "IntentObservabilityAbiRowAt(IntentObservabilityAbiCount()).valid" in projection
assert "runtime_call_abi_id != i + 1" not in projection
for retired in (
    "IntentObservabilityAbiStableIdAt",
    "IntentObservabilityAbiSourceNameAt",
    "IntentObservabilityAbiRuntimeNameAt",
    "IntentObservabilityAbiArgumentCountAt",
    "IntentObservabilityAbiReturnTypeAt",
    "IntentObservabilityAbiParameterTypesAt",
):
    assert retired not in projection
PY

if [[ "$(wc -l < "$PROJECTION" | tr -d ' ')" -gt 140 ]]; then
    echo "[intent-observability-abi] row projection exceeds 140 lines" >&2
    exit 1
fi
if grep -Fq 'list(range(1, len(rows) + 1))' \
        "$ROOT_DIR/scripts/render_intent_observability_abi.py"; then
    echo "[intent-observability-abi] sorted row position regained ABI identity authority" >&2
    exit 1
fi

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

echo "[intent-observability-abi] 51 native/self-host registry rows plus non-positional identity and parameter-shape negatives: ok"
