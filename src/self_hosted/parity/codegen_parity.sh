#!/usr/bin/env bash
# Rung-0..15 parity for the Pergyra-origin C codegen substitute (2026-06-17).
#
# This is the first *hard compiler-core* substitution gate, opened after the
# 2026-06-17 BDFL decision lifted the hard-migration freeze
# (docs/self_hosted/README.md). The criterion is run-output equivalence, not
# byte-equal C: the oracle emits MIR-lowered C with runtime headers, while the
# Pergyra emitter produces standalone C. They are judged equal only by the
# observable stdout of the compiled program.
#
# For each fixture <name>.pgy:
#   1. live oracle: build the fixture through the C backend -> exe, run it,
#      capture stdout. The committed expected/<name>_stdout.txt MUST equal this
#      (live-drift guard, mirroring parser_parity.sh).
#   2. self-host: `pgy --ast <fixture>` -> ast.txt; run the codegen tool on
#      ast.txt -> out.c; gcc out.c -> exe; run it, capture stdout.
#   3. assert self stdout == committed expected (== oracle), tr -d '\r'.
#
# The codegen tool itself is compiled through the requested backends. LLVM is
# mandatory in LLVM-enabled builds and explicitly skipped in C-only builds.
#
# Runner contract: every fixture, including no-argument fixtures such as
# `hello`, must execute through the same command-array path in both oracle and
# self-host legs. Keep the executable as element 0 of the array; this avoids
# empty-argument expansion differences across bash versions under nounset.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/src/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix_from_current_path)"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:codegen] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:codegen] missing compiler binary: $PGY" >&2
    exit 1
fi

CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-parity:codegen] SKIP missing C compiler on PATH: $CC"
    exit 0
fi

TOOL_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/codegen/fixture"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/codegen/expected"
# Build artifacts live under a repo-relative dir. The Pergyra codegen tool is a
# native binary that resolves its AST-path argument relative to cwd, so it is
# always invoked from ROOT_DIR with a repo-relative path (mirrors
# parser_parity.sh). gcc/run paths use the absolute form.
REL_BUILD=".tmp/self_hosted/codegen"
ABS_BUILD="$ROOT_DIR/$REL_BUILD"

if [[ ! -f "$TOOL_SOURCE" ]]; then
    echo "[self-host-parity:codegen] missing Pergyra tool: $TOOL_SOURCE" >&2
    exit 1
fi

mkdir -p "$ABS_BUILD"

run_native_capture() {
    local cwd="$1"
    local out="$2"
    local err="$3"
    local bin="$4"
    shift 4

    if pgy_binary_is_runnable_here "$bin"; then
        (cd "$cwd" && "$bin" "$@" >"$out" 2>"$err")
        local direct_rc=$?
        case "$direct_rc" in
            126|127)
                case "$(uname -s 2>/dev/null || echo unknown)" in
                    MINGW*|MSYS*|CYGWIN*) ;;
                    *) return "$direct_rc" ;;
                esac
                ;;
            *)
                return "$direct_rc"
                ;;
        esac
    fi

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) return 127 ;;
    esac
    command -v powershell.exe >/dev/null 2>&1 || return 127

    local cwd_native
    local bin_native
    local out_native
    local err_native
    local args_native=""
    local arg

    cwd_native="$(pgy_path_for_windows_tool "$cwd")"
    bin_native="$(pgy_path_for_windows_tool "$bin")"
    out_native="$(pgy_path_for_windows_tool "$out")"
    err_native="$(pgy_path_for_windows_tool "$err")"
    for arg in "$@"; do
        local escaped_arg="${arg//\"/\\\"}"
        args_native="${args_native} \"${escaped_arg}\""
    done

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; \$enc = New-Object System.Text.UTF8Encoding \$false; \$psi = New-Object System.Diagnostics.ProcessStartInfo; \$psi.FileName = $(pgy_powershell_quote "$bin_native"); \$psi.WorkingDirectory = $(pgy_powershell_quote "$cwd_native"); \$psi.UseShellExecute = \$false; \$psi.RedirectStandardOutput = \$true; \$psi.RedirectStandardError = \$true; \$psi.Arguments = $(pgy_powershell_quote "$args_native"); \$p = [System.Diagnostics.Process]::Start(\$psi); if (\$p -eq \$null) { exit 127 }; \$stdout = \$p.StandardOutput.ReadToEnd(); \$stderr = \$p.StandardError.ReadToEnd(); \$p.WaitForExit(); [System.IO.File]::WriteAllText($(pgy_powershell_quote "$out_native"), \$stdout, \$enc); [System.IO.File]::WriteAllText($(pgy_powershell_quote "$err_native"), \$stderr, \$enc); exit \$p.ExitCode"
}

