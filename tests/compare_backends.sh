#!/usr/bin/env bash
set -euo pipefail

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

files_equal() {
    local left="$1"
    local right="$2"

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import pathlib, sys
left = pathlib.Path(sys.argv[1]).read_bytes()
right = pathlib.Path(sys.argv[2]).read_bytes()
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi

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

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import difflib, pathlib, sys
left = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines(True)
right = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace").splitlines(True)
sys.stdout.writelines(difflib.unified_diff(left, right, fromfile=sys.argv[1], tofile=sys.argv[2]))
PY
        return 0
    fi

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
    return 0
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
        ;;
    *)
        TMP_BASE="${TMPDIR:-/tmp}"
        ;;
esac
WORK_ROOT="$ROOT_DIR/.tmp"
mkdir -p "$WORK_ROOT"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_PGY="${TMP_BASE%/}/pgy-$(basename "$ROOT_DIR")-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_BIN="$PGY_BIN"
else
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            if [[ -x "$TMP_PGY" && ( ! -x "$DEFAULT_PGY" || "$TMP_PGY" -nt "$DEFAULT_PGY" ) ]]; then
                PGY_BIN="$TMP_PGY"
            else
                PGY_BIN="$DEFAULT_PGY"
            fi
            ;;
        *)
            if [[ -x "$TMP_PGY" ]]; then
                PGY_BIN="$TMP_PGY"
            else
                PGY_BIN="$DEFAULT_PGY"
            fi
            ;;
    esac
fi
WORK_DIR="$(mktemp -d "$WORK_ROOT/pgy_backend_compare.XXXXXX")"

add_path_if_dir() {
    local dir="$1"
    [[ -d "$dir" ]] || return 0
    case ":${PATH}:" in
        *":$dir:"*) ;;
        *) PATH="$dir:$PATH" ;;
    esac
}

add_windows_path_candidate() {
    local dir="$1"
    local posix_dir=""

    [[ -n "$dir" ]] || return 0
    add_path_if_dir "$dir"
    if command -v cygpath >/dev/null 2>&1; then
        posix_dir="$(cygpath -u "$dir" 2>/dev/null || true)"
        add_path_if_dir "$posix_dir"
    fi
}

add_tool_dir_to_path() {
    local tool="$1"
    local tool_dir=""

    [[ -n "$tool" ]] || return 0
    tool_dir="$(dirname "$tool")"
    add_path_if_dir "$tool_dir"
    if command -v cygpath >/dev/null 2>&1; then
        tool_dir="$(cygpath -u "$tool_dir" 2>/dev/null || true)"
        add_path_if_dir "$tool_dir"
    fi
}

setup_windows_llvm_runtime_path() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            add_windows_path_candidate "${MSYSTEM_PREFIX:-}/bin"
            add_windows_path_candidate "${LLVM_INSTALL:-}"
            add_windows_path_candidate "${LLVM_INSTALL:-}/bin"
            add_path_if_dir "/c/Program Files/LLVM/bin"
            add_path_if_dir "/c/LLVM/bin"
            add_path_if_dir "/clang64/bin"
            add_path_if_dir "/ucrt64/bin"
            ;;
    esac
}

setup_windows_llvm_runtime_path

cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

setup_windows_launch_path() {
    local tool="$1"
    local cc_path=""
    local clang_path=""

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            add_tool_dir_to_path "$tool"
            add_tool_dir_to_path "$PGY_BIN"
            if cc_path="$(command -v cc 2>/dev/null)"; then
                add_tool_dir_to_path "$cc_path"
            fi
            if clang_path="$(command -v clang 2>/dev/null)"; then
                add_tool_dir_to_path "$clang_path"
            fi
            ;;
    esac
}

if [[ ! -x "$PGY_BIN" ]]; then
    echo "backend-compare: missing compiler binary: $PGY_BIN" >&2
    exit 1
fi

setup_windows_launch_path "$PGY_BIN"

