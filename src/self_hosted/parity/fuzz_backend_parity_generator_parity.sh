#!/usr/bin/env bash
# Pergyra-origin backend parity fuzz smoke.
#
# The generator is written in Pergyra. This script is only the driver:
#   1. compile the generator through C;
#   2. when LLVM is available, compile it through LLVM too;
#   3. assert both generator binaries emit the same manifest and sources;
#   4. optionally compile/run generated cases through C and LLVM and compare
#      observable stdout/stderr/exit.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:fuzz-generator] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:fuzz-generator] missing compiler binary: $PGY" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/fuzz/backend_parity_generator/main.pgy"
if [[ ! -f "$TOOL_SOURCE" ]]; then
    echo "[self-host-parity:fuzz-generator] missing generator source: $TOOL_SOURCE" >&2
    exit 1
fi

SEED="${PGY_FUZZ_SEED:-1001}"
COUNT="${PGY_FUZZ_COUNT:-8}"
RUN_ORACLE="${PGY_FUZZ_BACKEND_RUN_ORACLE:-0}"
RUN_TIMEOUT_SECONDS="${PGY_FUZZ_RUN_TIMEOUT_SECONDS:-30}"

WORK_ROOT="$ROOT_DIR/.tmp/self_hosted"
mkdir -p "$WORK_ROOT"
WORK_DIR="$(mktemp -d "$WORK_ROOT/fuzz_backend_parity.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"

cleanup() {
    local rc=$?
    if [[ "$rc" -eq 0 && "${PGY_FUZZ_KEEP_DIR:-0}" != "1" ]]; then
        rm -rf "$WORK_DIR"
    else
        echo "[self-host-parity:fuzz-generator] preserved work dir: $WORK_DIR" >&2
    fi
}
trap cleanup EXIT

files_equal() {
    local left="$1"
    local right="$2"

    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$left" "$right"
        return $?
    fi

    [[ "$(cat "$left")" == "$(cat "$right")" ]]
}

show_diff() {
    local left="$1"
    local right="$2"

    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index --no-prefix -- "$left" "$right" || true
        return 0
    fi
    if command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
        return 0
    fi
    echo "--- left ---"
    cat "$left"
    echo "--- right ---"
    cat "$right"
}

resolve_native_bin() {
    local path="$1"
    if [[ -x "$path" ]]; then
        printf '%s\n' "$path"
    elif [[ "$path" != *.exe && -x "${path}.exe" ]]; then
        printf '%s.exe\n' "$path"
    else
        printf '%s\n' "$path"
    fi
}

run_native_capture() {
    local cwd="$1"
    local out="$2"
    local err="$3"
    local bin="$4"
    shift 4

    local use_windows_bridge=0
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            if pgy_binary_expects_windows_paths "$bin" \
                && command -v cmd.exe >/dev/null 2>&1; then
                use_windows_bridge=1
            fi
            ;;
    esac

    if [[ "$use_windows_bridge" -eq 0 ]] && pgy_binary_is_runnable_here "$bin"; then
        if command -v timeout >/dev/null 2>&1; then
            (cd "$cwd" && timeout "$RUN_TIMEOUT_SECONDS"s "$bin" "$@" >"$out" 2>"$err")
        else
            (cd "$cwd" && "$bin" "$@" >"$out" 2>"$err")
        fi
        return $?
    fi

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) return 127 ;;
    esac
    command -v cmd.exe >/dev/null 2>&1 || return 127

    local cwd_native
    local bin_native
    local out_native
    local err_native
    local args_cmd=""
    local arg

    cwd_native="$(pgy_path_for_windows_tool "$cwd")"
    bin_native="$(pgy_path_for_windows_tool "$bin")"
    out_native="$(pgy_path_for_windows_tool "$out")"
    err_native="$(pgy_path_for_windows_tool "$err")"
    for arg in "$@"; do
        local escaped_arg="${arg//\"/\\\"}"
        args_cmd="${args_cmd} \"${escaped_arg}\""
    done

    cmd.exe //d //c "cd /d \"${cwd_native}\" && \"${bin_native}\"${args_cmd} > \"${out_native}\" 2> \"${err_native}\""
}

compile_backend() {
    local source="$1"
    local backend="$2"
    local out="$3"
    local log="$4"

    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$out")") >"$log" 2>&1
}

assert_file_equal() {
    local label="$1"
    local left="$2"
    local right="$3"

    if ! files_equal "$left" "$right"; then
        echo "[self-host-parity:fuzz-generator] $label mismatch" >&2
        show_diff "$left" "$right" >&2
        exit 1
    fi
}

normalize_output() {
    local input="$1"
    local output="$2"
    tr -d '\r' <"$input" >"$output"
}

run_generator() {
    local label="$1"
    local bin="$2"
    local corpus_rel="$3"
    local stdout_file="$4"
    local stderr_file="$5"
    local rc

    mkdir -p "$ROOT_DIR/$corpus_rel"
    set +e
    run_native_capture "$ROOT_DIR" "$stdout_file" "$stderr_file" "$bin" \
        "$SEED" "$COUNT" "$corpus_rel"
    rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:fuzz-generator] $label generator failed rc=$rc" >&2
        cat "$stdout_file" "$stderr_file" >&2
        exit 1
    fi
    if ! grep -Fq '"schema":"pgy.selfhost.backend-parity-fuzz-generator.v1"' "$stdout_file" \
        || ! grep -Fq '"ok":true' "$stdout_file"; then
        echo "[self-host-parity:fuzz-generator] $label generator schema/ok missing" >&2
        cat "$stdout_file" >&2
        exit 1
    fi
}