# Fixture base names; each resolves to fixture/<base>.pgy and
# expected/<base>_stdout.txt.
FIXTURES=(
    hello
    two_logs
    concat
    nested_concat
    int_arith
    int_subdiv
    mixed_int_str
    int_neg
    while_sum
    if_else
    nested_ctrl
    func_call
    func_recursive
    result_int_core
    str_greet
    str_reassign
    for_sum
    for_continue
    while_break
    bool_logic
    str_builtins
    array_sum
    array_max
    array_combinators
    str_array
    str_array_concat
    str_indexof
    exit_guard
    array_push
    str_array_push
    str_trim
    io_probe
    args_probe
    struct_point
    struct_param
    array_param
    log_int_direct
    else_if_chain
    builtin_name_literal
    string_equality
    str_builtins2
    string_utils_core
    dir_walk
    for_each
    array_pop
    str_case_math
    string_concat_op
    write_file
    log_trailing_newline
    file_handle
    io_absolute_policy
    float_math
)

# Per-fixture runtime arguments. The same argv snapshot is passed to both the
# oracle and the self-host executable.
fixture_run_args() {
    local base="$1"
    case "$base" in
        args_probe)
            printf '%s\n' alpha beta gamma
            ;;
    esac
}

# Re-derive the oracle stdout and assert the committed expected has not drifted.
check_oracle_drift() {
    local base="$1"
    local src="$FIXTURE_DIR/${base}.pgy"
    local expected_file="$EXPECTED_DIR/${base}_stdout.txt"
    local oracle_exe="$ABS_BUILD/${base}_oracle.exe"

    if [[ ! -f "$src" ]]; then
        echo "[self-host-parity:codegen] missing fixture source: $src" >&2
        exit 1
    fi
    if [[ ! -f "$expected_file" ]]; then
        echo "[self-host-parity:codegen] missing expected stdout: $expected_file" >&2
        exit 1
    fi

    local oracle_compile_out="$ABS_BUILD/${base}_oracle_compile.out"
    local oracle_compile_err="$ABS_BUILD/${base}_oracle_compile.err"
    if ! run_native_capture "$ROOT_DIR" "$oracle_compile_out" "$oracle_compile_err" "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$src")" \
        --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_exe")"; then
        echo "[self-host-parity:codegen] $base: C-backend oracle failed to build" >&2
        cat "$oracle_compile_out" "$oracle_compile_err" >&2
        exit 1
    fi

    local run_args=()
    while IFS= read -r arg; do
        run_args+=("$arg")
    done < <(fixture_run_args "$base")

    local oracle_out
    local oracle_raw="$ABS_BUILD/${base}_oracle.out.raw"
    local oracle_err="$ABS_BUILD/${base}_oracle.err"
    # Run from ROOT_DIR so file-reading fixtures (ReadFile/FileExists) resolve
    # repo-relative paths deterministically.
    if ! run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe" "${run_args[@]}"; then
        echo "[self-host-parity:codegen] $base: C-backend oracle exit failed" >&2
        cat "$oracle_err" >&2
        exit 1
    fi
    oracle_out="$(tr -d '\r' < "$oracle_raw")"
    local expected_norm
    expected_norm="$(tr -d '\r' < "$expected_file")"
    if [[ "$oracle_out" != "$expected_norm" ]]; then
        echo "[self-host-parity:codegen] $base: committed expected drifted from C-backend oracle" >&2
        echo "regenerate: pgy <fixture> --backend=c -o oracle && oracle > $expected_file" >&2
        diff <(printf '%s\n' "$expected_norm") <(printf '%s\n' "$oracle_out") | head -20 >&2
        exit 1
    fi
}

compile_tool_backend() {
    local backend="$1"
    local tool_bin="$2"
    local compile_log="$ABS_BUILD/tool_${backend}.compile.log"
    local compile_out="$ABS_BUILD/tool_${backend}.compile.out"
    local compile_err="$ABS_BUILD/tool_${backend}.compile.err"

    echo "[self-host-parity:codegen] compiling codegen tool backend=$backend..."
    if ! run_native_capture "$ROOT_DIR" "$compile_out" "$compile_err" "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$tool_bin")"; then
        cat "$compile_out" "$compile_err" > "$compile_log"
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$compile_log"; then
            echo "[self-host-parity:codegen] LLVM backend unavailable; skipping llvm-compiled codegen tool"
            return 2
        fi
        echo "[self-host-parity:codegen] backend=$backend codegen tool failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
    cat "$compile_out" "$compile_err" > "$compile_log"
    return 0
}

