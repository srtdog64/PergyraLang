#!/usr/bin/env bash
# DRV-0 parity: one Pergyra process assembles source -> AST text -> emitted C.
#
# Oracle shape:
#   - AST text: self-parser <source>
#   - emitted C: current self-host codegen tool compiled by the C oracle and fed
#     the same AST text
#
# The DRV-0 tool is compiled through the requested backends. C-only builds prove
# the C leg; LLVM-enabled builds also prove the LLVM-built driver emits the same
# artifacts. Artifact equality is judged by the Pergyra-owned backend output
# comparator, not by shell diff.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:driver-rung0] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:driver-rung0] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:driver-rung0" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_rung0}"
HARNESS_PATHS_FILE="$BUILD_DIR/driver_rung0_harness_paths.txt"
DRIVER_FIXTURE_MANIFEST_FILE="$BUILD_DIR/driver_fixture_manifest.txt"
FIXTURES=()

mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:driver-rung0" \
    "$BUILD_DIR" \
    "driver-rung0-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[self-host-parity:driver-rung0] TestHarness manifest expected 3 driver paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

DRIVER_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"
CODEGEN_SOURCE="$ROOT_DIR/${harness_paths[2]}"

for path in "$DRIVER_SOURCE" "$PARSER_SOURCE" "$CODEGEN_SOURCE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:driver-rung0] missing TestHarness input: $path" >&2
        exit 1
    fi
done

compile_tool() {
    local label="$1"
    local source="$2"
    local backend="$3"
    local out_bin="$4"
    local compile_log="$BUILD_DIR/${label}_${backend}.compile.log"

    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" \
        >"$compile_log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$compile_log"; then
            return 2
        fi
        echo "[self-host-parity:driver-rung0] $label backend=$backend compile failed" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

read_driver_fixture_manifest() {
    local manifest_bin="$BUILD_DIR/driver_rung0_manifest.exe"
    local manifest_err="$BUILD_DIR/driver_rung0_manifest.err"
    local line

    compile_tool "driver-rung0-manifest" "$DRIVER_SOURCE" c "$manifest_bin"

    FIXTURES=()
    if ! (cd "$ROOT_DIR" && "$manifest_bin" --fixture-manifest \
        >"$DRIVER_FIXTURE_MANIFEST_FILE" \
        2>"$manifest_err"); then
        echo "[self-host-parity:driver-rung0] fixture manifest emission failed" >&2
        cat "$manifest_err" >&2
        exit 1
    fi

    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        FIXTURES+=("$line")
    done <"$DRIVER_FIXTURE_MANIFEST_FILE"

    if [[ "${#FIXTURES[@]}" -ne 68 ]]; then
        echo "[self-host-parity:driver-rung0] fixture manifest count drifted: ${#FIXTURES[@]} != 68" >&2
        exit 1
    fi
}

capture_tool() {
    local label="$1"
    local out_file="$2"
    local err_file="$3"
    local bin="$4"
    shift 4

    set +e
    (cd "$ROOT_DIR" && "$bin" "$@" >"$out_file.raw" 2>"$err_file")
    local rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"
    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:driver-rung0] $label failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
}

compare_artifact() {
    local label="$1"
    local expected_file="$2"
    local actual_file="$3"
    local artifact_kind="$4"

    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$label" "$BUILD_DIR" "$expected_file" "$actual_file" "$artifact_kind"
}

CODEGEN_BIN="$BUILD_DIR/codegen_oracle.exe"
PARSER_BIN="$BUILD_DIR/parser_ast_producer.exe"
compile_tool "parser-ast-producer" "$PARSER_SOURCE" c "$PARSER_BIN"
compile_tool "codegen-oracle" "$CODEGEN_SOURCE" c "$CODEGEN_BIN"
read_driver_fixture_manifest

BACKENDS="${PGY_SELFHOST_DRIVER_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()

for backend in $BACKENDS; do
    DRIVER_BIN="$BUILD_DIR/driver_rung0_${backend}.exe"
    set +e
    compile_tool "driver-rung0" "$DRIVER_SOURCE" "$backend" "$DRIVER_BIN"
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        echo "[self-host-parity:driver-rung0] LLVM backend unavailable; skipping llvm-built driver"
        SKIPPED_BACKENDS+=("$backend")
        continue
    fi
    if [[ "$compile_rc" -ne 0 ]]; then
        exit "$compile_rc"
    fi

    for fixture_rel in "${FIXTURES[@]}"; do
        fixture_abs="$ROOT_DIR/$fixture_rel"
        base="$(basename "$fixture_rel" .pgy)"
        expected_ast="$BUILD_DIR/${base}_expected.ast.txt"
        driver_ast="$BUILD_DIR/${base}_${backend}.driver.ast.txt"
        expected_c="$BUILD_DIR/${base}_expected.c"
        driver_c="$BUILD_DIR/${base}_${backend}.driver.c"

        if [[ ! -f "$fixture_abs" ]]; then
            echo "[self-host-parity:driver-rung0] missing fixture: $fixture_rel" >&2
            exit 1
        fi

        capture_tool "self-parser AST $fixture_rel" "$expected_ast" "$BUILD_DIR/${base}_expected_ast.err" \
            "$PARSER_BIN" "$fixture_rel"
        capture_tool "driver $backend --emit-ast $fixture_rel" "$driver_ast" "$BUILD_DIR/${base}_${backend}_driver_ast.err" \
            "$DRIVER_BIN" "$fixture_rel" --emit-ast
        compare_artifact "driver-rung0:ast:$backend:$base" "$expected_ast" "$driver_ast" "ast_text"

        ast_rel="${expected_ast#"$ROOT_DIR"/}"
        capture_tool "codegen oracle $fixture_rel" "$expected_c" "$BUILD_DIR/${base}_expected_c.err" \
            "$CODEGEN_BIN" "$ast_rel"
        capture_tool "driver $backend --emit-c $fixture_rel" "$driver_c" "$BUILD_DIR/${base}_${backend}_driver_c.err" \
            "$DRIVER_BIN" "$fixture_rel" --emit-c
        compare_artifact "driver-rung0:c:$backend:$base" "$expected_c" "$driver_c" "emitted_c"
    done

    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:driver-rung0] no requested backend ran" >&2
    exit 1
fi

label="${RAN_BACKENDS[*]}"
if [[ "${#SKIPPED_BACKENDS[@]}" -gt 0 ]]; then
    label="$label; ${SKIPPED_BACKENDS[*]} skipped"
fi
echo "[self-host-parity:driver-rung0] DRV-0 artifact parity ok (backends=$label fixtures=${#FIXTURES[@]})"
