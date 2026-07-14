#!/usr/bin/env bash
# Rung-0..19 parity for the Pergyra-origin C codegen substitute (2026-06-24).
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
#   2. self-host: self-parser <fixture> -> ast.txt; run the codegen tool on
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
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_role_parity_leg.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_reject_parity_leg.sh"
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
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:codegen" "$PGY"

CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[self-host-parity:codegen] SKIP missing C compiler on PATH: $CC"
    exit 0
fi
CODEGEN_JOBS="${PGY_SELFHOST_CODEGEN_JOBS:-2}"
if ! [[ "$CODEGEN_JOBS" =~ ^[1-9][0-9]*$ ]] || [[ "$CODEGEN_JOBS" -gt 4 ]]; then
    echo "[self-host-parity:codegen] PGY_SELFHOST_CODEGEN_JOBS must be 1..4" >&2
    exit 1
fi

# Build artifacts live under a repo-relative dir. The Pergyra codegen tool is a
# native binary that resolves its AST-path argument relative to cwd, so it is
# always invoked from ROOT_DIR with a repo-relative path (mirrors
# parser_parity.sh). gcc/run paths use the absolute form.
REL_BUILD=".tmp/self_hosted/codegen"
ABS_BUILD="$ROOT_DIR/$REL_BUILD"
HARNESS_PATHS_FILE="$ABS_BUILD/codegen_harness_paths.txt"
CODEGEN_FIXTURE_MANIFEST_FILE="$ABS_BUILD/codegen_fixture_manifest.txt"
PARSER_BIN="$ABS_BUILD/parser_ast_producer.exe"

mkdir -p "$ABS_BUILD"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:codegen" \
    "$ABS_BUILD" \
    "codegen-parity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 11 ]]; then
    echo "[self-host-parity:codegen] TestHarness manifest expected 11 codegen paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PARSER_SOURCE="$ROOT_DIR/${harness_paths[1]}"
COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[2]}"
FIXTURE_DIR="$ROOT_DIR/${harness_paths[3]}"
EXPECTED_DIR="$ROOT_DIR/${harness_paths[4]}"
REJECT_SOURCE="$ROOT_DIR/${harness_paths[5]}"
REJECT_EXPECTED="$ROOT_DIR/${harness_paths[6]}"
ROLE_SOURCE="$ROOT_DIR/${harness_paths[7]}"
ROLE_EXPECTED="$ROOT_DIR/${harness_paths[8]}"
EVENT_REJECT_SOURCE="$ROOT_DIR/${harness_paths[9]}"
EVENT_REJECT_EXPECTED="$ROOT_DIR/${harness_paths[10]}"

for path in "$TOOL_SOURCE" "$PARSER_SOURCE" "$COMPARATOR_SOURCE" \
    "$REJECT_SOURCE" "$REJECT_EXPECTED" "$ROLE_SOURCE" "$ROLE_EXPECTED" \
    "$EVENT_REJECT_SOURCE" "$EVENT_REJECT_EXPECTED"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:codegen] missing TestHarness input: $path" >&2
        exit 1
    fi
done
for dir in "$FIXTURE_DIR" "$EXPECTED_DIR"; do
    if [[ ! -d "$dir" ]]; then
        echo "[self-host-parity:codegen] missing TestHarness directory: $dir" >&2
        exit 1
    fi
done

COMPARATOR_BIN="$ABS_BUILD/backend_output_comparator.exe"