run_tool_backend() {
    local backend="$1"
    local tool_bin="$2"

    for base in "${FIXTURES[@]}"; do
        local src="$FIXTURE_DIR/${base}.pgy"
        local expected_file="$EXPECTED_DIR/${base}_stdout.txt"
        local ast_rel="$REL_BUILD/${base}_${backend}_ast.txt"
        local ast_file="$ROOT_DIR/$ast_rel"
        local ast_raw="$ast_file.raw"
        local ast_err="$ast_file.err"
        local c_file="$ABS_BUILD/${base}_${backend}.c"
        local self_exe="$ABS_BUILD/${base}_${backend}_self.exe"

        # 1. AST text from the live compiler (written to a repo-relative path).
        if ! run_native_capture "$ROOT_DIR" "$ast_raw" "$ast_err" "$PGY" \
            --ast "$(pgy_path_for_compiler "$PGY" "$src")"; then
            echo "[self-host-parity:codegen] backend=$backend $base: --ast failed" >&2
            cat "$ast_err" >&2
            exit 1
        fi
        tr -d '\r' < "$ast_raw" > "$ast_file"
        if [[ ! -s "$ast_file" ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: empty --ast output" >&2
            cat "$ast_err" >&2
            exit 1
        fi

        # 2. Pergyra codegen tool: AST text -> C. The tool resolves its argument
        #    relative to cwd, so run it from ROOT_DIR with the repo-relative path.
        #    Capture the tool's own exit (not a pipe's) before stripping CRs.
        local tool_rc
        set +e
        run_native_capture "$ROOT_DIR" "$c_file.raw" "$c_file.err" "$tool_bin" "$ast_rel"
        tool_rc="$?"
        set -e
        tr -d '\r' < "$c_file.raw" > "$c_file"
        if [[ "$tool_rc" -ne 0 ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: codegen tool exit=$tool_rc" >&2
            cat "$c_file.err" "$c_file" >&2
            exit 1
        fi

        # 3. gcc the emitted C and run it.
        if ! "$CC" "$c_file" -o "$self_exe" 2>"$ABS_BUILD/${base}_${backend}_cc.log"; then
            echo "[self-host-parity:codegen] backend=$backend $base: emitted C failed to compile" >&2
            cat "$ABS_BUILD/${base}_${backend}_cc.log" >&2
            echo "--- emitted C ---" >&2
            cat "$c_file" >&2
            exit 1
        fi
        local run_args=()
        while IFS= read -r arg; do
            run_args+=("$arg")
        done < <(fixture_run_args "$base")

        local run_raw="$ABS_BUILD/${base}_${backend}_self.out.raw"
        local run_err="$ABS_BUILD/${base}_${backend}_self.err"
        local run_norm="$ABS_BUILD/${base}_${backend}_self.out"
        local run_rc
        set +e
        run_native_capture "$ROOT_DIR" "$run_raw" "$run_err" "$self_exe" "${run_args[@]}"
        run_rc="$?"
        set -e
        tr -d '\r' < "$run_raw" > "$run_norm"
        if [[ "$run_rc" -ne 0 ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: generated executable exit=$run_rc" >&2
            cat "$run_err" >&2
            exit 1
        fi
        local self_out
        self_out="$(cat "$run_norm")"

        # 4. Compare against committed expected (== oracle, guarded above).
        local expected_norm
        expected_norm="$(tr -d '\r' < "$expected_file")"
        if [[ "$self_out" != "$expected_norm" ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: RUN-STDOUT DRIFT vs $expected_file" >&2
            diff <(printf '%s\n' "$expected_norm") <(printf '%s\n' "$self_out") | head -20 >&2
            exit 1
        fi
    done

    echo "[self-host-parity:codegen] backend=$backend run-stdout equal (${#FIXTURES[@]} fixtures)"
}

for base in "${FIXTURES[@]}"; do
    check_oracle_drift "$base"
done

BACKENDS="${PGY_SELFHOST_CODEGEN_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()
for backend in $BACKENDS; do
    tool_bin="$ABS_BUILD/tool_${backend}.exe"
    set +e
    compile_tool_backend "$backend" "$tool_bin"
    compile_rc="$?"
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        SKIPPED_BACKENDS+=("$backend")
        continue
    fi
    if [[ "$compile_rc" -ne 0 ]]; then
        exit "$compile_rc"
    fi
    run_tool_backend "$backend" "$tool_bin"
    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:codegen] no requested backend ran" >&2
    exit 1
fi

BACKENDS_LABEL="${RAN_BACKENDS[*]}"
if [[ "${#SKIPPED_BACKENDS[@]}" -gt 0 ]]; then
    BACKENDS_LABEL="$BACKENDS_LABEL; ${SKIPPED_BACKENDS[*]} skipped"
fi

echo "[self-host-parity:codegen] rung-0..15 parity ok (${#FIXTURES[@]} fixtures; backends=$BACKENDS_LABEL)"
