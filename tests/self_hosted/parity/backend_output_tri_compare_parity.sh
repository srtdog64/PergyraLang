#!/usr/bin/env bash
# Tri-parity smoke for backend outputs:
#   C backend output == LLVM backend output, and the Pergyra-origin
#   backend_output_comparator independently reports ok:true for that pair.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v grep >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:backend-tri-compare] missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_ROOT="$ROOT_DIR/.tmp"
mkdir -p "$WORK_ROOT"
WORK_DIR="$(mktemp -d "$WORK_ROOT/pgy_selfhost_tri_compare.XXXXXX")"
RUN_TIMEOUT_SECONDS="${PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS:-30}"

files_equal() {
    local left="$1"
    local right="$2"

    if command -v git >/dev/null 2>&1; then
        git diff --no-index --quiet -- "$left" "$right"
        return $?
    fi

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
        git --no-pager diff --no-index --no-prefix -- "$left" "$right" \
            || true
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
    return 0
}

cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# Probe LLVM backend availability. macOS C-only CI builds pgy with
# LLVM_ENABLED=0, and a pgy that lacks the LLVM backend cannot satisfy this
# tri-compare contract. SKIP gracefully so the parity script does not turn a
# build configuration into a self-host smoke failure.
LLVM_PROBE_SRC="$WORK_DIR/_pgy_llvm_probe.pgy"
LLVM_PROBE_BIN="$WORK_DIR/_pgy_llvm_probe_bin"
printf 'func Main() -> Void {}\n' > "$LLVM_PROBE_SRC"
LLVM_PROBE_REL="${LLVM_PROBE_SRC#"$ROOT_DIR/"}"
LLVM_PROBE_BIN_REL="${LLVM_PROBE_BIN#"$ROOT_DIR/"}"
if ! (cd "$ROOT_DIR" && "$PGY" "$LLVM_PROBE_REL" --backend=llvm \
        -o "$LLVM_PROBE_BIN_REL") >/dev/null 2>&1; then
    echo "[self-host-parity:backend-tri-compare] SKIP: LLVM backend unavailable in this pgy build"
    exit 0
fi

pgy_quote_ps() {
    local value="${1//\'/\'\'}"
    printf "'%s'" "$value"
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

run_windows_fallback() {
    local bin="$1"
    local out="$2"
    local err="$3"
    local bin_native
    local out_native
    local err_native
    local cwd_native
    local timeout_ms

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) return 127 ;;
    esac
    command -v powershell.exe >/dev/null 2>&1 || return 127

    bin_native="$(pgy_path_for_windows_tool "$bin")"
    out_native="$(pgy_path_for_windows_tool "$out")"
    err_native="$(pgy_path_for_windows_tool "$err")"
    cwd_native="$(pgy_path_for_windows_tool "$PWD")"
    timeout_ms=$((RUN_TIMEOUT_SECONDS * 1000))

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; Set-Location -LiteralPath $(pgy_quote_ps "$cwd_native"); \$p = Start-Process -FilePath $(pgy_quote_ps "$bin_native") -NoNewWindow -PassThru -RedirectStandardOutput $(pgy_quote_ps "$out_native") -RedirectStandardError $(pgy_quote_ps "$err_native"); if (\$p -eq \$null) { exit 127 }; if (-not \$p.WaitForExit(${timeout_ms})) { Stop-Process -Id \$p.Id -Force; exit 124 }; exit \$p.ExitCode"
}

run_pgy_windows_capture() {
    local cwd="$1"
    local out="$2"
    local err="$3"
    shift 3

    local bin_native
    local out_native
    local err_native
    local cwd_native
    local ps_args=""
    local arg

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) return 127 ;;
    esac
    command -v powershell.exe >/dev/null 2>&1 || return 127

    bin_native="$(pgy_path_for_windows_tool "$PGY")"
    out_native="$(pgy_path_for_windows_tool "$out")"
    err_native="$(pgy_path_for_windows_tool "$err")"
    cwd_native="$(pgy_path_for_windows_tool "$cwd")"

    for arg in "$@"; do
        ps_args="${ps_args} $(pgy_quote_ps "$arg")"
    done

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; Set-Location -LiteralPath $(pgy_quote_ps "$cwd_native"); & $(pgy_quote_ps "$bin_native")${ps_args} > $(pgy_quote_ps "$out_native") 2> $(pgy_quote_ps "$err_native"); exit \$LASTEXITCODE"
}