run_native_capture() {
    local cwd="$1"
    local out="$2"
    local err="$3"
    local bin="$4"
    shift 4

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            local cwd_bash
            local bin_bash
            local out_bash
            local err_bash
            cwd_bash="$(pgy_path_for_bash_tool "$cwd")"
            bin_bash="$(pgy_path_for_bash_tool "$bin")"
            out_bash="$(pgy_path_for_bash_tool "$out")"
            err_bash="$(pgy_path_for_bash_tool "$err")"
            local old_pwd="$PWD"
            cd "$cwd_bash"
            "$bin_bash" "$@" >"$out_bash" 2>"$err_bash"
            local rc=$?
            cd "$old_pwd"
            case "$rc" in
                126|127)
                    ;;
                *)
                    return "$rc"
                    ;;
            esac
            ;;
    esac

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
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; \$enc = New-Object System.Text.UTF8Encoding \$false; \$psi = New-Object System.Diagnostics.ProcessStartInfo; \$psi.FileName = $(pgy_powershell_quote "$bin_native"); \$psi.WorkingDirectory = $(pgy_powershell_quote "$cwd_native"); \$psi.UseShellExecute = \$false; \$psi.RedirectStandardOutput = \$true; \$psi.RedirectStandardError = \$true; \$psi.Arguments = $(pgy_powershell_quote "$args_native"); \$p = [System.Diagnostics.Process]::Start(\$psi); if (\$p -eq \$null) { exit 127 }; \$t1 = \$p.StandardOutput.ReadToEndAsync(); \$t2 = \$p.StandardError.ReadToEndAsync(); [System.Threading.Tasks.Task]::WaitAll(@(\$t1, \$t2)); \$p.WaitForExit(); \$stdout = \$t1.Result; \$stderr = \$t2.Result; [System.IO.File]::WriteAllText($(pgy_powershell_quote "$out_native"), \$stdout, \$enc); [System.IO.File]::WriteAllText($(pgy_powershell_quote "$err_native"), \$stderr, \$enc); exit \$p.ExitCode"
}

path_relative_to_root() {
    local path="$1"
    printf '%s\n' "${path#"$ROOT_DIR"/}"
}

compile_backend_output_comparator() {
    local compile_out="$ABS_BUILD/backend_output_comparator.compile.out"
    local compile_err="$ABS_BUILD/backend_output_comparator.compile.err"

    if ! run_native_capture "$ROOT_DIR" "$compile_out" "$compile_err" "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$COMPARATOR_SOURCE")" \
        --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$COMPARATOR_BIN")"; then
        echo "[self-host-parity:codegen] backend output comparator failed to build" >&2
        cat "$compile_out" "$compile_err" >&2
        exit 1
    fi
}

compile_parser_ast_producer() {
    local compile_out="$ABS_BUILD/parser_ast_producer.compile.out"
    local compile_err="$ABS_BUILD/parser_ast_producer.compile.err"

    echo "[self-host-parity:codegen] compiling parser AST producer backend=c..."
    if ! run_native_capture "$ROOT_DIR" "$compile_out" "$compile_err" "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$PARSER_SOURCE")" \
        --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$PARSER_BIN")"; then
        echo "[self-host-parity:codegen] parser AST producer failed to build" >&2
        cat "$compile_out" "$compile_err" >&2
        exit 1
    fi
}

compare_run_output_with_owner() {
    local backend="$1"
    local base="$2"
    local expected_file="$3"
    local actual_file="$4"
    local actual_projection="$5"
    local cmp_out="$ABS_BUILD/${base}_${backend}_compare.out"
    local cmp_err="$ABS_BUILD/${base}_${backend}_compare.err"
    local expected_rel
    local actual_rel

    expected_rel="$(path_relative_to_root "$expected_file")"
    actual_rel="$(path_relative_to_root "$actual_file")"

    if ! run_native_capture "$ROOT_DIR" "$cmp_out" "$cmp_err" "$COMPARATOR_BIN" \
        "$expected_rel" "$actual_rel" 0 "$actual_projection"; then
        echo "[self-host-parity:codegen] backend=$backend $base: RUN-STDOUT DRIFT vs $expected_file" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

generated_secure_open_probe_supported() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) return 1 ;;
    esac
    command -v ln >/dev/null 2>&1 || return 1
    command -v mktemp >/dev/null 2>&1 || return 1
    return 0
}

