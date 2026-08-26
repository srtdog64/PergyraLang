#!/usr/bin/env bash
# Public C and LLVM compiles must consume the installed DRV-2 callable identity
# lane. The explicit native pipeline is only the runtime-output oracle; its MIR
# identity space is not claimed equivalent here.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:callable-parameter-installed"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/callable_parameter_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/cases/backend_compare/compose_two_functions/main.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
DRIVER="$(cd "$(dirname "$DRIVER")" && pwd -P)/$(basename "$DRIVER")"
suffix=""
installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    suffix=".exe"
    installed_name="pgy-self-driver.exe"
fi
[[ "$DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "callable acceptance requires the driver installed beside pgy"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
printf '16\n13\n6\n' >"$WORK_DIR/expected.run"

compile_public() {
    local backend="$1"
    local stem="$2"
    local output_rel="$WORK_REL/$stem$suffix"
    (cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
        "$PGY" "$SOURCE_REL" --backend="$backend" -o "$output_rel") \
        >"$WORK_DIR/$stem.compile.out" 2>"$WORK_DIR/$stem.compile.err" || {
        cat "$WORK_DIR/$stem.compile.out" "$WORK_DIR/$stem.compile.err" >&2
        fail "installed $backend compile rejected callable parameters"
    }
    "$WORK_DIR/$stem$suffix" | tr -d '\r' >"$WORK_DIR/$stem.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$stem.run" ||
        fail "installed $backend callable output drifted"
}

compile_public c installed-c
compile_public llvm installed-llvm

NATIVE_REL="$WORK_REL/explicit-native-c$suffix"
(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE &&
    "$PGY" "$SOURCE_REL" --backend=c --native-pipeline -o "$NATIVE_REL") \
    >"$WORK_DIR/native.compile.out" 2>"$WORK_DIR/native.compile.err" || {
    cat "$WORK_DIR/native.compile.out" "$WORK_DIR/native.compile.err" >&2
    fail "explicit native runtime oracle did not compile"
}
"$WORK_DIR/explicit-native-c$suffix" | tr -d '\r' >"$WORK_DIR/native.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/native.run" ||
    fail "explicit native runtime oracle drifted"
cmp -s "$WORK_DIR/native.run" "$WORK_DIR/installed-c.run" ||
    fail "installed C diverged from the runtime oracle"
cmp -s "$WORK_DIR/native.run" "$WORK_DIR/installed-llvm.run" ||
    fail "installed LLVM diverged from the runtime oracle"

echo "[$LABEL] public C/LLVM callable substitution + native runtime oracle: PASS"