run_native_bin() {
    local bin="$1"
    local out="$2"
    local err="$3"
    local rc

    if command -v timeout >/dev/null 2>&1; then
        timeout "$RUN_TIMEOUT_SECONDS"s "$bin" >"$out" 2>"$err"
        rc=$?
    else
        "$bin" >"$out" 2>"$err"
        rc=$?
    fi
    if [[ "$rc" -eq 126 || "$rc" -eq 127 ]]; then
        run_windows_fallback "$bin" "$out" "$err"
        return $?
    fi
    return "$rc"
}

run_pergyra_output_compare() {
    local source_rel="$1"
    local stream_label="$2"
    local expected="$3"
    local actual="$4"
    local tri_root="$5"
    local tri_bin="$6"
    local tri_out
    local tri_stdout
    local tri_stderr
    local tri_rc
    local expected_arg
    local actual_arg

    mkdir -p "$tri_root/.tmp"

    expected_arg="${expected#"$ROOT_DIR/"}"
    actual_arg="${actual#"$ROOT_DIR/"}"
    tri_stdout="$tri_root/.tmp/${stream_label}.tri.stdout"
    tri_stderr="$tri_root/.tmp/${stream_label}.tri.stderr"
    set +e
    run_native_bin "$tri_bin" "$tri_stdout" "$tri_stderr" "$expected_arg" "$actual_arg" 0 1
    tri_rc=$?
    set -e
    tri_out="$(cat "$tri_stdout" "$tri_stderr")"
    if [[ "$tri_rc" -ne 0 ]]; then
        echo "[self-host-parity:backend-tri-compare] self-host comparator rejected matching C/LLVM ${stream_label} for $source_rel" >&2
        printf '%s\n' "$tri_out" >&2
        exit 1
    fi
    if ! grep -Fq '"schema":"pgy.selfhost.backend-output-comparator.v1"' <<<"$tri_out" \
        || ! grep -Fq '"ok":true' <<<"$tri_out"; then
        echo "[self-host-parity:backend-tri-compare] comparator output missing ok:true schema for ${stream_label} in $source_rel" >&2
        printf '%s\n' "$tri_out" >&2
        exit 1
    fi
    if ! files_equal "$expected" "$actual"; then
        echo "[self-host-parity:backend-tri-compare] shell sanity disagrees with self-host comparator for ${stream_label} in $source_rel" >&2
        show_diff "$expected" "$actual" >&2
        printf '%s\n' "$tri_out" >&2
        exit 1
    fi
}

