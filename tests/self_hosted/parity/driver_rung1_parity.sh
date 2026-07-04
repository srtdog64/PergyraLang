#!/usr/bin/env bash
# DRV-1 parity: the Pergyra driver owns a real CLI surface.
#
# Oracle shape:
#   - AST text: live `pgy --ast <source>`
#   - emitted C: current self-host codegen tool compiled by the C oracle and fed
#     the same AST text
#   - stdout and `-o` file output are both compared through the Pergyra-owned
#     backend output comparator.

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
        echo "[self-host-parity:driver-rung1] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:driver-rung1] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:driver-rung1" "$PGY"

DRIVER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/driver_rung1_main.pgy"
CODEGEN_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_rung1}"

FIXTURES=(
    "examples/hello.pgy"
    "src/self_hosted/codegen/fixture/func_call.pgy"
    "src/self_hosted/codegen/fixture/struct_param.pgy"
)

mkdir -p "$BUILD_DIR"

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
        echo "[self-host-parity:driver-rung1] $label backend=$backend compile failed" >&2
        cat "$compile_log" >&2
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
        echo "[self-host-parity:driver-rung1] $label failed rc=$rc" >&2
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
compile_tool "codegen-oracle" "$CODEGEN_SOURCE" c "$CODEGEN_BIN"

BACKENDS="${PGY_SELFHOST_DRIVER_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()

for backend in $BACKENDS; do
    DRIVER_BIN="$BUILD_DIR/driver_rung1_${backend}.exe"
    set +e
    compile_tool "driver-rung1" "$DRIVER_SOURCE" "$backend" "$DRIVER_BIN"
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        echo "[self-host-parity:driver-rung1] LLVM backend unavailable; skipping llvm-built driver"
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
        driver_ast_stdout="$BUILD_DIR/${base}_${backend}.driver.ast.stdout.txt"
        driver_ast_file="$BUILD_DIR/${base}_${backend}.driver.ast.file.txt"
        driver_ast_file_rel="${driver_ast_file#"$ROOT_DIR"/}"
        expected_c="$BUILD_DIR/${base}_expected.c"
        driver_c_stdout="$BUILD_DIR/${base}_${backend}.driver.c.stdout.txt"
        driver_c_file="$BUILD_DIR/${base}_${backend}.driver.c.file.txt"
        driver_c_file_rel="${driver_c_file#"$ROOT_DIR"/}"

        if [[ ! -f "$fixture_abs" ]]; then
            echo "[self-host-parity:driver-rung1] missing fixture: $fixture_rel" >&2
            exit 1
        fi

        capture_tool "pgy --ast $fixture_rel" "$expected_ast" "$BUILD_DIR/${base}_expected_ast.err" \
            "$PGY" --ast "$(pgy_path_for_compiler "$PGY" "$fixture_abs")"
        capture_tool "driver $backend --emit-ast stdout $fixture_rel" "$driver_ast_stdout" "$BUILD_DIR/${base}_${backend}_driver_ast_stdout.err" \
            "$DRIVER_BIN" "$fixture_rel" --emit-ast
        compare_artifact "driver-rung1:ast-stdout:$backend:$base" "$expected_ast" "$driver_ast_stdout" "ast_text"

        rm -f "$driver_ast_file"
        capture_tool "driver $backend --emit-ast -o $fixture_rel" "$BUILD_DIR/${base}_${backend}_driver_ast_file.stdout" "$BUILD_DIR/${base}_${backend}_driver_ast_file.err" \
            "$DRIVER_BIN" "$fixture_rel" --emit-ast -o "$driver_ast_file_rel"
        [[ -f "$driver_ast_file" ]] || {
            echo "[self-host-parity:driver-rung1] AST -o output missing: $driver_ast_file_rel" >&2
            exit 1
        }
        compare_artifact "driver-rung1:ast-file:$backend:$base" "$expected_ast" "$driver_ast_file" "ast_text"

        ast_rel="${expected_ast#"$ROOT_DIR"/}"
        capture_tool "codegen oracle $fixture_rel" "$expected_c" "$BUILD_DIR/${base}_expected_c.err" \
            "$CODEGEN_BIN" "$ast_rel"
        capture_tool "driver $backend --emit-c stdout $fixture_rel" "$driver_c_stdout" "$BUILD_DIR/${base}_${backend}_driver_c_stdout.err" \
            "$DRIVER_BIN" "$fixture_rel" --emit-c
        compare_artifact "driver-rung1:c-stdout:$backend:$base" "$expected_c" "$driver_c_stdout" "emitted_c"

        rm -f "$driver_c_file"
        capture_tool "driver $backend default -o $fixture_rel" "$BUILD_DIR/${base}_${backend}_driver_c_file.stdout" "$BUILD_DIR/${base}_${backend}_driver_c_file.err" \
            "$DRIVER_BIN" "$fixture_rel" -o "$driver_c_file_rel"
        [[ -f "$driver_c_file" ]] || {
            echo "[self-host-parity:driver-rung1] C -o output missing: $driver_c_file_rel" >&2
            exit 1
        }
        compare_artifact "driver-rung1:c-file:$backend:$base" "$expected_c" "$driver_c_file" "emitted_c"
    done

    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:driver-rung1] no requested backend ran" >&2
    exit 1
fi

label="${RAN_BACKENDS[*]}"
if [[ "${#SKIPPED_BACKENDS[@]}" -gt 0 ]]; then
    label="$label; ${SKIPPED_BACKENDS[*]} skipped"
fi
echo "[self-host-parity:driver-rung1] DRV-1 CLI parity ok (backends=$label fixtures=${#FIXTURES[@]})"