run_backend_case() {
    local index="$1"
    local source="$2"
    local case_base="$WORK_DIR/case_${index}"
    local c_bin="$case_base.c"
    local llvm_bin="$case_base.llvm"
    local c_stdout="$case_base.c.stdout"
    local llvm_stdout="$case_base.llvm.stdout"
    local c_stderr="$case_base.c.stderr"
    local llvm_stderr="$case_base.llvm.stderr"
    local c_stdout_norm="$case_base.c.stdout.norm"
    local llvm_stdout_norm="$case_base.llvm.stdout.norm"
    local c_stderr_norm="$case_base.c.stderr.norm"
    local llvm_stderr_norm="$case_base.llvm.stderr.norm"
    local c_rc
    local llvm_rc

    if ! compile_backend "$source" c "$c_bin" "$case_base.c.compile.log"; then
        echo "[self-host-parity:fuzz-generator] generated case $index failed C compile" >&2
        cat "$case_base.c.compile.log" >&2
        exit 1
    fi
    if ! compile_backend "$source" llvm "$llvm_bin" "$case_base.llvm.compile.log"; then
        echo "[self-host-parity:fuzz-generator] generated case $index failed LLVM compile" >&2
        cat "$case_base.llvm.compile.log" >&2
        exit 1
    fi

    c_bin="$(resolve_native_bin "$c_bin")"
    llvm_bin="$(resolve_native_bin "$llvm_bin")"

    set +e
    run_native_capture "$ROOT_DIR" "$c_stdout" "$c_stderr" "$c_bin"
    c_rc=$?
    run_native_capture "$ROOT_DIR" "$llvm_stdout" "$llvm_stderr" "$llvm_bin"
    llvm_rc=$?
    set -e

    if [[ "$c_rc" -ne "$llvm_rc" ]]; then
        echo "[self-host-parity:fuzz-generator] generated case $index exit mismatch C=$c_rc LLVM=$llvm_rc" >&2
        exit 1
    fi

    normalize_output "$c_stdout" "$c_stdout_norm"
    normalize_output "$llvm_stdout" "$llvm_stdout_norm"
    normalize_output "$c_stderr" "$c_stderr_norm"
    normalize_output "$llvm_stderr" "$llvm_stderr_norm"
    assert_file_equal "generated case $index stdout" "$c_stdout_norm" "$llvm_stdout_norm"
    assert_file_equal "generated case $index stderr" "$c_stderr_norm" "$llvm_stderr_norm"
}

C_GEN="$WORK_DIR/backend_parity_generator_c"
LLVM_GEN="$WORK_DIR/backend_parity_generator_llvm"

if ! compile_backend "$TOOL_SOURCE" c "$C_GEN" "$WORK_DIR/generator_c.compile.log"; then
    echo "[self-host-parity:fuzz-generator] C generator compile failed" >&2
    cat "$WORK_DIR/generator_c.compile.log" >&2
    exit 1
fi
C_GEN="$(resolve_native_bin "$C_GEN")"

LLVM_AVAILABLE=1
if ! compile_backend "$TOOL_SOURCE" llvm "$LLVM_GEN" "$WORK_DIR/generator_llvm.compile.log"; then
    LLVM_AVAILABLE=0
fi

C_CORPUS_REL="$WORK_REL/corpus_c"
run_generator "c" "$C_GEN" "$C_CORPUS_REL" \
    "$WORK_DIR/generator_c.stdout" "$WORK_DIR/generator_c.stderr"

if [[ "$LLVM_AVAILABLE" -eq 0 ]]; then
    echo "[self-host-parity:fuzz-generator] SKIP LLVM leg unavailable; C generator ok (seed=$SEED count=$COUNT)"
    exit 0
fi

LLVM_GEN="$(resolve_native_bin "$LLVM_GEN")"
LLVM_CORPUS_REL="$WORK_REL/corpus_llvm"
run_generator "llvm" "$LLVM_GEN" "$LLVM_CORPUS_REL" \
    "$WORK_DIR/generator_llvm.stdout" "$WORK_DIR/generator_llvm.stderr"

assert_file_equal "generator stdout" "$WORK_DIR/generator_c.stdout" "$WORK_DIR/generator_llvm.stdout"
assert_file_equal "generator stderr" "$WORK_DIR/generator_c.stderr" "$WORK_DIR/generator_llvm.stderr"
assert_file_equal "manifest" \
    "$ROOT_DIR/$C_CORPUS_REL/manifest.jsonl" \
    "$ROOT_DIR/$LLVM_CORPUS_REL/manifest.jsonl"

for ((i = 0; i < COUNT; i++)); do
    assert_file_equal "generated f${i}.pgy" \
        "$ROOT_DIR/$C_CORPUS_REL/f${i}.pgy" \
        "$ROOT_DIR/$LLVM_CORPUS_REL/f${i}.pgy"
done

if [[ "$RUN_ORACLE" == "1" ]]; then
    for ((i = 0; i < COUNT; i++)); do
        run_backend_case "$i" "$ROOT_DIR/$C_CORPUS_REL/f${i}.pgy"
    done
    echo "[self-host-parity:fuzz-generator] generator + generated backend oracle ok (seed=$SEED count=$COUNT)"
else
    echo "[self-host-parity:fuzz-generator] generator parity ok (seed=$SEED count=$COUNT)"
fi