run_generated_secure_open_probe() {
    local backend="$1"
    local base="$2"
    local exe="$3"
    local target=""

    generated_secure_open_probe_supported || return 0

    case "$base" in
        write_file)
            target=".tmp/wf_fixture_test.txt"
            ;;
        file_handle)
            target=".tmp/fh_fixture.txt"
            ;;
        *)
            return 0
            ;;
    esac

    local probe_dir
    probe_dir="$(mktemp -d "$ABS_BUILD/${base}_${backend}_nofollow.XXXXXX")"
    mkdir -p "$probe_dir/.tmp"
    printf 'outside' >"$probe_dir/outside.txt"
    ln -s "../outside.txt" "$probe_dir/$target"

    local probe_out="$probe_dir/stdout.txt"
    local probe_err="$probe_dir/stderr.txt"
    if ! run_native_capture "$probe_dir" "$probe_out" "$probe_err" "$exe"; then
        echo "[self-host-parity:codegen] backend=$backend $base: secure-open symlink probe executable failed" >&2
        cat "$probe_err" >&2
        exit 1
    fi

    local outside_content
    outside_content="$(cat "$probe_dir/outside.txt")"
    if [[ "$outside_content" != "outside" ]]; then
        echo "[self-host-parity:codegen] backend=$backend $base: generated C followed a symlink write target" >&2
        echo "outside content: $outside_content" >&2
        exit 1
    fi
}

# Fixture base names are emitted by the compiled codegen owner.
FIXTURES=()

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
    done < <(fixture_run_args "$base") || true

    local oracle_raw="$ABS_BUILD/${base}_oracle.out.raw"
    local oracle_norm="$ABS_BUILD/${base}_oracle.out"
    local oracle_err="$ABS_BUILD/${base}_oracle.err"
    # Run from ROOT_DIR so file-reading fixtures (ReadFile/FileExists) resolve
    # repo-relative paths deterministically.
    if ! run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe" "${run_args[@]}"; then
        echo "[self-host-parity:codegen] $base: C-backend oracle exit failed" >&2
        cat "$oracle_err" >&2
        exit 1
    fi
    tr -d '\r' < "$oracle_raw" > "$oracle_norm"
    compare_run_output_with_owner "c-oracle" "$base" "$expected_file" "$oracle_norm" 0
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

read_codegen_fixture_manifest() {
    local manifest_bin="$ABS_BUILD/tool_manifest.exe"
    local line

    compile_tool_backend c "$manifest_bin"
    FIXTURES=()
    if ! run_native_capture "$ROOT_DIR" "$CODEGEN_FIXTURE_MANIFEST_FILE" "$ABS_BUILD/codegen_fixture_manifest.err" \
        "$manifest_bin" --fixture-manifest; then
        echo "[self-host-parity:codegen] fixture manifest emission failed" >&2
        cat "$ABS_BUILD/codegen_fixture_manifest.err" >&2
        exit 1
    fi

    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        FIXTURES+=("$line")
    done <"$CODEGEN_FIXTURE_MANIFEST_FILE"

    if [[ "${#FIXTURES[@]}" -ne 71 ]]; then
        echo "[self-host-parity:codegen] fixture manifest count drifted: ${#FIXTURES[@]} != 71" >&2
        exit 1
    fi

    if [[ -n "${PGY_SELFHOST_CODEGEN_FIXTURES:-}" ]]; then
        local selected=()
        local requested
        local requested_fixtures=()
        IFS=', ' read -r -a requested_fixtures \
            <<< "$PGY_SELFHOST_CODEGEN_FIXTURES"
        for requested in "${requested_fixtures[@]}"; do
            [[ -n "$requested" ]] || continue
            if ! printf '%s\n' "${FIXTURES[@]}" | grep -Fxq "$requested"; then
                echo "[self-host-parity:codegen] unknown fixture filter: $requested" >&2
                exit 1
            fi
            selected+=("$requested")
        done
        if [[ "${#selected[@]}" -eq 0 ]]; then
            echo "[self-host-parity:codegen] empty fixture filter" >&2
            exit 1
        fi
        FIXTURES=("${selected[@]}")
    fi
}

