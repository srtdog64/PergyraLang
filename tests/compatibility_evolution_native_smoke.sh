#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[compatibility-evolution-native] missing $file contract: $text" >&2
        exit 1
    fi
}

require_text src/compiler/driver_diag.h \
    "driver_diag_compatibility_manifest_validate_file"
require_text src/compiler/driver_diag.c \
    "compatibility manifest does not cover all evolution surfaces"
require_text src/compiler/driver_app.c \
    "driver_diag_compatibility_manifest_validate_file"
require_text src/self_hosted/compiler/expected/compatibility_evolution.txt \
    "change|"
for runtime_policy in \
    "runtime_call_abi_schema=pgy.selfhost.runtime-call-compat.v1" \
    "runtime_call_abi_protocol=pergyra.runtime-call-abi.v2" \
    "runtime_call_abi_unknown_version=reject" \
    "runtime_call_abi_unknown_field=reject" \
    "runtime_call_abi_missing_fact=fail_closed" \
    "runtime_call_abi_constructed_nominal=mir_materialize_once" \
    "runtime_call_abi_policy=same_major_reject_unknown_fields_fail_closed"; do
    require_text src/self_hosted/compiler/expected/compatibility_evolution.txt \
        "$runtime_policy"
done

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[compatibility-evolution-native] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_compatibility_native.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
OUT="$WORK_DIR/basic.exe"
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/basic.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$OUT")" \
    >"$WORK_DIR/compile.log" 2>&1)
test -s "$OUT"

echo "[compatibility-evolution-native] native driver consumes the self-host manifest"
