#!/usr/bin/env bash
# The self-hosted MIR reader must consume machine-layer fact rows through the
# checked self-host projection and reject a mutated manifest/runtime mapping.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir_lower/machine_layer_fact_owner.pgy"
INPUT="$ROOT_DIR/src/self_hosted/mir_lower/mir_json_input_owner.pgy"
MACHINE_OWNER="$ROOT_DIR/src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy"
MACHINE_BINDING_OWNER="$ROOT_DIR/src/self_hosted/compiler/machine_layer_runtime_binding_owner.pgy"
DECLARATION_CONSUMER="$ROOT_DIR/src/self_hosted/compiler/machine_layer_declaration_consumer.pgy"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-mir-machine-layer] missing compiler binary: $PGY" >&2
    exit 1
fi

grep -Fq -- 'CompilerRuntimeCallAbiMachineLayerManifestId' "$MACHINE_OWNER"
grep -Fq -- 'MirMachineLayerFactsReady' "$INPUT"
grep -Fq -- 'MirMachineLayerObjectReady' "$OWNER"
grep -Fq -- 'machine_contact_kind' "$OWNER"
grep -Fq -- 'physical_grant' "$OWNER"
grep -Fq -- 'physical_base' "$OWNER"
grep -Fq -- 'physical_size' "$OWNER"
grep -Fq -- 'physical_mode' "$OWNER"
grep -Fq -- 'SelfHostMachineLayerDeclarationFromPath' "$DECLARATION_CONSUMER"
grep -Fq -- 'CompilerMachineLayerRuntimeBindingBlock' "$MACHINE_BINDING_OWNER"
grep -Fq -- 'CompilerMachineLayerRuntimeBindingStatement' "$MACHINE_BINDING_OWNER"
RIR_VALIDATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/machine_layer_rir_validator/main.pgy"
grep -Fq -- 'machine_contact' "$RIR_VALIDATOR_SOURCE"
grep -Fq -- 'CompilerRuntimeCallAbiMachineLayerContactNameAt' "$RIR_VALIDATOR_SOURCE"
JSON_PROJECTION_OWNER="$ROOT_DIR/src/self_hosted/mir/json_projection_owner.pgy"
PROJECTION_PROBE_SOURCE="$ROOT_DIR/src/self_hosted/tools/machine_layer_mir_projection_probe/main.pgy"
grep -Fq -- 'machine_contact_kind' "$JSON_PROJECTION_OWNER"
grep -Fq -- 'physical_base' "$JSON_PROJECTION_OWNER"
if grep -Eq -- 'AST_|ReadFile\(' "$OWNER"; then
    echo "[self-host-mir-machine-layer] machine fact owner reached a source fallback" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_machine_layer}"
