#!/usr/bin/env bash

# Sourced by domain_runtime_zone_sync_execution_owner.sh. The installed
# self-host MIR producer and the source-owned direct-MIR C/LLVM projector must
# consume the same default identity carriage for Zone and World parameters.

: "${ROOT_DIR:?zone identity direct-MIR gate requires ROOT_DIR}"
: "${BUILD_DIR:?zone identity direct-MIR gate requires BUILD_DIR}"
: "${PGY_BIN:?zone identity direct-MIR gate requires PGY_BIN}"
: "${SELF_DRIVER:?zone identity direct-MIR gate requires SELF_DRIVER}"
: "${CC_BIN:?zone identity direct-MIR gate requires CC_BIN}"

IDENTITY_MIR_DIR="$BUILD_DIR/zone-identity-direct-mir"
IDENTITY_MIR_PROBE_SOURCE="$ROOT_DIR/tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy"
IDENTITY_MIR_PROBE="$IDENTITY_MIR_DIR/probe.exe"
IDENTITY_MIR_CLANG="${CLANG_BIN:-clang}"
IDENTITY_MIR_PYTHON="${PYTHON_BIN:-python3}"
mkdir -p "$IDENTITY_MIR_DIR"

"$PGY_BIN" --native-pipeline --backend=c --opt=dev \
    "$IDENTITY_MIR_PROBE_SOURCE" -o "$IDENTITY_MIR_PROBE"

run_identity_mir_case() {
    local label="$1"
    local source="$2"
    local type_name="$3"
    local expected="$4"
    local mir="$IDENTITY_MIR_DIR/$label.json"
    local c_file="$IDENTITY_MIR_DIR/$label.c"
    local llvm_file="$IDENTITY_MIR_DIR/$label.ll"
    local c_bin="$IDENTITY_MIR_DIR/$label-c.exe"
    local llvm_bin="$IDENTITY_MIR_DIR/$label-llvm.exe"
    local c_out="$IDENTITY_MIR_DIR/$label-c.out"
    local llvm_out="$IDENTITY_MIR_DIR/$label-llvm.out"
    local expected_out="$IDENTITY_MIR_DIR/$label.expected"
    local value_mutant="$IDENTITY_MIR_DIR/$label-value-carriage.json"
    local mutant_out="$IDENTITY_MIR_DIR/$label-value-carriage.out"
    local mutant_err="$IDENTITY_MIR_DIR/$label-value-carriage.err"

    "$SELF_DRIVER" --emit-mir-json-verified "${source#"$ROOT_DIR/"}" \
        >"$mir"
    grep -Fq "\"type\":\"$type_name\",\"carriage\":\"mutable-identity\",\"resource\":\"none\",\"pass\":\"indirect\"" \
        "$mir"

    "$IDENTITY_MIR_PROBE" "${mir#"$ROOT_DIR/"}" c >"$c_file"
    if grep -Eq 'pgy_identity_cell_[0-9]+ \*[[:space:]]+\*pgy_param_0' "$c_file"; then
        echo "$label default identity regressed to pointer-to-pointer C ABI" >&2
        exit 1
    fi
    grep -Eq 'pgy_identity_cell_[0-9]+ \*[[:space:]]+pgy_param_0' "$c_file"
    "$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
        "${POSIX_FEATURE_FLAGS[@]}" \
        -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
        "$c_file" -o "$c_bin"
    "$c_bin" | tr -d '\r' >"$c_out"

    "$IDENTITY_MIR_PROBE" "${mir#"$ROOT_DIR/"}" llvm >"$llvm_file"
    grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\)' \
        "$llvm_file"
    "$IDENTITY_MIR_CLANG" "$llvm_file" -o "$llvm_bin"
    "$llvm_bin" | tr -d '\r' >"$llvm_out"

    printf '%s\n' "$expected" >"$expected_out"
    cmp -s "$expected_out" "$c_out"
    cmp -s "$expected_out" "$llvm_out"

    "$IDENTITY_MIR_PYTHON" - "$mir" "$value_mutant" "$type_name" <<'PY'
import json
import pathlib
import sys

source, target, type_name = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
document = json.loads(source.read_text(encoding="utf-8"))
matches = 0
for routine in document["routines"]:
    for parameter in routine["params"]:
        if parameter["type"] == type_name and parameter["carriage"] == "mutable-identity":
            parameter["carriage"] = "value"
            parameter["pass"] = "direct"
            matches += 1
if matches != 1:
    raise SystemExit("expected one default identity parameter")
target.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
PY
    if "$IDENTITY_MIR_PROBE" "${value_mutant#"$ROOT_DIR/"}" c \
        >"$mutant_out" 2>"$mutant_err"; then
        echo "$label value-carriage mutant escaped direct-MIR admission" >&2
        exit 1
    fi
    grep -Fq 'CODEGEN ERROR:' "$mutant_out" "$mutant_err"
    if grep -Eq '^#include |^define ' "$mutant_out"; then
        echo "$label value-carriage rejection leaked a partial artifact" >&2
        exit 1
    fi
}

run_identity_mir_case zone \
    "$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_parameter_identity.pgy" \
    CounterZone 7
run_identity_mir_case world \
    "$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_world_zone_identity_parameter.pgy" \
    CartWorld 1

run_public_identity_case() {
    local label="$1"
    local source="$2"
    local expected="$3"
    local backend
    for backend in c llvm; do
        local binary="$IDENTITY_MIR_DIR/public-$label-$backend.exe"
        local output="$IDENTITY_MIR_DIR/public-$label-$backend.out"
        (cd "$ROOT_DIR" && "$PGY_BIN" "${source#"$ROOT_DIR/"}" \
            --backend="$backend" -o "${binary#"$ROOT_DIR/"}")
        "$binary" | tr -d '\r' >"$output"
        printf '%s\n' "$expected" >"$IDENTITY_MIR_DIR/public-$label.expected"
        cmp -s "$IDENTITY_MIR_DIR/public-$label.expected" "$output"
    done
}

run_public_identity_case zone \
    "$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_parameter_identity.pgy" 7
run_public_identity_case world \
    "$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_world_zone_identity_parameter.pgy" 1

if rg -q 'DirectMirScalarProgramIdentityCellBorrowCarriage' \
    "$ROOT_DIR/src/self_hosted/compiler"; then
    echo "nominal kind reintroduced a second identity parameter carriage owner" >&2
    exit 1
fi

echo "[domain-runtime-zone-identity-direct-mir] self MIR projector + public C/LLVM default Zone/World identity and value-carriage refusal: PASS"