run_tri_case() {
    local case_dir="$1"
    local case_name
    local source_rel
    local c_bin
    local llvm_bin
    local c_out
    local llvm_out
    local c_err
    local llvm_err
    local c_rc
    local llvm_rc
    local tri_root
    local tri_tool
    local tri_bin
    local tri_compile_log

    case_name="$(basename "$case_dir")"
    source_rel="$case_dir/main.pgy"
    if [[ ! -f "$ROOT_DIR/$source_rel" ]]; then
        echo "[self-host-parity:backend-tri-compare] missing backend compare source: $source_rel" >&2
        exit 1
    fi
    c_bin="$WORK_DIR/${case_name}_c"
    llvm_bin="$WORK_DIR/${case_name}_llvm"
    c_out="$WORK_DIR/${case_name}_c.stdout"
    llvm_out="$WORK_DIR/${case_name}_llvm.stdout"
    c_err="$WORK_DIR/${case_name}_c.stderr"
    llvm_err="$WORK_DIR/${case_name}_llvm.stderr"

    (cd "$ROOT_DIR" && "$PGY" "$source_rel" --backend=c -o "${c_bin#"$ROOT_DIR"/}") \
        >"$WORK_DIR/${case_name}_c.compile.log" 2>&1
    (cd "$ROOT_DIR" && "$PGY" "$source_rel" --backend=llvm -o "${llvm_bin#"$ROOT_DIR"/}") \
        >"$WORK_DIR/${case_name}_llvm.compile.log" 2>&1

    c_bin="$(resolve_native_bin "$c_bin")"
    llvm_bin="$(resolve_native_bin "$llvm_bin")"

    set +e
    (cd "$ROOT_DIR/$case_dir" && run_native_bin "$c_bin" "$c_out" "$c_err")
    c_rc=$?
    (cd "$ROOT_DIR/$case_dir" && run_native_bin "$llvm_bin" "$llvm_out" "$llvm_err")
    llvm_rc=$?
    set -e

    if [[ "$c_rc" -ne "$llvm_rc" ]]; then
        echo "[self-host-parity:backend-tri-compare] exit mismatch for $source_rel: C=$c_rc LLVM=$llvm_rc" >&2
        exit 1
    fi

    tri_root="$WORK_DIR/${case_name}_tri_root"
    tri_tool="$tri_root/src/self_hosted/tools/backend_output_comparator/main.pgy"
    tri_bin="$tri_root/.tmp/backend_output_comparator.exe"
    tri_compile_log="$tri_root/.tmp/backend_output_comparator.compile.log"
    mkdir -p "$(dirname "$tri_tool")"
    mkdir -p "$tri_root/src/self_hosted/lib"
    mkdir -p "$tri_root/src/self_hosted/compiler"
    mkdir -p "$tri_root/.tmp"
    cp "$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy" "$tri_tool"
    cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$tri_root/src/self_hosted/lib/"
    cp "$ROOT_DIR/src/self_hosted/compiler/artifact_zone_owner.pgy" \
        "$tri_root/src/self_hosted/compiler/artifact_zone_owner.pgy"
    cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_owner.pgy" \
        "$tri_root/src/self_hosted/compiler/test_harness_owner.pgy"
    cp "$ROOT_DIR/src/self_hosted/compiler/subprocess_runner_owner.pgy" \
        "$tri_root/src/self_hosted/compiler/subprocess_runner_owner.pgy"

    if ! (cd "$tri_root" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$tri_tool")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$tri_bin")" \
        >"$tri_compile_log" 2>&1); then
        echo "[self-host-parity:backend-tri-compare] comparator compile failed for $source_rel" >&2
        cat "$tri_compile_log" >&2
        exit 1
    fi

    run_pergyra_output_compare "$source_rel" "stdout" "$c_out" "$llvm_out" \
        "$tri_root" "$tri_bin"
    run_pergyra_output_compare "$source_rel" "stderr" "$c_err" "$llvm_err" \
        "$tri_root" "$tri_bin"
}

if [[ "$#" -gt 0 ]]; then
    cases=("$@")
else
    cases=(
        "tests/cases/backend_compare/basic"
        "tests/cases/backend_compare/arith_grand_total"
        "tests/cases/backend_compare/array_builtins"
        "tests/cases/backend_compare/allocator_lane_boxarray"
        "tests/cases/backend_compare/allocator_defer_cleanup"
        "tests/cases/backend_compare/class_factory_field_method"
        "tests/cases/backend_compare/channel_send_recv_basic"
        "tests/cases/backend_compare/async_spawn_await"
        "tests/cases/backend_compare/slice_surface"
        "tests/cases/backend_compare/slice_copy"
        "tests/cases/backend_compare/string_interpolation"
        "tests/cases/backend_compare/pin_write_view_block"
        "tests/cases/backend_compare/secure_slot_view"
        "tests/cases/backend_compare/intent_zone_binding"
    )
    if [[ "${PGY_BACKEND_TRI_COMPARE_SUITE:-smoke}" == "extended" ]]; then
        cases+=(
            "tests/cases/backend_compare/file_handle_io"
            "tests/cases/backend_compare/io_string_negative_paths"
            "tests/cases/backend_compare/rc_weak_lifecycle"
            "tests/cases/backend_compare/hashmap_basic_ops"
            "tests/cases/backend_compare/list_mutation_ops"
            "tests/cases/backend_compare/queue_state_ops"
            "tests/cases/backend_compare/set_membership_ops"
            "tests/cases/backend_compare/try_operator_result"
            "tests/cases/backend_compare/future_cancel_state"
            "tests/cases/backend_compare/select_single_ready"
            "tests/cases/backend_compare/unsafe_lexical_boundary"
            "tests/cases/backend_compare/top_level_visibility_decl"
            "tests/cases/backend_compare/event_system"
            "tests/cases/backend_compare/generic_future_spawn_multi_arg"
            "tests/cases/backend_compare/runtime_seeded_random"
            "tests/cases/backend_compare/map_ops"
            "tests/cases/backend_compare/zone_host_method_abi_combo"
        )
    fi
fi

for case_dir in "${cases[@]}"; do
    run_tri_case "$case_dir"
done

echo "[self-host-parity:backend-tri-compare] C/LLVM stdout/stderr accepted by Pergyra comparator (${#cases[@]} cases)"