mkdir -p "$BUILD_DIR"
LOWER_BIN="$BUILD_DIR/mir_lower.exe"
MIR_JSON="$BUILD_DIR/device_slot.mirjson"
AST_OUT="$BUILD_DIR/device_slot.reast"
BAD_JSON="$BUILD_DIR/device_slot.bad.mirjson"
BAD_PHYSICAL_JSON="$BUILD_DIR/device_slot.bad-physical.mirjson"
BAD_PHYSICAL_SHAPE_JSON="$BUILD_DIR/device_slot.bad-physical-shape.mirjson"
BAD_OUT="$BUILD_DIR/device_slot.bad.out"
BAD_MISSING_JSON="$BUILD_DIR/device_slot.missing-fact.mirjson"
BAD_MISSING_OUT="$BUILD_DIR/device_slot.missing-fact.out"
LOWER_LOG="$BUILD_DIR/mir_lower.compile.log"
DRIVER_BIN="$BUILD_DIR/driver_rung2.exe"
DRIVER_LOG="$BUILD_DIR/driver_rung2.compile.log"
SOURCE_MACHINE_MIR="$BUILD_DIR/device_slot.source.mirjson"
SOURCE_REMOTE_MIR="$BUILD_DIR/device_slot.remote.source.mirjson"
SOURCE_MACHINE_OUT="$BUILD_DIR/device_slot.source.out"
SOURCE_REMOTE_OUT="$BUILD_DIR/device_slot.remote.source.out"
AIR_VALIDATOR_BIN="$BUILD_DIR/machine_layer_air_validator.exe"
AIR_VALIDATOR_SOURCE="$ROOT_DIR/src/self_hosted/tools/machine_layer_air_validator/main.pgy"
RIR_VALIDATOR_BIN="$BUILD_DIR/machine_layer_rir_validator.exe"
RIR_JSON="$BUILD_DIR/device_slot.rirjson"
RIR_BAD_JSON="$BUILD_DIR/device_slot.bad.rirjson"
RIR_OUT="$BUILD_DIR/device_slot.rir.out"
RIR_BAD_OUT="$BUILD_DIR/device_slot.bad.rir.out"
RIR_LOG="$BUILD_DIR/machine_layer_rir_validator.compile.log"
AIR_RAW="$BUILD_DIR/device_slot.airjson.utf16"
AIR_JSON="$BUILD_DIR/device_slot.airjson"
AIR_BAD_JSON="$BUILD_DIR/device_slot.bad.airjson"
AIR_OUT="$BUILD_DIR/device_slot.air.out"
AIR_BAD_OUT="$BUILD_DIR/device_slot.bad.air.out"
AIR_LOG="$BUILD_DIR/machine_layer_air_validator.compile.log"
PROBE_BIN="$BUILD_DIR/machine_layer_mir_projection_probe.exe"
PROBE_LOG="$BUILD_DIR/machine_layer_mir_projection_probe.compile.log"
PROBE_JSON="$BUILD_DIR/machine_layer_mir_projection_probe.mirjson"
MACHINE_MANIFEST_JSON="$BUILD_DIR/machine-layer-declaration.json"
BAD_MACHINE_MANIFEST_JSON="$BUILD_DIR/machine-layer-declaration.bad.json"
BAD_MACHINE_MANIFEST_OUT="$BUILD_DIR/machine-layer-declaration.bad.out"
BAD_GRANT_MACHINE_MANIFEST_JSON="$BUILD_DIR/machine-layer-declaration.bad-grant.json"
BAD_GRANT_MACHINE_MANIFEST_OUT="$BUILD_DIR/machine-layer-declaration.bad-grant.out"
BAD_PROVENANCE_MACHINE_MANIFEST_JSON="$BUILD_DIR/machine-layer-declaration.bad-provenance.json"
BAD_PROVENANCE_MACHINE_MANIFEST_OUT="$BUILD_DIR/machine-layer-declaration.bad-provenance.out"

(cd "$ROOT_DIR" && "$PGY" --machine-manifest-json >"$MACHINE_MANIFEST_JSON")
MACHINE_MANIFEST_REL=".tmp/self_hosted/mir_machine_layer/machine-layer-declaration.json"

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "src/self_hosted/mir_lower/main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$LOWER_BIN")" \
    >"$LOWER_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    echo "[self-host-mir-machine-layer] mir_lower rebuild failed" >&2
    cat "$LOWER_LOG" >&2
    exit 1
fi

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "src/self_hosted/compiler/driver_rung2_main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER_BIN")" \
    >"$DRIVER_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    echo "[self-host-mir-machine-layer] source-to-MIR driver rebuild failed" >&2
    cat "$DRIVER_LOG" >&2
    exit 1
fi

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "${PROJECTION_PROBE_SOURCE#$ROOT_DIR/}")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PROBE_BIN")" \
    >"$PROBE_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    echo "[self-host-mir-machine-layer] self-host MIR producer probe rebuild failed" >&2
    cat "$PROBE_LOG" >&2
    exit 1
fi
if ! (cd "$ROOT_DIR" && "$PROBE_BIN" "$MACHINE_MANIFEST_REL" >"$PROBE_JSON" 2>&1); then
    echo "[self-host-mir-machine-layer] self-host MIR producer rejected machine graph" >&2
    cat "$PROBE_JSON" >&2
    exit 1
fi
grep -Fq -- '"schema":"pgy.mir.v1"' "$PROBE_JSON"
for contact in claim read write release submit-read; do
    grep -Fq -- "\"machine_contact_kind\":\"$contact\"" "$PROBE_JSON" || {
        echo "[self-host-mir-machine-layer] self-host producer lost contact row: $contact" >&2
        exit 1
    }