if [[ "${PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS:-0}" != "0" ]]; then
    ABI_PIPELINE_BIN="${PGY_ABI_PIPELINE_TEST_BIN:-}"
    if [[ -z "$ABI_PIPELINE_BIN" ]]; then
        echo "backend-compare: PGY_ABI_PIPELINE_TEST_BIN is required when same-process precheck is enabled" >&2
        exit 1
    fi
    if [[ ! -x "$ABI_PIPELINE_BIN" && -x "${ABI_PIPELINE_BIN}.exe" ]]; then
        ABI_PIPELINE_BIN="${ABI_PIPELINE_BIN}.exe"
    fi
    if [[ ! -x "$ABI_PIPELINE_BIN" ]]; then
        echo "backend-compare: missing ABI pipeline test binary: $ABI_PIPELINE_BIN" >&2
        exit 1
    fi
    setup_windows_launch_path "$ABI_PIPELINE_BIN"
    if ! PGY_ABI_PIPELINE_SAME_PROCESS=1 \
        PGY_ABI_PIPELINE_BACKEND=llvm \
        "$ABI_PIPELINE_BIN"; then
        abi_rc=$?
        echo "backend-compare: ABI pipeline same-process precheck failed (exit=$abi_rc): $ABI_PIPELINE_BIN" >&2
        case "$(uname -s 2>/dev/null || echo unknown):$abi_rc" in
            MINGW*:126|MINGW*:127|MSYS*:126|MSYS*:127|CYGWIN*:126|CYGWIN*:127)
                echo "backend-compare: Windows executable launch failed; verify LLVM runtime DLL directories are on PATH" >&2
                ;;
        esac
        exit "$abi_rc"
    fi
fi

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

run_case() {
    local case_ref="$1"
    local case_name
    local case_src_dir=""
    local source_rel=""
    local source_arg
    local run_dir
    local c_bin
    local c_bin_arg
    local llvm_bin
    local llvm_bin_arg
    local c_compile_log
    local llvm_compile_log
    local c_out
    local llvm_out
    local c_err
    local llvm_err
    local c_rc=0
    local llvm_rc=0

    if [[ -d "$ROOT_DIR/$case_ref" ]]; then
        case_name="$(basename "$case_ref")"
        case_src_dir="$ROOT_DIR/$case_ref"
        source_rel="$case_ref/main.pgy"
        source_arg="$source_rel"
        run_dir="$case_src_dir"
    else
        source_rel="$case_ref"
        case_name="$(basename "$source_rel" .pgy)"
        source_arg="$source_rel"
        run_dir="$(dirname "$ROOT_DIR/$source_rel")"
    fi

    c_bin="$WORK_DIR/${case_name}_c"
    llvm_bin="$WORK_DIR/${case_name}_llvm"
    c_bin_arg="${c_bin#"$ROOT_DIR"/}"
    llvm_bin_arg="${llvm_bin#"$ROOT_DIR"/}"
    c_compile_log="$WORK_DIR/${case_name}_c.compile.log"
    llvm_compile_log="$WORK_DIR/${case_name}_llvm.compile.log"
    c_out="$WORK_DIR/${case_name}_c.stdout"
    llvm_out="$WORK_DIR/${case_name}_llvm.stdout"
    c_err="$WORK_DIR/${case_name}_c.stderr"
    llvm_err="$WORK_DIR/${case_name}_llvm.stderr"

    if ! (cd "$ROOT_DIR" && "$PGY_BIN" "$source_arg" --backend=c -o "$c_bin_arg") \
        >"$c_compile_log" 2>&1; then
        echo "backend-compare: C backend compile failed for $source_rel" >&2
        cat "$c_compile_log" >&2
        return 1
    fi

    if ! (cd "$ROOT_DIR" && "$PGY_BIN" "$source_arg" --backend=llvm -o "$llvm_bin_arg") \
        >"$llvm_compile_log" 2>&1; then
        echo "backend-compare: LLVM backend compile failed for $source_rel" >&2
        cat "$llvm_compile_log" >&2
        return 1
    fi

    c_bin="$(resolve_native_bin "$c_bin")"
    llvm_bin="$(resolve_native_bin "$llvm_bin")"
    setup_windows_launch_path "$c_bin"
    setup_windows_launch_path "$llvm_bin"

    if (cd "$run_dir" && "$c_bin" >"$c_out" 2>"$c_err"); then
        c_rc=0
    else
        c_rc=$?
    fi
    if (cd "$run_dir" && "$llvm_bin" >"$llvm_out" 2>"$llvm_err"); then
        llvm_rc=0
    else
        llvm_rc=$?
    fi

    case "$(uname -s 2>/dev/null || echo unknown):$c_rc:$llvm_rc" in
        MINGW*:126:*|MINGW*:127:*|MSYS*:126:*|MSYS*:127:*|CYGWIN*:126:*|CYGWIN*:127:*|MINGW*:*:126|MINGW*:*:127|MSYS*:*:126|MSYS*:*:127|CYGWIN*:*:126|CYGWIN*:*:127)
            echo "backend-compare: Windows executable launch failed for $source_rel; verify LLVM runtime DLL directories are on PATH" >&2
            echo "backend-compare: resolved C binary: $c_bin" >&2
            echo "backend-compare: resolved LLVM binary: $llvm_bin" >&2
            echo "backend-compare: C stderr:" >&2
            cat "$c_err" >&2 || true
            echo "backend-compare: LLVM stderr:" >&2
            cat "$llvm_err" >&2 || true
            echo "backend-compare: C compile log:" >&2
            cat "$c_compile_log" >&2 || true
            echo "backend-compare: LLVM compile log:" >&2
            cat "$llvm_compile_log" >&2 || true
            ;;
    esac

    if [[ "$c_rc" -ne "$llvm_rc" ]]; then
        echo "backend-compare: exit code mismatch for $source_rel (C=$c_rc LLVM=$llvm_rc)" >&2
        return 1
    fi

    if ! files_equal "$c_out" "$llvm_out"; then
        echo "backend-compare: stdout mismatch for $source_rel" >&2
        show_diff "$c_out" "$llvm_out" >&2
        return 1
    fi

    if ! files_equal "$c_err" "$llvm_err"; then
        echo "backend-compare: stderr mismatch for $source_rel" >&2
        show_diff "$c_err" "$llvm_err" >&2
        return 1
    fi

    echo "backend-compare: PASS $source_rel"
}