run_tool_fixture() {
    local backend="$1"
    local tool_bin="$2"
    local base="$3"

    local src="$FIXTURE_DIR/${base}.pgy"
    local expected_file="$EXPECTED_DIR/${base}_stdout.txt"
    local ast_rel="$REL_BUILD/${base}_${backend}_ast.txt"
    local ast_file="$ROOT_DIR/$ast_rel"
    local ast_raw="$ast_file.raw"
    local ast_err="$ast_file.err"
    local c_file="$ABS_BUILD/${base}_${backend}.c"
    local self_exe="$ABS_BUILD/${base}_${backend}_self.exe"

        # 1. AST text from the self-host parser (written to a repo-relative path).
        local src_rel
        src_rel="$(path_relative_to_root "$src")"
        if ! run_native_capture "$ROOT_DIR" "$ast_raw" "$ast_err" "$PARSER_BIN" "$src_rel"; then
            echo "[self-host-parity:codegen] backend=$backend $base: self-parser AST failed" >&2
            cat "$ast_err" >&2
            exit 1
        fi
        tr -d '\r' < "$ast_raw" > "$ast_file"
        if [[ ! -s "$ast_file" ]]; then
            echo "[self-host-parity:codegen] backend=$backend $base: empty self-parser AST output" >&2
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
        done < <(fixture_run_args "$base") || true

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

        # 4. Compare against committed expected (== oracle, guarded above).
        # The verdict is owned by the Pergyra backend-output comparator so
        # run-output artifact parity consumes ArtifactZone/TestHarness rows.
    compare_run_output_with_owner "$backend" "$base" "$expected_file" "$run_norm" 2
    run_generated_secure_open_probe "$backend" "$base" "$self_exe"
}

wait_fixture_batch() {
    local failed=0
    local pid
    for pid in "$@"; do
        if ! wait "$pid"; then
            failed=1
        fi
    done
    return "$failed"
}

run_tool_backend() {
    local backend="$1"
    local tool_bin="$2"
    local pids=()
    local base
    for base in "${FIXTURES[@]}"; do
        run_tool_fixture "$backend" "$tool_bin" "$base" &
        pids+=("$!")
        if [[ "${#pids[@]}" -ge "$CODEGEN_JOBS" ]]; then
            wait_fixture_batch "${pids[@]}" || exit 1
            pids=()
        fi
    done
    if [[ "${#pids[@]}" -gt 0 ]]; then
        wait_fixture_batch "${pids[@]}" || exit 1
    fi

    echo "[self-host-parity:codegen] backend=$backend run-stdout equal (${#FIXTURES[@]} fixtures)"
}

run_oracle_drift_checks() {
    local pids=()
    local base
    for base in "${FIXTURES[@]}"; do
        check_oracle_drift "$base" &
        pids+=("$!")
        if [[ "${#pids[@]}" -ge "$CODEGEN_JOBS" ]]; then
            wait_fixture_batch "${pids[@]}" || exit 1
            pids=()
        fi
    done
    if [[ "${#pids[@]}" -gt 0 ]]; then
        wait_fixture_batch "${pids[@]}" || exit 1
    fi
}

compile_backend_output_comparator
compile_parser_ast_producer
read_codegen_fixture_manifest
run_oracle_drift_checks

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
    run_codegen_reject_case \
        "$backend" "$tool_bin" "enum_payload_reject" \
        "$REJECT_SOURCE" "$REJECT_EXPECTED" "payload enum"
    run_codegen_reject_case \
        "$backend" "$tool_bin" "event_decl_reject" \
        "$EVENT_REJECT_SOURCE" "$EVENT_REJECT_EXPECTED" "event declaration"
    run_role_operator_parity "$backend" "$tool_bin"
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

printf '[self-host-parity:codegen] rung-0..21 parity ok: fixtures=%s backends=%s\n' \
    "${#FIXTURES[@]}" "$BACKENDS_LABEL"
