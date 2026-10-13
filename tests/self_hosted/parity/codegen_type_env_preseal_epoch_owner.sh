#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
NATIVE_LLVM_PGY="${PGY_NATIVE_LLVM_BIN:-$ROOT_DIR/bin-dev-llvm/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/codegen_type_env_preseal_epoch_owner.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen_type_env_preseal_epoch}"
mkdir -p "$BUILD_DIR"

run_backend() {
    local label="$1"
    local compiler="$2"
    local backend="$3"
    shift 3
    local bin="$BUILD_DIR/preseal_${label}.exe"
    local compile_log="$BUILD_DIR/preseal_${label}.compile.log"
    (cd "$ROOT_DIR" && "$compiler" "$@" \
        "$(pgy_path_for_compiler "$compiler" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$compiler" "$bin")" \
        >"$compile_log" 2>&1) || {
        cat "$compile_log" >&2
        return 1
    }
    pgy_require_runnable_binary_here \
        "self-host-parity:codegen-type-env-preseal:$label" "$bin"
    local output
    output="$(cd "$ROOT_DIR" && "$bin" | tr -d '\r')"
    [[ "$output" == "codegen-type-env-preseal-epoch-ok" ]] || {
        echo "[self-host-parity:codegen-type-env-preseal] $label output drifted: $output" >&2
        return 1
    }
    if (cd "$ROOT_DIR" && "$bin" --malformed \
        >"$BUILD_DIR/preseal_${label}.malformed.out" \
        2>"$BUILD_DIR/preseal_${label}.malformed.err"); then
        echo "[self-host-parity:codegen-type-env-preseal] $label malformed delta was accepted" >&2
        return 1
    fi
    grep -Fq 'codegen preseal type-row delta is malformed' \
        "$BUILD_DIR/preseal_${label}.malformed.out" \
        "$BUILD_DIR/preseal_${label}.malformed.err" || {
        echo "[self-host-parity:codegen-type-env-preseal] $label malformed diagnostic drifted" >&2
        return 1
    }
}

run_backend installed_c "$PGY" c
run_backend native_llvm "$NATIVE_LLVM_PGY" llvm --native-pipeline

echo "[self-host-parity:codegen-type-env-preseal] ordered delta C/native LLVM parity ok"