done
sed '0,/"runtime_operation":"Claim"/s//"runtime_operation":"BadClaim"/' \
    "$MACHINE_MANIFEST_JSON" >"$BAD_MACHINE_MANIFEST_JSON"
BAD_MACHINE_MANIFEST_REL=".tmp/self_hosted/mir_machine_layer/machine-layer-declaration.bad.json"
if (cd "$ROOT_DIR" && "$PROBE_BIN" "$BAD_MACHINE_MANIFEST_REL" \
    >"$BAD_MACHINE_MANIFEST_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mutated native declaration was accepted" >&2
    exit 1
fi
grep -Fq -- 'machine probe facts rejected' "$BAD_MACHINE_MANIFEST_OUT"
sed '0,/"device_grant":"device-slot0"/s//"device_grant":"pergyra.invalid-grant"/' \
    "$MACHINE_MANIFEST_JSON" >"$BAD_GRANT_MACHINE_MANIFEST_JSON"
BAD_GRANT_MACHINE_MANIFEST_REL=".tmp/self_hosted/mir_machine_layer/machine-layer-declaration.bad-grant.json"
if (cd "$ROOT_DIR" && "$PROBE_BIN" "$BAD_GRANT_MACHINE_MANIFEST_REL" \
    >"$BAD_GRANT_MACHINE_MANIFEST_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mismatched physical device grant was accepted" >&2
    exit 1
fi
grep -Fq -- 'machine probe facts rejected' "$BAD_GRANT_MACHINE_MANIFEST_OUT"
sed '0,/"id":"pergyra.machine-declaration.host-sim.v1"/s//"id":"pergyra.invalid-target.v1"/' \
    "$MACHINE_MANIFEST_JSON" >"$BAD_PROVENANCE_MACHINE_MANIFEST_JSON"
BAD_PROVENANCE_MACHINE_MANIFEST_REL=".tmp/self_hosted/mir_machine_layer/machine-layer-declaration.bad-provenance.json"
if (cd "$ROOT_DIR" && "$PROBE_BIN" "$BAD_PROVENANCE_MACHINE_MANIFEST_REL" \
    >"$BAD_PROVENANCE_MACHINE_MANIFEST_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] invalid physical declaration namespace was accepted" >&2
    exit 1
fi
grep -Fq -- 'machine probe facts rejected' "$BAD_PROVENANCE_MACHINE_MANIFEST_OUT"
grep -Fq -- '"physical_base":268435456' "$PROBE_JSON"
grep -Fq -- '"physical_size":4096' "$PROBE_JSON"
grep -Fq -- '"physical_mode":"volatile"' "$PROBE_JSON"
PROBE_BAD_TARGET_OUT="$BUILD_DIR/machine_layer_mir_projection_probe.bad-target.out"
if ! (cd "$ROOT_DIR" && "$PROBE_BIN" --missing-call-target >"$PROBE_BAD_TARGET_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] missing call-target negative probe failed to run" >&2
    cat "$PROBE_BAD_TARGET_OUT" >&2
    exit 1
fi
grep -Fq -- 'machine probe rejected missing call-target fact' "$PROBE_BAD_TARGET_OUT"
PROBE_REL="${PROBE_JSON#$ROOT_DIR/}"
PROBE_AST_OUT="$BUILD_DIR/machine_layer_mir_projection_probe.reast"
if ! (cd "$ROOT_DIR" && "$LOWER_BIN" "$PROBE_REL" "$MACHINE_MANIFEST_REL" >"$PROBE_AST_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] self-host producer JSON did not reach MIR consumer" >&2
    cat "$PROBE_AST_OUT" >&2
    exit 1
fi
grep -Fq -- 'DeviceSlot<Int>' "$PROBE_AST_OUT"

SOURCE_MACHINE_CASE="tests/cases/backend_compare/device_slot_machine_layer/main.pgy"
SOURCE_REMOTE_CASE="tests/cases/backend_compare/device_slot_remote/main.pgy"
for source_case in "$SOURCE_MACHINE_CASE" "$SOURCE_REMOTE_CASE"; do
    if [[ "$source_case" == "$SOURCE_MACHINE_CASE" ]]; then
        source_mir="$SOURCE_MACHINE_MIR"
        source_out="$SOURCE_MACHINE_OUT"
        expected_contacts=(claim read write release)
    else
        source_mir="$SOURCE_REMOTE_MIR"
        source_out="$SOURCE_REMOTE_OUT"
        expected_contacts=(claim write submit-read release)
    fi
    if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
        "$source_case" "$MACHINE_MANIFEST_REL" >"$source_mir" 2>&1); then
        echo "[self-host-mir-machine-layer] source-to-MIR machine fixture failed: $source_case" >&2
        cat "$source_mir" >&2
        exit 1
    fi
    grep -Fq -- '"schema":"pgy.mir.v1"' "$source_mir"
    for contact in "${expected_contacts[@]}"; do
        grep -Fq -- "\"machine_contact_kind\":\"$contact\"" "$source_mir" || {
            echo "[self-host-mir-machine-layer] source producer lost contact row: $source_case/$contact" >&2
            exit 1
        }
    done
    grep -Fq -- '"physical_base":268435456' "$source_mir"
    SOURCE_MIR_REL="${source_mir#$ROOT_DIR/}"
    if ! (cd "$ROOT_DIR" && "$LOWER_BIN" "$SOURCE_MIR_REL" "$MACHINE_MANIFEST_REL" >"$source_out" 2>&1); then
        echo "[self-host-mir-machine-layer] source-produced MIR did not reach self-host lowering: $source_case" >&2
        cat "$source_out" >&2
        exit 1
    fi
    grep -Fq -- 'DeviceSlot<Int>' "$source_out"
done

# The declaration/MIR path is not enough: the self-host C emitter must carry
# the same typed DeviceSlot owner through the final C runtime call boundary.
# Cover both the synchronous handle path and the RemoteFuture/await result
# bridge; both must consume the same declaration and machine runtime rows.
SELFHOST_MACHINE_C="$BUILD_DIR/device_slot.selfhost.c"
if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" "$SOURCE_MACHINE_CASE" \
    --machine-manifest-json "$MACHINE_MANIFEST_REL" >"$SELFHOST_MACHINE_C" 2>"$BUILD_DIR/device_slot.selfhost.err"); then
    echo "[self-host-mir-machine-layer] self-host DeviceSlot C emission failed" >&2
    cat "$BUILD_DIR/device_slot.selfhost.err" >&2
    exit 1
fi
grep -Fq -- '#include "pgy_runtime.h"' "$SELFHOST_MACHINE_C"
grep -Fq -- 'PgyDeviceSlot_Int dev = pgy_claim_device_Int();' "$SELFHOST_MACHINE_C"
grep -Fq -- 'pgy_device_read_Int(&dev)' "$SELFHOST_MACHINE_C"
grep -Fq -- 'pgy_device_write_Int(&dev' "$SELFHOST_MACHINE_C"
grep -Fq -- 'pgy_release_device_Int(&dev)' "$SELFHOST_MACHINE_C"
grep -Fq -- 'pgy_machine_layer_runtime_bind_mapping_export' "$SELFHOST_MACHINE_C"
grep -Fq -- 'pgy_machine_layer_require_mapping();' "$SELFHOST_MACHINE_C"
grep -Fq -- 'machine-layer runtime bind rejected' "$SELFHOST_MACHINE_C"
if command -v gcc >/dev/null 2>&1; then
    gcc -std=c11 -Wall -Wextra -Isrc -Isrc/runtime -fsyntax-only \
        "$SELFHOST_MACHINE_C"
fi

SELFHOST_REMOTE_C="$BUILD_DIR/device_slot.remote.selfhost.c"
if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" "$SOURCE_REMOTE_CASE" \
    --machine-manifest-json "$MACHINE_MANIFEST_REL" >"$SELFHOST_REMOTE_C" 2>"$BUILD_DIR/device_slot.remote.selfhost.err"); then
    echo "[self-host-mir-machine-layer] self-host RemoteFuture C emission failed" >&2
    cat "$BUILD_DIR/device_slot.remote.selfhost.err" >&2
    exit 1
fi
grep -Fq -- 'PgyTaskHandle pending = pgy_submit_device_read_Int(&dev);' "$SELFHOST_REMOTE_C"
grep -Fq -- 'pgy_machine_await_remote_Int' "$SELFHOST_REMOTE_C"
grep -Fq -- 'pgy_await(handle)' "$SELFHOST_REMOTE_C"
if command -v gcc >/dev/null 2>&1; then
    SELFHOST_REMOTE_BIN="$BUILD_DIR/device_slot.remote.selfhost.exe"
    SELFHOST_REMOTE_OUT="$BUILD_DIR/device_slot.remote.selfhost.out"
    gcc -std=c11 -Wall -Wextra -Isrc -Isrc/runtime \
        "$SELFHOST_REMOTE_C" -pthread -lm -o "$SELFHOST_REMOTE_BIN"
    "$SELFHOST_REMOTE_BIN" >"$SELFHOST_REMOTE_OUT" 2>"$BUILD_DIR/device_slot.remote.selfhost.err"
    # Windows-native generated programs may write CRLF; the value gate is
    # about the scalar result, not the host newline convention.
    tr -d '\r' <"$SELFHOST_REMOTE_OUT" | grep -Fxq -- '11'
fi

CASE="tests/cases/backend_compare/device_slot_machine_layer/main.pgy"
RIR_CASE="$SOURCE_REMOTE_CASE"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$CASE")" >"$MIR_JSON")
MIR_REL="${MIR_JSON#$ROOT_DIR/}"
if ! (cd "$ROOT_DIR" && "$LOWER_BIN" "$MIR_REL" "$MACHINE_MANIFEST_REL" >"$AST_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] valid machine MIR was rejected" >&2
    cat "$AST_OUT" >&2
    exit 1
fi
grep -Fq -- 'DeviceSlot<Int>' "$AST_OUT"

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "${AIR_VALIDATOR_SOURCE#$ROOT_DIR/}")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$AIR_VALIDATOR_BIN")" \
    >"$AIR_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    echo "[self-host-mir-machine-layer] AIR validator rebuild failed" >&2
    cat "$AIR_LOG" >&2
    exit 1
fi
compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "${RIR_VALIDATOR_SOURCE#$ROOT_DIR/}")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$RIR_VALIDATOR_BIN")" \
    >"$RIR_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    echo "[self-host-mir-machine-layer] RIR validator rebuild failed" >&2
    cat "$RIR_LOG" >&2
    exit 1
fi
(cd "$ROOT_DIR" && "$PGY" --rir-json \
    "$(pgy_path_for_compiler "$PGY" "$RIR_CASE")" >"$RIR_JSON")
RIR_REL="${RIR_JSON#$ROOT_DIR/}"
if ! (cd "$ROOT_DIR" && "$RIR_VALIDATOR_BIN" "$RIR_REL" >"$RIR_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] valid machine RIR was rejected" >&2
    cat "$RIR_OUT" >&2
    exit 1
fi
grep -Fq -- 'pgy.selfhost.machine-layer-rir.v1|contacts=' "$RIR_OUT"
grep -Fq -- '"machine_contact":"submit-read"' "$RIR_JSON"
sed '0,/"machine_contact":"claim"/s//"machine_contact":"pergyra.invalid-contact"/' \
    "$RIR_JSON" >"$RIR_BAD_JSON"
RIR_BAD_REL="${RIR_BAD_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$RIR_VALIDATOR_BIN" "$RIR_BAD_REL" >"$RIR_BAD_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mutated machine RIR contact was accepted" >&2
    exit 1
fi
grep -Fq -- 'MACHINE-RIR ERROR: RIR machine contact is missing or unknown' "$RIR_BAD_OUT"
(cd "$ROOT_DIR" && "$PGY" --air-json \
    "$(pgy_path_for_compiler "$PGY" "$CASE")" >"$AIR_RAW")
PY_BIN=""
for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1; then
        PY_BIN="$candidate"
        break
    fi
done
if [[ -z "$PY_BIN" ]]; then
    echo "[self-host-mir-machine-layer] python3/python is required to normalize AIR JSON" >&2
    exit 1
fi
"$PY_BIN" - "$AIR_RAW" "$AIR_JSON" <<'PY'
import sys
raw, out = sys.argv[1:]
data = open(raw, "rb").read()
text = data.decode("utf-16") if data.startswith((b"\xff\xfe", b"\xfe\xff")) else data.decode("utf-8")
open(out, "wb").write(text.encode("utf-8"))
PY
AIR_REL="${AIR_JSON#$ROOT_DIR/}"
if ! (cd "$ROOT_DIR" && "$AIR_VALIDATOR_BIN" "$AIR_REL" "$MACHINE_MANIFEST_REL" >"$AIR_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] valid machine AIR was rejected" >&2
    cat "$AIR_OUT" >&2
    exit 1
fi
grep -Fq -- 'pgy.selfhost.machine-layer-air.v1|sites=' "$AIR_OUT"

sed '0,/"manifest":"pergyra.abstract-device-slot.v1"/s//"manifest":"pergyra.invalid-device-slot.v1"/' \
    "$AIR_JSON" >"$AIR_BAD_JSON"
AIR_BAD_REL="${AIR_BAD_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$AIR_VALIDATOR_BIN" "$AIR_BAD_REL" "$MACHINE_MANIFEST_REL" >"$AIR_BAD_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mutated machine AIR manifest was accepted" >&2
    exit 1
fi
grep -Fq -- 'MACHINE-AIR ERROR: AIR machine-layer site is missing owner facts' "$AIR_BAD_OUT"

sed '0,/\"manifest\":\"pergyra.abstract-device-slot.v1\"/s//\"manifest\":\"pergyra.invalid-device-slot.v1\"/' \
    "$MIR_JSON" >"$BAD_JSON"
BAD_REL="${BAD_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_REL" "$MACHINE_MANIFEST_REL" >"$BAD_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mutated machine manifest was accepted" >&2
    exit 1
fi
grep -Fq -- 'MIR machine-layer facts are missing or invalid' "$BAD_OUT"

sed '0,/\"physical_grant\":\"device-slot0\"/s//\"physical_grant\":\"pergyra.invalid-grant\"/' \
    "$MIR_JSON" >"$BAD_PHYSICAL_JSON"
BAD_PHYSICAL_REL="${BAD_PHYSICAL_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_PHYSICAL_REL" "$MACHINE_MANIFEST_REL" >"$BAD_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mutated physical grant was accepted" >&2
    exit 1
fi
grep -Fq -- 'MIR machine-layer facts are missing or invalid' "$BAD_OUT"

sed '0,/\"physical_base\":268435456/s//\"physical_base\":268435457/' \
    "$MIR_JSON" >"$BAD_PHYSICAL_SHAPE_JSON"
BAD_PHYSICAL_SHAPE_REL="${BAD_PHYSICAL_SHAPE_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_PHYSICAL_SHAPE_REL" "$MACHINE_MANIFEST_REL" >"$BAD_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] mutated physical grant shape was accepted" >&2
    exit 1
fi
grep -Fq -- 'MIR machine-layer facts are missing or invalid' "$BAD_OUT"

"$PY_BIN" - "$MIR_JSON" "$BAD_MISSING_JSON" <<'PY'
import json, sys
raw, out = sys.argv[1:]
data = open(raw, "rb").read()
text = data.decode("utf-16") if data.startswith((b"\xff\xfe", b"\xfe\xff")) else data.decode("utf-8")
doc = json.loads(text)
for routine in doc.get("routines", []):
    for block in routine.get("blocks", []):
        for instruction in block.get("instructions", []):
            if instruction.get("machine_contact_kind"):
                instruction["machine_layer"] = None
                open(out, "w", encoding="utf-8").write(json.dumps(doc, separators=(",", ":")))
                raise SystemExit(0)
raise SystemExit("no machine contact fact found")
PY
BAD_MISSING_REL="${BAD_MISSING_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_MISSING_REL" "$MACHINE_MANIFEST_REL" >"$BAD_MISSING_OUT" 2>&1); then
    echo "[self-host-mir-machine-layer] missing machine-layer owner fact was accepted" >&2
    exit 1
fi
grep -Fq -- 'MIR machine-layer facts are missing or invalid' "$BAD_MISSING_OUT"

echo "[self-host-mir-machine-layer] MIR/AIR declaration rows and self-host C DeviceSlot/RemoteFuture lowering are wired; malformed owner identity fails closed"
