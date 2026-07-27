#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:driver-execution-action-abi"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/driver_execution_action_abi_probe.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_execution_action_abi}"
EXPECTED_OUTPUT=$'ok\nartifact-written\n17'
EXPECTED_FILE_CONTENT="driver-action-abi"
mkdir -p "$BUILD_DIR"

[[ -f "$SOURCE" ]] || { echo "[$LABEL] missing fixture: $SOURCE" >&2; exit 1; }

require_source_term() {
    local term="$1"
    grep -Fq -- "$term" "$SOURCE" || {
        echo "[$LABEL] fixture no longer proves required ABI term: $term" >&2
        exit 1
    }
}

require_source_term "enum DriverExecutionActionAbiStage"
require_source_term "struct DriverExecutionActionAbiRequest"
require_source_term "struct DriverExecutionActionAbiResult"
require_source_term "let stage: DriverExecutionActionAbiStage;"
require_source_term "subject DriverExecutionActionAbiProbe"
require_source_term "request: DriverExecutionActionAbiRequest"
require_source_term ") -> DriverExecutionActionAbiResult with caps io_read, io_write {"
require_source_term "WriteFile(request.output_path, request.payload);"
require_source_term "let observed: String = ReadFile(request.output_path);"
require_source_term "let result: DriverExecutionActionAbiResult = probe.Execute(request);"

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/driver_execution_action_abi_${backend}.exe"
    local compile_log="$BUILD_DIR/driver_execution_action_abi_${backend}.compile.log"
    local stdout_file="$BUILD_DIR/driver_execution_action_abi_${backend}.out"
    local stderr_file="$BUILD_DIR/driver_execution_action_abi_${backend}.err"
    local artifact="$BUILD_DIR/${backend}.artifact.txt"
    local artifact_rel="${artifact#"$ROOT_DIR"/}"
    local observed=""

    if [[ "$artifact_rel" == "$artifact" ]]; then
        echo "[$LABEL] build directory must remain under the repository root" >&2
        return 1
    fi
    rm -f "$bin" "$stdout_file" "$stderr_file" "$artifact"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1); then
        echo "[$LABEL] $backend compile failed" >&2
        cat "$compile_log" >&2
        return 1
    fi
    pgy_require_runnable_binary_here "$LABEL:$backend" "$bin" || return 1

    if ! (cd "$ROOT_DIR" && "$bin" "$artifact_rel" >"$stdout_file" 2>"$stderr_file"); then
        echo "[$LABEL] $backend runtime failed" >&2
        cat "$stdout_file" "$stderr_file" >&2
        return 1
    fi
    if [[ -s "$stderr_file" ]]; then
        echo "[$LABEL] $backend runtime wrote unexpected stderr" >&2
        cat "$stderr_file" >&2
        return 1
    fi

    observed="$(tr -d '\r' < "$stdout_file")"
    if [[ "$observed" != "$EXPECTED_OUTPUT" ]]; then
        echo "[$LABEL] $backend returned the wrong action result/stage" >&2
        printf 'expected:\n%s\nactual:\n%s\n' "$EXPECTED_OUTPUT" "$observed" >&2
        return 1
    fi
    if [[ ! -f "$artifact" ]]; then
        echo "[$LABEL] $backend action did not create its artifact" >&2
        return 1
    fi
    if [[ "$(cat "$artifact")" != "$EXPECTED_FILE_CONTENT" ]]; then
        echo "[$LABEL] $backend action artifact content mismatch" >&2
        return 1
    fi
}

run_backend c
run_backend llvm

if ! cmp -s "$BUILD_DIR/c.artifact.txt" "$BUILD_DIR/llvm.artifact.txt"; then
    echo "[$LABEL] C/LLVM action artifacts differ" >&2
    exit 1
fi
if ! cmp -s \
    "$BUILD_DIR/driver_execution_action_abi_c.out" \
    "$BUILD_DIR/driver_execution_action_abi_llvm.out"; then
    echo "[$LABEL] C/LLVM action result/stage output differs" >&2
    exit 1
fi

echo "[$LABEL] subject/action aggregate ABI, enum result, capabilities, and WriteFile parity ok"