main() {
    local cases=(
        "tests/cases/backend_compare/basic"
        "tests/cases/backend_compare/slot_sugar"
        "tests/cases/backend_compare/break_continue"
        "tests/cases/backend_compare/array_enum"
        "tests/cases/backend_compare/destructure_array"
        "tests/cases/backend_compare/destructure_tuple_return"
        "tests/cases/backend_compare/slice_surface"
        "tests/cases/backend_compare/dynamic_array"
        "tests/cases/backend_compare/string_io"
        "tests/cases/backend_compare/module_namespace"
        "tests/cases/backend_compare/role_operator"
        "tests/cases/backend_compare/host_method_class_return"
        "tests/cases/backend_compare/boilerplate_reduction"
        "tests/cases/backend_compare/intent_decl_overlay"
        "tests/cases/backend_compare/intent_conflict_runtime"
        "tests/cases/backend_compare/intent_trace_compensate"
        "tests/cases/backend_compare/intent_failure_observability_strings"
        "tests/cases/backend_compare/intent_zone_binding"
        "tests/cases/backend_compare/intent_cross_world_transfer"
        "tests/cases/backend_compare/intent_rich_history_identity"
        "tests/cases/backend_compare/zone_param_mutation"
        "tests/cases/backend_compare/zone_host_method_abi_combo"
        "tests/cases/backend_compare/ownership_forwarding"
        "tests/cases/backend_compare/generic_default_contracts"
        "tests/cases/backend_compare/generic_multi_bound_defaults"
        "tests/cases/backend_compare/result_custom_error"
        "tests/cases/backend_compare/intent_header_interleaved"
        "tests/cases/backend_compare/map_keys"
        "tests/cases/backend_compare/world_zone_projection_visibility"
        "tests/cases/backend_compare/relation_effect_propagation"
        "tests/cases/backend_compare/higher_order_simple"
        "tests/cases/backend_compare/higher_order_compose"
        "tests/cases/backend_compare/parallel_channel_sum"
        "tests/cases/backend_compare/parallel_channel_dual"
        "tests/cases/backend_compare/triple_paradigm"
    )

    if [[ "$#" -gt 0 ]]; then
        cases=("$@")
    fi

    local -a failed=()
    local passed=0

    for source_rel in "${cases[@]}"; do
        # Run each case with failure-tolerance so one broken backend
        # parity doesn't mask the rest of the suite. `run_case` already
        # prints PASS/diff to stdout/stderr; we just track the tally.
        if run_case "$source_rel"; then
            passed=$((passed + 1))
        else
            failed+=("$source_rel")
        fi
    done

    local total=${#cases[@]}
    local fail_count=${#failed[@]}
    echo ""
    echo "backend-compare: summary — ${passed}/${total} passed, ${fail_count} failed"
    if (( fail_count > 0 )); then
        echo "backend-compare: failures:"
        for f in "${failed[@]}"; do
            echo "  - $f"
        done
        return 1
    fi
    return 0
}

main "$@"
