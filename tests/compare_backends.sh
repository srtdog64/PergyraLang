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
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"
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
RUN_TIMEOUT_SECONDS="${PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS:-30}"

add_path_if_dir() {
    local dir="$1"
    [[ -d "$dir" ]] || return 0
    case ":${PATH}:" in
        *":$dir:"*) ;;
        *) PATH="$dir:$PATH" ;;
    esac
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

pgy_prepend_windows_runtime_paths

normalize_executable_path() {
    local path="$1"

    if [[ -n "$path" && -x "$path" ]] \
        && command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$path" 2>/dev/null || printf '%s\n' "$path"
        return 0
    fi
    printf '%s\n' "$path"
}

cleanup() {
    if [[ "${PGY_BACKEND_COMPARE_KEEP_WORK:-0}" != "0" ]]; then
        echo "backend-compare: keeping work dir $WORK_DIR" >&2
        return 0
    fi
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

run_abi_pipeline_precheck() {
    PGY_ABI_PIPELINE_SAME_PROCESS=1 \
        PGY_ABI_PIPELINE_BACKEND=llvm \
        "$ABI_PIPELINE_BIN"
}

run_windows_abi_pipeline_precheck_fallback() {
    local abi_native="$ABI_PIPELINE_BIN"

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) return 127 ;;
    esac
    command -v powershell.exe >/dev/null 2>&1 || return 127
    abi_native="$(pgy_path_for_windows_tool "$ABI_PIPELINE_BIN")"

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; \$env:PGY_ABI_PIPELINE_SAME_PROCESS='1'; \$env:PGY_ABI_PIPELINE_BACKEND='llvm'; & '${abi_native}'; exit \$LASTEXITCODE"
}

pgy_powershell_quote() {
    local value="${1//\'/\'\'}"
    printf "'%s'" "$value"
}

if [[ "${PGY_BACKEND_COMPARE_INVENTORY_ONLY:-0}" == "0"
    && ! -x "$PGY_BIN" ]]; then
    echo "backend-compare: missing compiler binary: $PGY_BIN" >&2
    exit 1
fi
if [[ -x "$PGY_BIN" ]]; then
    PGY_BIN="$(normalize_executable_path "$PGY_BIN")"
fi
export PGY_BIN

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
    ABI_PIPELINE_BIN="$(normalize_executable_path "$ABI_PIPELINE_BIN")"
    setup_windows_launch_path "$ABI_PIPELINE_BIN"
    set +e
    run_abi_pipeline_precheck
    abi_rc=$?
    if [[ "$abi_rc" -eq 126 || "$abi_rc" -eq 127 ]]; then
        run_windows_abi_pipeline_precheck_fallback
        abi_rc=$?
    fi
    set -e
    if [[ "$abi_rc" -ne 0 ]]; then
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

run_native_binary() {
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
        run_windows_native_binary_fallback "$bin" "$out" "$err"
        return $?
    fi
    return "$rc"
}

run_windows_native_binary_fallback() {
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
        "\$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; Set-Location -LiteralPath $(pgy_powershell_quote "$cwd_native"); \$p = Start-Process -FilePath $(pgy_powershell_quote "$bin_native") -NoNewWindow -PassThru -RedirectStandardOutput $(pgy_powershell_quote "$out_native") -RedirectStandardError $(pgy_powershell_quote "$err_native"); if (\$p -eq \$null) { exit 127 }; if (-not \$p.WaitForExit(${timeout_ms})) { Stop-Process -Id \$p.Id -Force; exit 124 }; exit \$p.ExitCode"
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

    if (cd "$run_dir" && run_native_binary "$c_bin" "$c_out" "$c_err"); then
        c_rc=0
    else
        c_rc=$?
    fi
    if (cd "$run_dir" && run_native_binary "$llvm_bin" "$llvm_out" "$llvm_err"); then
        llvm_rc=0
    else
        llvm_rc=$?
    fi

    if [[ "$c_rc" -eq 124 || "$llvm_rc" -eq 124 ]]; then
        echo "backend-compare: execution timeout for $source_rel after ${RUN_TIMEOUT_SECONDS}s (C=$c_rc LLVM=$llvm_rc)" >&2
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
        "tests/cases/backend_compare/entry_lowercase_main"
        "tests/cases/backend_compare/extern_fn"
        "tests/cases/backend_compare/slot_basic"
        "tests/cases/backend_compare/slot_sugar"
        "tests/cases/backend_compare/slot_subject_cell"
        "tests/cases/backend_compare/secure_slot_subject_cell"
        "tests/cases/backend_compare/secure_slot_subject_bot"
        "tests/cases/backend_compare/slot_subject_boundary_ref"
        "tests/cases/backend_compare/secure_slot_subject_boundary_own"
        "tests/cases/backend_compare/secure_slot_subject_boundary_forward_own"
        "tests/cases/backend_compare/break_continue"
        "tests/cases/backend_compare/else_if_chain"
        "tests/cases/backend_compare/if_else_chain"
        "tests/cases/backend_compare/while_loop"
        "tests/cases/backend_compare/while_condition_basic"
        "tests/cases/backend_compare/array_index_loop_sum"
        "tests/cases/backend_compare/array_enum"
        "tests/cases/backend_compare/array_builtins"
        "tests/cases/backend_compare/array_inline_access"
        "tests/cases/backend_compare/destructure_array"
        "tests/cases/backend_compare/destructure_tuple_return"
        "tests/cases/backend_compare/tuple_literal_local"
        "tests/cases/backend_compare/tagged_union"
        "tests/cases/backend_compare/match_stmt"
        "tests/cases/backend_compare/enum_match_variant"
        "tests/cases/backend_compare/match_guard_or_pattern"
        "tests/cases/backend_compare/slice_surface"
        "tests/cases/backend_compare/slice_copy"
        "tests/cases/backend_compare/slice_inline_access"
        "tests/cases/backend_compare/slice_range_syntax"
        "tests/cases/backend_compare/dynamic_array"
        "tests/cases/backend_compare/dynamic_array_ops"
        "tests/cases/backend_compare/numeric_unary_minus"
        "tests/cases/backend_compare/float_unary_minus"
        "tests/cases/backend_compare/int_abs_min_max"
        "tests/cases/backend_compare/int_mod_div_signed"
        "tests/cases/backend_compare/mod_branch_arith"
        "tests/cases/backend_compare/bool_negate_branch"
        "tests/cases/backend_compare/nested_if_returns"
        "tests/cases/backend_compare/to_string_signed_numbers"
        "tests/cases/backend_compare/string_escape_sequences"
        "tests/cases/backend_compare/empty_array_basics"
        "tests/cases/backend_compare/mixed_param_types"
        "tests/cases/backend_compare/long_arithmetic"
        "tests/cases/backend_compare/long_subtract_workaround"
        "tests/cases/backend_compare/bool_to_string_concat"
        "tests/cases/backend_compare/map_function_helper"
        "tests/cases/backend_compare/generic_identity_multi"
        "tests/cases/backend_compare/set_string_ops"
        "tests/cases/backend_compare/class_method_self_access"
        "tests/cases/backend_compare/class_chain_methods"
        "tests/cases/backend_compare/class_method_enum_classify"
        "tests/cases/backend_compare/class_method_coalesce_call"
        "tests/cases/backend_compare/class_method_short_circuit"
        "tests/cases/backend_compare/class_compose_helper"
        "tests/cases/backend_compare/lexical_shadow_class_method"
        "tests/cases/backend_compare/list_shadow_scope_metadata"
        "tests/cases/backend_compare/nested_loop_break"
        "tests/cases/backend_compare/function_returning_array"
        "tests/cases/backend_compare/if_expression_in_let"
        "tests/cases/backend_compare/struct_field_access"
        "tests/cases/backend_compare/array_of_strings_loop"
        "tests/cases/backend_compare/else_if_int_classify"
        "tests/cases/backend_compare/option_match_simple"
        "tests/cases/backend_compare/option_same_binding_guard"
        "tests/cases/backend_compare/option_multi_same_binding_loop"
        "tests/cases/backend_compare/option_nested_same_binding_shadow"
        "tests/cases/backend_compare/match_nested_same_binding_shadow"
        "tests/cases/backend_compare/continue_skip_odd"
        "tests/cases/backend_compare/sum_to_n_recursive"
        "tests/cases/backend_compare/recursive_funcs"
        "tests/cases/backend_compare/recursive_int_pow"
        "tests/cases/backend_compare/count_until_threshold"
        "tests/cases/backend_compare/count_heavy_inline"
        "tests/cases/backend_compare/string_arg_branch_call"
        "tests/cases/backend_compare/division_zero_check"
        "tests/cases/backend_compare/class_field_init_order"
        "tests/cases/backend_compare/for_in_array_int"
        "tests/cases/backend_compare/for_in_array_with_branches"
        "tests/cases/backend_compare/nested_array_subarray"
        "tests/cases/backend_compare/float_to_string_precision"
        "tests/cases/backend_compare/map_key_lookup_branch"
        "tests/cases/backend_compare/queue_string_ops"
        "tests/cases/backend_compare/list_int_loop"
        "tests/cases/backend_compare/array_min_max_combined"
        "tests/cases/backend_compare/array_running_max"
        "tests/cases/backend_compare/array_prefix_sum"
        "tests/cases/backend_compare/array_element_assign"
        "tests/cases/backend_compare/array_reverse_in_place"
        "tests/cases/backend_compare/array_set_in_place_memo"
        "tests/cases/backend_compare/array_selection_sort"
        "tests/cases/backend_compare/compose_two_functions"
        "tests/cases/backend_compare/negative_index_check"
        "tests/cases/backend_compare/multi_return_paths"
        "tests/cases/backend_compare/bool_expr_chain"
        "tests/cases/backend_compare/and_or_mix_chain_branches"
        "tests/cases/backend_compare/bool_short_circuit_chain"
        "tests/cases/backend_compare/bool_compound_predicates"
        "tests/cases/backend_compare/fibonacci_iterative"
        "tests/cases/backend_compare/fib_iterative"
        "tests/cases/backend_compare/fib_memo_class"
        "tests/cases/backend_compare/fib_memo_pair"
        "tests/cases/backend_compare/map_count_unique"
        "tests/cases/backend_compare/result_via_unwrap"
        "tests/cases/backend_compare/string_compare_branch"
        "tests/cases/backend_compare/if_short_circuit_pure"
        "tests/cases/backend_compare/for_range_explicit"
        "tests/cases/backend_compare/for_same_binding_sequence"
        "tests/cases/backend_compare/for_nested_same_binding_shadow"
        "tests/cases/backend_compare/option_param_pass"
        "tests/cases/backend_compare/subject_class_pair"
        "tests/cases/backend_compare/nested_match_int"
        "tests/cases/backend_compare/string_split_simple"
        "tests/cases/backend_compare/sum_filter_loop"
        "tests/cases/backend_compare/substring_extract"
        "tests/cases/backend_compare/list_push_get_loop"
        "tests/cases/backend_compare/map_long_values"
        "tests/cases/backend_compare/set_intersection_manual"
        "tests/cases/backend_compare/string_starts_with_prefix"
        "tests/cases/backend_compare/loop_collect_max"
        "tests/cases/backend_compare/sort_three_ints"
        "tests/cases/backend_compare/count_letters_in_word"
        "tests/cases/backend_compare/binary_search_int"
        "tests/cases/backend_compare/gcd_recursive"
        "tests/cases/backend_compare/gcd_recursive_ish"
        "tests/cases/backend_compare/reverse_array_in_place"
        "tests/cases/backend_compare/palindrome_check"
        "tests/cases/backend_compare/bubble_sort_small"
        "tests/cases/backend_compare/fizzbuzz_loop"
        "tests/cases/backend_compare/multi_array_find"
        "tests/cases/backend_compare/bool_state_toggle"
        "tests/cases/backend_compare/primes_below_n"
        "tests/cases/backend_compare/sieve_eratosthenes_like"
        "tests/cases/backend_compare/grid_sum_2d_emulated"
        "tests/cases/backend_compare/string_repeat_pattern"
        "tests/cases/backend_compare/linear_search_first_match"
        "tests/cases/backend_compare/map_word_grouping"
        "tests/cases/backend_compare/string_to_int_parse"
        "tests/cases/backend_compare/queue_workload_sim"
        "tests/cases/backend_compare/binary_to_int"
        "tests/cases/backend_compare/sliding_window_sum"
        "tests/cases/backend_compare/matrix_transpose_3x2"
        "tests/cases/backend_compare/loop_collect_evens"
        "tests/cases/backend_compare/reverse_string_loop"
        "tests/cases/backend_compare/dedup_array_first_seen"
        "tests/cases/backend_compare/map_array_value"
        "tests/cases/backend_compare/count_words_in_sentence"
        "tests/cases/backend_compare/inclusive_range_sum"
        "tests/cases/backend_compare/histogram_bins"
        "tests/cases/backend_compare/map_value_sum"
        "tests/cases/backend_compare/two_pointer_sum_pair"
        "tests/cases/backend_compare/two_pointer_check"
        "tests/cases/backend_compare/count_negatives_loop"
        "tests/cases/backend_compare/loop_count_negative_zero"
        "tests/cases/backend_compare/loop_accumulate_int"
        "tests/cases/backend_compare/string_alternating_case"
        "tests/cases/backend_compare/digit_count_int"
        "tests/cases/backend_compare/digit_sum_recursive"
        "tests/cases/backend_compare/zip_arrays_to_pairs"
        "tests/cases/backend_compare/find_min_in_array"
        "tests/cases/backend_compare/count_vowels"
        "tests/cases/backend_compare/factorial_iterative"
        "tests/cases/backend_compare/sum_squares_loop"
        "tests/cases/backend_compare/sum_positive_array"
        "tests/cases/backend_compare/array_sum_filtered"
        "tests/cases/backend_compare/set_member_pipeline"
        "tests/cases/backend_compare/map_branch_threshold"
        "tests/cases/backend_compare/sum_until_target"
        "tests/cases/backend_compare/queue_levelorder_sim"
        "tests/cases/backend_compare/linked_check_string_split"
        "tests/cases/backend_compare/powers_of_two_table"
        "tests/cases/backend_compare/insertion_sort_array"
        "tests/cases/backend_compare/string_compress_runlength"
        "tests/cases/backend_compare/loop_collect_distinct_set"
        "tests/cases/backend_compare/count_consecutive_sequence"
        "tests/cases/backend_compare/loop_running_avg"
        "tests/cases/backend_compare/triangle_number_sum"
        "tests/cases/backend_compare/matrix_diag_sum"
        "tests/cases/backend_compare/sort_descending_loop"
        "tests/cases/backend_compare/bubble_sort_inline"
        "tests/cases/backend_compare/compound_interest_loop"
        "tests/cases/backend_compare/frequency_dominant"
        "tests/cases/backend_compare/count_pairs_sum_equal"
        "tests/cases/backend_compare/hex_digit_lookup"
        "tests/cases/backend_compare/sequential_array_diff"
        "tests/cases/backend_compare/check_all_positive"
        "tests/cases/backend_compare/rotate_array_left"
        "tests/cases/backend_compare/string_balanced_parens"
        "tests/cases/backend_compare/interval_overlap_count"
        "tests/cases/backend_compare/trapezoid_area"
        "tests/cases/backend_compare/caesar_shift_decode"
        "tests/cases/backend_compare/float_min_max_clamp"
        "tests/cases/backend_compare/bool_short_circuit_calls"
        "tests/cases/backend_compare/nested_short_circuit_loop"
        "tests/cases/backend_compare/short_circuit_guard_chain"
        "tests/cases/backend_compare/short_circuit_inside_loop_join"
        "tests/cases/backend_compare/long_unary_minus"
        "tests/cases/backend_compare/substring_equality_inline"
        "tests/cases/backend_compare/arith_grand_total"
        "tests/cases/backend_compare/int_clamp_with_offset"
        "tests/cases/backend_compare/string_count_chars"
        "tests/cases/backend_compare/string_count_subword"
        "tests/cases/backend_compare/string_format_tens"
        "tests/cases/backend_compare/string_capitalize_via_concat"
        "tests/cases/backend_compare/enum_match_destructure"
        "tests/cases/backend_compare/enum_construct_in_loop_match"
        "tests/cases/backend_compare/enum_in_array_loop"
        "tests/cases/backend_compare/enum_match_case_inner_or"
        "tests/cases/backend_compare/enum_match_in_loop_with_short_circuit"
        "tests/cases/backend_compare/enum_match_chain_assign"
        "tests/cases/backend_compare/enum_match_with_string_payload"
        "tests/cases/backend_compare/enum_no_payload_first_then_payload"
        "tests/cases/backend_compare/enum_payload_first_then_no_payload"
        "tests/cases/backend_compare/enum_payload_sandwich"
        "tests/cases/backend_compare/enum_signal_with_idle_first"
        "tests/cases/backend_compare/enum_state_transition_chain"
        "tests/cases/backend_compare/enum_wildcard_discard_match"
        "tests/cases/backend_compare/enum_seq_apply"
        "tests/cases/backend_compare/enum_in_function_arg_chain"
        "tests/cases/backend_compare/enum_with_string_id"
        "tests/cases/backend_compare/enum_with_multi_arity"
        "tests/cases/backend_compare/enum_option_payload"
        "tests/cases/backend_compare/option_enum_payload"
        "tests/cases/backend_compare/option_3enums_mixed"
        "tests/cases/backend_compare/option_chain_with_enum"
        "tests/cases/backend_compare/enum_no_payload_variant"
        "tests/cases/backend_compare/enum_state_machine"
        "tests/cases/backend_compare/bitwise_via_division"
        "tests/cases/backend_compare/enum_traffic_light"
        "tests/cases/backend_compare/enum_branch_chain"
        "tests/cases/backend_compare/enum_step_progression"
        "tests/cases/backend_compare/nested_match_enum"
        "tests/cases/backend_compare/enum_calculator"
        "tests/cases/backend_compare/enum_color_mix"
        "tests/cases/backend_compare/enum_priority_queue"
        "tests/cases/backend_compare/result_basic_chain"
        "tests/cases/backend_compare/result_branchy_chain"
        "tests/cases/backend_compare/result_chain_processing"
        "tests/cases/backend_compare/generic_box_int"
        "tests/cases/backend_compare/arithmetic_overflow_check"
        "tests/cases/backend_compare/nested_match_state"
        "tests/cases/backend_compare/nested_enum_chain"
        "tests/cases/backend_compare/match_with_inner_short_circuit"
        "tests/cases/backend_compare/nested_if_else_chain_int"
        "tests/cases/backend_compare/option_chain_compute"
        "tests/cases/backend_compare/phi_branch_value"
        "tests/cases/backend_compare/subject_method_chain"
        "tests/cases/backend_compare/result_chain_match"
        "tests/cases/backend_compare/result_err_enum_nested_match"
        "tests/cases/backend_compare/result_payload_propagation"
        "tests/cases/backend_compare/result_string_err_enum_ok"
        "tests/cases/backend_compare/try_chain_enum_err"
        "tests/cases/backend_compare/operators"
        "tests/cases/backend_compare/scalar_numeric_widening"
        "tests/cases/backend_compare/float_compare_branches"
        "tests/cases/backend_compare/float_arith_chain"
        "tests/cases/backend_compare/scalar_math_runtime"
        "tests/cases/backend_compare/scalar_parse_conversion"
        "tests/cases/backend_compare/scalar_trig_log_runtime"
        "tests/cases/backend_compare/scalar_utility_builtins"
        "tests/cases/backend_compare/io_print_banner"
        "tests/cases/backend_compare/file_handle_io"
        "tests/cases/backend_compare/runtime_time_sleep"
        "tests/cases/backend_compare/device_slot_remote"
        "tests/cases/backend_compare/runtime_seeded_random"
        "tests/cases/backend_compare/string_io"
        "tests/cases/backend_compare/io_string_negative_paths"
        "tests/cases/backend_compare/string_interpolation"
        "tests/cases/backend_compare/string_utility_aliases"
        "tests/cases/backend_compare/string_concat"
        "tests/cases/backend_compare/string_join"
        "tests/cases/backend_compare/string_split_count"
        "tests/cases/backend_compare/string_split_edge"
        "tests/cases/backend_compare/string_starts_ends_predicate"
        "tests/cases/backend_compare/multi_types"
        "tests/cases/backend_compare/module_namespace"
        "tests/cases/backend_compare/namespace_export_import"
        "tests/cases/backend_compare/top_level_visibility_decl"
        "tests/cases/backend_compare/pipeline_composition"
        "tests/cases/backend_compare/role_operator"
        "tests/cases/backend_compare/operator_overload"
        "tests/cases/backend_compare/role_operator_overload"
        "tests/cases/backend_compare/free_function_recursion"
        "tests/cases/backend_compare/recursion"
        "tests/cases/backend_compare/mutual_recursion"
        "tests/cases/backend_compare/nested_function_calls"
        "tests/cases/backend_compare/nested_calls"
        "tests/cases/backend_compare/host_method_class_return"
        "tests/cases/backend_compare/subject_method_recursion_defer"
        "tests/cases/backend_compare/branch_defer_scope"
        "tests/cases/backend_compare/branch_defer_skipped"
        "tests/cases/backend_compare/defer_scope_exit"
        "tests/cases/backend_compare/loop_defer_break_current"
        "tests/cases/backend_compare/loop_defer_continue_current"
        "tests/cases/backend_compare/boilerplate_reduction"
        "tests/cases/backend_compare/intent_decl_overlay"
        "tests/cases/backend_compare/intent_conflict_runtime"
        "tests/cases/backend_compare/intent_trace_compensate"
        "tests/cases/backend_compare/intent_failure_result"
        "tests/cases/backend_compare/intent_failure_observability_strings"
        "tests/cases/backend_compare/intent_observability_rollback"
        "tests/cases/backend_compare/intent_zone_binding"
        "tests/cases/backend_compare/intent_cross_world_transfer"
        "tests/cases/backend_compare/intent_rich_history_identity"
        "tests/cases/backend_compare/intent_authority_snapshot"
        "tests/cases/backend_compare/handoff_projection_frontier"
        "tests/cases/backend_compare/handoff_world_state_frontier"
        "tests/cases/backend_compare/handoff_layer_state_frontier"
        "tests/cases/backend_compare/zone_param_mutation"
        "tests/cases/backend_compare/zone_host_method_abi_combo"
        "tests/cases/backend_compare/subject_projection"
        "tests/cases/backend_compare/subject_class_dispatch"
        "tests/cases/backend_compare/action_outcome_dispatch"
        "tests/cases/backend_compare/subject_action_state"
        "tests/cases/backend_compare/object_layer_binding"
        "tests/cases/backend_compare/ownership_forwarding"
        "tests/cases/backend_compare/generic_future_spawn_int"
        "tests/cases/backend_compare/generic_future_spawn_multi_arg"
        "tests/cases/backend_compare/generic_future_spawn_mixed"
        "tests/cases/backend_compare/generic_future_spawn_string"
        "tests/cases/backend_compare/generic_spawn"
        "tests/cases/backend_compare/generic_spawn_multi"
        "tests/cases/backend_compare/generic_call"
        "tests/cases/backend_compare/generic_default_contracts"
        "tests/cases/backend_compare/generic_multi_bound_defaults"
        "tests/cases/backend_compare/nested_generic_containers"
        "tests/cases/backend_compare/forward_ability_order"
        "tests/cases/backend_compare/role_include_methods"
        "tests/cases/backend_compare/party_role_bind"
        "tests/cases/backend_compare/party_role_bind_dispatch"
        "tests/cases/backend_compare/party_roster_host_methods"
        "tests/cases/backend_compare/result_custom_error"
        "tests/cases/backend_compare/result_class_chain_methods"
        "tests/cases/backend_compare/result_class_method_call"
        "tests/cases/backend_compare/result_class_method_err"
        "tests/cases/backend_compare/result_classpair_ok"
        "tests/cases/backend_compare/result_enum_ok_enum_err"
        "tests/cases/backend_compare/result_method_propagating"
        "tests/cases/backend_compare/result_method_with_self_field"
        "tests/cases/backend_compare/method_returning_result"
        "tests/cases/backend_compare/result_two_classes"
        "tests/cases/backend_compare/option_coalesce"
        "tests/cases/backend_compare/coalesce_accumulate_loop"
        "tests/cases/backend_compare/coalesce_in_if_condition"
        "tests/cases/backend_compare/nested_coalesce_chain"
        "tests/cases/backend_compare/option_chain_in_loop_cond"
        "tests/cases/backend_compare/option_chain_with_coalesce_chain"
        "tests/cases/backend_compare/option_acc_chain"
        "tests/cases/backend_compare/option_branch_nested"
        "tests/cases/backend_compare/option_class_chain_call"
        "tests/cases/backend_compare/option_class_field_addr"
        "tests/cases/backend_compare/option_class_field_holder"
        "tests/cases/backend_compare/option_class_method_call"
        "tests/cases/backend_compare/option_class_method_in_loop"
        "tests/cases/backend_compare/option_class_or_pattern"
        "tests/cases/backend_compare/option_class_self_consume"
        "tests/cases/backend_compare/option_class_self_method"
        "tests/cases/backend_compare/option_class_value"
        "tests/cases/backend_compare/option_class_via_param"
        "tests/cases/backend_compare/option_classref_passthrough"
        "tests/cases/backend_compare/option_enum_with_payload"
        "tests/cases/backend_compare/option_validate_chain"
        "tests/cases/backend_compare/option_coalesce_in_arith"
        "tests/cases/backend_compare/option_in_loop_branch"
        "tests/cases/backend_compare/option_in_while_cond"
        "tests/cases/backend_compare/method_returning_option"
        "tests/cases/backend_compare/option_pair_method_chain"
        "tests/cases/backend_compare/option_swap_pattern"
        "tests/cases/backend_compare/coalesce_string_concat"
        "tests/cases/backend_compare/intent_header_interleaved"
        "tests/cases/backend_compare/map_keys"
        "tests/cases/backend_compare/map_key_variants"
        "tests/cases/backend_compare/map_presence_ops"
        "tests/cases/backend_compare/map_ops"
        "tests/cases/backend_compare/map_ops_long_bool"
        "tests/cases/backend_compare/list_get_string"
        "tests/cases/backend_compare/list_mutation_ops"
        "tests/cases/backend_compare/list_ops"
        "tests/cases/backend_compare/for_in_array_string_stdlib"
        "tests/cases/backend_compare/for_in_list_int"
        "tests/cases/backend_compare/nested_loops"
        "tests/cases/backend_compare/map_get_string"
        "tests/cases/backend_compare/queue_pop_string"
        "tests/cases/backend_compare/queue_state_ops"
        "tests/cases/backend_compare/queue_ops"
        "tests/cases/backend_compare/set_membership_ops"
        "tests/cases/backend_compare/set_ops"
        "tests/cases/backend_compare/rc_weak_lifecycle"
        "tests/cases/backend_compare/pin_read_view_block"
        "tests/cases/backend_compare/secure_slot_view"
        "tests/cases/backend_compare/pin_secure_read_view_block"
        "tests/cases/backend_compare/pin_mixed_read_view_sequence"
        "tests/cases/backend_compare/pin_write_view_block"
        "tests/cases/backend_compare/pin_secure_write_view_block"
        "tests/cases/backend_compare/pin_successor_cleanup_block"
        "tests/cases/backend_compare/pin_return_value_block"
        "tests/cases/backend_compare/pin_branch_return_block"
        "tests/cases/backend_compare/pin_continue_cleanup_block"
        "tests/cases/backend_compare/pin_break_cleanup_block"
        "tests/cases/backend_compare/pin_secure_param_read_view_block"
        "tests/cases/backend_compare/unsafe_lexical_boundary"
        "tests/cases/backend_compare/world_zone_projection_visibility"
        "tests/cases/backend_compare/world_zone_cross_queries"
        "tests/cases/backend_compare/world_derived_states"
        "tests/cases/backend_compare/world_composed_states"
        "tests/cases/backend_compare/world_zone_mutation_dirty"
        "tests/cases/backend_compare/world_nested_member_assign"
        "tests/cases/backend_compare/zone_has_layer"
        "tests/cases/backend_compare/zone_action_effect_runtime"
        "tests/cases/backend_compare/zone_effect_pool_runtime"
        "tests/cases/backend_compare/zone_layer_projection_runtime"
        "tests/cases/backend_compare/world_embedded_branch_projection_visibility"
        "tests/cases/backend_compare/world_embedded_action_frontier"
        "tests/cases/backend_compare/world_embedded_action_pool_frontier"
        "tests/cases/backend_compare/relation_effect_propagation"
        "tests/cases/backend_compare/relation_effect_projection_sync"
        "tests/cases/backend_compare/authority_failure_surface"
        "tests/cases/backend_compare/higher_order_simple"
        "tests/cases/backend_compare/higher_order_compose"
        "tests/cases/backend_compare/lambda_expr"
        "tests/cases/backend_compare/lambda_block_return"
        "tests/cases/backend_compare/event_named_handler"
        "tests/cases/backend_compare/event_unsubscribe"
        "tests/cases/backend_compare/event_system"
        "tests/cases/backend_compare/event_lambda_handler"
        "tests/cases/backend_compare/async_func_decl"
        "tests/cases/backend_compare/async_spawn_await"
        "tests/cases/backend_compare/async_block_runtime"
        "tests/cases/backend_compare/future_annotation"
        "tests/cases/backend_compare/future_cancel_state"
        "tests/cases/backend_compare/future_cancel_propagation"
        "tests/cases/backend_compare/cancel_future"
        "tests/cases/backend_compare/cancel_propagation"
        "tests/cases/backend_compare/string_spawn"
        "tests/cases/backend_compare/channel_basic"
        "tests/cases/backend_compare/channel_try_recv_timeout"
        "tests/cases/backend_compare/channel_status_options"
        "tests/cases/backend_compare/parallel_channel_sum"
        "tests/cases/backend_compare/parallel_channel_dual"
        "tests/cases/backend_compare/channel_pressure_state"
        "tests/cases/backend_compare/channel_pressure"
        "tests/cases/backend_compare/select_single_ready"
        "tests/cases/backend_compare/select_unbound_ready"
        "tests/cases/backend_compare/select_ready"
        "tests/cases/backend_compare/select_match_case"
        "tests/cases/backend_compare/select_fairness"
        "tests/cases/backend_compare/try_operator_result"
        "tests/cases/backend_compare/triple_paradigm"
        "tests/cases/backend_compare/bin_push_chain"
        "tests/cases/backend_compare/bid_max_score"
        "tests/cases/backend_compare/array_filter_predicate_class"
        "tests/cases/backend_compare/array_minmax_range"
        "tests/cases/backend_compare/bool_short_circuit_method"
        "tests/cases/backend_compare/bucket_count_array"
        "tests/cases/backend_compare/buyer_shopping_chain"
        "tests/cases/backend_compare/cell_grid_total"
        "tests/cases/backend_compare/class_bool_int_method_mix"
        "tests/cases/backend_compare/class_dual_method_loop"
        "tests/cases/backend_compare/class_dual_predicate"
        "tests/cases/backend_compare/class_holds_enum_field"
        "tests/cases/backend_compare/class_holds_class"
        "tests/cases/backend_compare/class_method_self_chain"
        "tests/cases/backend_compare/class_param_method_arr"
        "tests/cases/backend_compare/class_returning_class"
        "tests/cases/backend_compare/class_three_method_compose"
        "tests/cases/backend_compare/class_three_params_clamp"
        "tests/cases/backend_compare/class_walk_iterative"
        "tests/cases/backend_compare/class_with_array_param"
        "tests/cases/backend_compare/dot_product_with_bias"
        "tests/cases/backend_compare/enum_polyarity_compare"
        "tests/cases/backend_compare/factorial_sum_pure"
        "tests/cases/backend_compare/fib_iterative_sum"
        "tests/cases/backend_compare/higher_order_return_context"
        "tests/cases/backend_compare/intermediate_locals_arith"
        "tests/cases/backend_compare/mark_classify_score"
        "tests/cases/backend_compare/match_string_subject"
        "tests/cases/backend_compare/modulo_grouping_sum"
        "tests/cases/backend_compare/multi_match_err_collision"
        "tests/cases/backend_compare/nested_branching_pure"
        "tests/cases/backend_compare/option_2level_class_nested"
        "tests/cases/backend_compare/option_chain_3level_class"
        "tests/cases/backend_compare/option_chain_step_classmethod"
        "tests/cases/backend_compare/option_class_carrying_string"
        "tests/cases/backend_compare/option_class_param_method_call"
        "tests/cases/backend_compare/option_drain_2step"
        "tests/cases/backend_compare/option_default_method"
        "tests/cases/backend_compare/option_mode_dispatch"
        "tests/cases/backend_compare/option_nested_class_field_string"
        "tests/cases/backend_compare/option_pair_map_lookup"
        "tests/cases/backend_compare/option_as_class_field"
        "tests/cases/backend_compare/option_str_to_int_arr"
        "tests/cases/backend_compare/option_typed_chain_diff"
        "tests/cases/backend_compare/option_two_step_half"
        "tests/cases/backend_compare/range_three_method_test"
        "tests/cases/backend_compare/result_as_class_field"
        "tests/cases/backend_compare/result_chain_propagate_err"
        "tests/cases/backend_compare/result_chained_method_class"
        "tests/cases/backend_compare/result_class_chain_score"
        "tests/cases/backend_compare/result_class_with_string"
        "tests/cases/backend_compare/result_field_class_method"
        "tests/cases/backend_compare/result_nested_outer_inner_e"
        "tests/cases/backend_compare/result_pipeline_distinct_classes"
        "tests/cases/backend_compare/result_string_payload"
        "tests/cases/backend_compare/string_array_tag_sum"
        "tests/cases/backend_compare/string_dual_tag"
        "tests/cases/backend_compare/state_collect_high_low"
        "tests/cases/backend_compare/stats_class_build"
        "tests/cases/backend_compare/three_func_compose"
        "tests/cases/backend_compare/try_class_method_chain"
        "tests/cases/backend_compare/two_class_chained_compose"
        "tests/cases/backend_compare/two_step_pure_func_compose"
        "tests/cases/backend_compare/while_class_method_returning_int"
        "tests/cases/backend_compare/array_binary_search"
        "tests/cases/backend_compare/array_balanced_split"
        "tests/cases/backend_compare/array_count_above_avg"
        "tests/cases/backend_compare/array_count_inversions"
        "tests/cases/backend_compare/array_count_occurrences"
        "tests/cases/backend_compare/array_count_ones_bits"
        "tests/cases/backend_compare/array_count_pairs_sum"
        "tests/cases/backend_compare/array_count_sorted_pairs"
        "tests/cases/backend_compare/array_dedup_inplace"
        "tests/cases/backend_compare/array_filter_into_new"
        "tests/cases/backend_compare/array_first_missing_positive"
        "tests/cases/backend_compare/array_fold_minmax_sum"
        "tests/cases/backend_compare/array_insertion_sort"
        "tests/cases/backend_compare/array_kadane_max_subarray"
        "tests/cases/backend_compare/array_minmax_pair"
        "tests/cases/backend_compare/array_pair_concat_sort"
        "tests/cases/backend_compare/array_partition_pivot"
        "tests/cases/backend_compare/array_remove_value"
        "tests/cases/backend_compare/array_rotate_left"
        "tests/cases/backend_compare/array_running_avg_int"
        "tests/cases/backend_compare/array_running_avg_window"
        "tests/cases/backend_compare/array_running_distinct_count"
        "tests/cases/backend_compare/array_running_xor"
        "tests/cases/backend_compare/array_skip_pattern"
        "tests/cases/backend_compare/array_sliding_diff"
        "tests/cases/backend_compare/array_squeeze_zeros"
        "tests/cases/backend_compare/array_swap_pairs"
        "tests/cases/backend_compare/array_swap_pos_neg"
        "tests/cases/backend_compare/array_zero_out_evens"
        "tests/cases/backend_compare/bool_ladder_chain"
        "tests/cases/backend_compare/buffer_full_check"
        "tests/cases/backend_compare/enum_arity_branch_mix"
        "tests/cases/backend_compare/enum_array_dispatch"
        "tests/cases/backend_compare/enum_match_array_loop"
        "tests/cases/backend_compare/enum_match_complex_body"
        "tests/cases/backend_compare/enum_match_default_fallthrough"
        "tests/cases/backend_compare/enum_state_chain_progress"
        "tests/cases/backend_compare/enum_with_4_variants"
        "tests/cases/backend_compare/match_array_payload_destructure"
        "tests/cases/backend_compare/match_basic_arithmetic_chain"
        "tests/cases/backend_compare/match_branch_recursive_helper"
        "tests/cases/backend_compare/match_case_for_loop_dispatch"
        "tests/cases/backend_compare/match_case_loop_with_break"
        "tests/cases/backend_compare/match_case_loop_with_var"
        "tests/cases/backend_compare/match_case_with_let_body"
        "tests/cases/backend_compare/match_class_field_use"
        "tests/cases/backend_compare/match_int_to_status"
        "tests/cases/backend_compare/match_let_string"
        "tests/cases/backend_compare/match_nested_dispatch_real"
        "tests/cases/backend_compare/match_no_payload_compact"
        "tests/cases/backend_compare/match_option_helper_chain"
        "tests/cases/backend_compare/match_simple_compute"
        "tests/cases/backend_compare/match_with_helper_call"
        "tests/cases/backend_compare/match_with_return_branches"
        "tests/cases/backend_compare/multi_match_same_binding"
        "tests/cases/backend_compare/nested_match_3way"
        "tests/cases/backend_compare/nested_match_unique_names"
        "tests/cases/backend_compare/loop_with_break_and_continue"
        "tests/cases/backend_compare/option_call_site_chain"
        "tests/cases/backend_compare/option_count_loop"
        "tests/cases/backend_compare/option_compose_three_steps"
        "tests/cases/backend_compare/option_consumed_in_loop"
        "tests/cases/backend_compare/option_match_complex_branches"
        "tests/cases/backend_compare/option_match_let_chain"
        "tests/cases/backend_compare/option_method_chain"
        "tests/cases/backend_compare/option_nested_match_inner_outer"
        "tests/cases/backend_compare/option_pair_combine"
        "tests/cases/backend_compare/option_returned_from_match"
        "tests/cases/backend_compare/option_returning_none_in_branch"
        "tests/cases/backend_compare/option_validate_then_use"
        "tests/cases/backend_compare/option_var_then_match"
        "tests/cases/backend_compare/option_with_default_arg"
        "tests/cases/backend_compare/result_class_propagate"
        "tests/cases/backend_compare/string_alphanum_extract"
        "tests/cases/backend_compare/string_caesar_simple"
        "tests/cases/backend_compare/string_reverse_words"
        "tests/cases/backend_compare/string_repeat_n"
        "tests/cases/backend_compare/string_runlength_encode"
        "tests/cases/backend_compare/string_split_first"
        "tests/cases/backend_compare/string_strip_prefix_suffix"
        "tests/cases/backend_compare/string_substring_window"
        "tests/cases/backend_compare/triangle_check"
        "tests/cases/backend_compare/triple_func_compose"
        "tests/cases/backend_compare/class_factory_chained"
        "tests/cases/backend_compare/class_factory_in_loop"
        "tests/cases/backend_compare/class_factory_method_field"
        "tests/cases/backend_compare/class_factory_option_wrap"
        "tests/cases/backend_compare/class_factory_result_wrap"
        "tests/cases/backend_compare/class_field_array_method"
        "tests/cases/backend_compare/class_loop_method_total"
        "tests/cases/backend_compare/class_method_result_loop"
        "tests/cases/backend_compare/class_self_factory_chain"
        "tests/cases/backend_compare/class_steps_loop_simple"
        "tests/cases/backend_compare/class_three_distinct"
        "tests/cases/backend_compare/class_two_step_no_loop"
        "tests/cases/backend_compare/empty_array_in_branch"
        "tests/cases/backend_compare/enum_match_payload_basic"
        "tests/cases/backend_compare/enum_state_progression"
        "tests/cases/backend_compare/enum_var_mutation_loop"
        "tests/cases/backend_compare/for_in_double_option_match"
        "tests/cases/backend_compare/for_in_match_classify"
        "tests/cases/backend_compare/for_in_result_dispatch"
        "tests/cases/backend_compare/for_match_inner_if"
        "tests/cases/backend_compare/for_token_classify"
        "tests/cases/backend_compare/for_v_match_v_shadow"
        "tests/cases/backend_compare/function_pointer_reduce"
        "tests/cases/backend_compare/int_steps_loop_simple"
        "tests/cases/backend_compare/lambda_reduce"
        "tests/cases/backend_compare/match_arm_class_ctor"
        "tests/cases/backend_compare/match_arms_with_local_for"
        "tests/cases/backend_compare/match_returns_string"
        "tests/cases/backend_compare/nested_class_compose"
        "tests/cases/backend_compare/nested_for_in_match"
        "tests/cases/backend_compare/option_chain_pipeline_for"
        "tests/cases/backend_compare/recursive_basics"
        "tests/cases/backend_compare/result_triple_nested_match"
        "tests/cases/backend_compare/string_array_concat"
        "tests/cases/backend_compare/while_match_early_return"
        "tests/cases/backend_compare/array_avg_class"
        "tests/cases/backend_compare/array_avg_dev_chain"
        "tests/cases/backend_compare/atom_charged_match"
        "tests/cases/backend_compare/array_cond_compound"
        "tests/cases/backend_compare/array_filter_count_sum"
        "tests/cases/backend_compare/array_inline_class_weighted"
        "tests/cases/backend_compare/array_match_action_sim"
        "tests/cases/backend_compare/array_min_max_loop"
        "tests/cases/backend_compare/pixel_color_match"
        "tests/cases/backend_compare/bank_fluent_chain"
        "tests/cases/backend_compare/bank_interest_recursive"
        "tests/cases/backend_compare/bool_logic_helpers"
        "tests/cases/backend_compare/bubble_sort_basic"
        "tests/cases/backend_compare/build_phase_advance"
        "tests/cases/backend_compare/chain_match_factory"
        "tests/cases/backend_compare/channel_send_recv_basic"
        "tests/cases/backend_compare/class_alive_while_loop"
        "tests/cases/backend_compare/class_arg_helper_loop"
        "tests/cases/backend_compare/class_as_strategy"
        "tests/cases/backend_compare/class_bool_method_filter"
        "tests/cases/backend_compare/class_bump_option_match"
        "tests/cases/backend_compare/class_chained_factory_call"
        "tests/cases/backend_compare/class_compare_return"
        "tests/cases/backend_compare/class_compose_factory"
        "tests/cases/backend_compare/class_factory_aggregate"
        "tests/cases/backend_compare/class_factory_aggregate_loop"
        "tests/cases/backend_compare/class_factory_battle"
        "tests/cases/backend_compare/class_factory_field_method"
        "tests/cases/backend_compare/class_factory_filter_field"
        "tests/cases/backend_compare/class_factory_in_let"
        "tests/cases/backend_compare/class_field_method_chain"
        "tests/cases/backend_compare/class_field_method_chain_inline"
        "tests/cases/backend_compare/class_field_via_factory"
        "tests/cases/backend_compare/class_helper_in_match"
        "tests/cases/backend_compare/class_helper_module"
        "tests/cases/backend_compare/class_immutable_step_until"
        "tests/cases/backend_compare/class_in_loop_field_use"
        "tests/cases/backend_compare/class_method_branch_nest"
        "tests/cases/backend_compare/class_nested_construct_method"
        "tests/cases/backend_compare/class_nested_field_chain"
        "tests/cases/backend_compare/class_node_field_access"
        "tests/cases/backend_compare/node_traversal_sum"
        "tests/cases/backend_compare/class_param_return_chain"
        "tests/cases/backend_compare/class_recursive_factory"
        "tests/cases/backend_compare/class_result_chain_loop"
        "tests/cases/backend_compare/class_self_field_method"
        "tests/cases/backend_compare/class_method_self_return"
        "tests/cases/backend_compare/class_stack_immutable"
        "tests/cases/backend_compare/class_suit_score"
        "tests/cases/backend_compare/class_user_box"
        "tests/cases/backend_compare/user_class_cell"
        "tests/cases/backend_compare/user_class_stack"
        "tests/cases/backend_compare/user_func_clone"
        "tests/cases/backend_compare/era_stage_factory"
        "tests/cases/backend_compare/user_clone_func"
        "tests/cases/backend_compare/wave_match_chain"
        "tests/cases/backend_compare/class_within_class_chain"
        "tests/cases/backend_compare/color_factory_mix"
        "tests/cases/backend_compare/counter_done_chain"
        "tests/cases/backend_compare/counter_op_dispatch"
        "tests/cases/backend_compare/crystal_best_quality"
        "tests/cases/backend_compare/decode_array_result"
        "tests/cases/backend_compare/deep_three_level_field"
        "tests/cases/backend_compare/dish_result_collect"
        "tests/cases/backend_compare/drop_option_collect"
        "tests/cases/backend_compare/element_match_chain"
        "tests/cases/backend_compare/encode_decode_chain"
        "tests/cases/backend_compare/enum_chain_match_compose"
        "tests/cases/backend_compare/enum_class_factory_match"
        "tests/cases/backend_compare/enum_dual_dispatch"
        "tests/cases/backend_compare/enum_to_class_match"
        "tests/cases/backend_compare/event_time_chain"
        "tests/cases/backend_compare/factory_inline_field_access"
        "tests/cases/backend_compare/factory_inline_in_loop"
        "tests/cases/backend_compare/factory_inline_method"
        "tests/cases/backend_compare/factory_inner_class_method_chain"
        "tests/cases/backend_compare/factory_method_with_b_chain"
        "tests/cases/backend_compare/factory_nested_field_method_chain"
        "tests/cases/backend_compare/factory_normalized_max"
        "tests/cases/backend_compare/factory_state_in_loop"
        "tests/cases/backend_compare/fluent_builder_chain"
        "tests/cases/backend_compare/fluent_chain_in_loop"
        "tests/cases/backend_compare/game_round_dispatch"
        "tests/cases/backend_compare/for_factory_inline_chain"
        "tests/cases/backend_compare/long_class_chain"
        "tests/cases/backend_compare/long_fluent_chain"
        "tests/cases/backend_compare/for_range_class_factory"
        "tests/cases/backend_compare/for_range_class_payload"
        "tests/cases/backend_compare/for_range_factory_chain"
        "tests/cases/backend_compare/for_range_match_parity"
        "tests/cases/backend_compare/for_range_nested"
        "tests/cases/backend_compare/for_range_sum"
        "tests/cases/backend_compare/grid_average"
        "tests/cases/backend_compare/hashmap_basic_ops"
        "tests/cases/backend_compare/health_immutable_helpers"
        "tests/cases/backend_compare/hex_nested_diff"
        "tests/cases/backend_compare/inline_class_compare_loop"
        "tests/cases/backend_compare/inline_class_loop_best"
        "tests/cases/backend_compare/int_helpers_basic"
        "tests/cases/backend_compare/inline_ctor_for_range"
        "tests/cases/backend_compare/intent_minimal"
        "tests/cases/backend_compare/intent_with_step"
        "tests/cases/backend_compare/item_chain_in_for"
        "tests/cases/backend_compare/list_set_basics"
        "tests/cases/backend_compare/machine_step_alive"
        "tests/cases/backend_compare/manhattan_distance"
        "tests/cases/backend_compare/match_arms_class_lets"
        "tests/cases/backend_compare/match_arms_with_if"
        "tests/cases/backend_compare/match_engage_combo"
        "tests/cases/backend_compare/method_returns_class_chain"
        "tests/cases/backend_compare/mode_reading_index"
        "tests/cases/backend_compare/move_recursive_dir"
        "tests/cases/backend_compare/multi_factory_method_chain"
        "tests/cases/backend_compare/nested_class_chain_method"
        "tests/cases/backend_compare/option_class_loot_chain"
        "tests/cases/backend_compare/pair_max_distance"
        "tests/cases/backend_compare/pair_swap_chain"
        "tests/cases/backend_compare/parallel_arrays_loop"
        "tests/cases/backend_compare/param_class_chain_recursion"
        "tests/cases/backend_compare/pipeline_stage_loop"
        "tests/cases/backend_compare/player_state_chain"
        "tests/cases/backend_compare/quest_collect_for_range"
        "tests/cases/backend_compare/recursive_class_factory_acc"
        "tests/cases/backend_compare/result_class_match_method"
        "tests/cases/backend_compare/shield_match_effect"
        "tests/cases/backend_compare/slot_int_basic"
        "tests/cases/backend_compare/score_apply_ops"
        "tests/cases/backend_compare/score_max_in_loop"
        "tests/cases/backend_compare/series_snap_chain"
        "tests/cases/backend_compare/sphere_volume_density"
        "tests/cases/backend_compare/spec_active_total"
        "tests/cases/backend_compare/spec_long_chain"
        "tests/cases/backend_compare/stage_build_loop"
        "tests/cases/backend_compare/step_pipeline_run"
        "tests/cases/backend_compare/stats_compute_var"
        "tests/cases/backend_compare/string_array_count"
        "tests/cases/backend_compare/string_concat_loop"
        "tests/cases/backend_compare/string_lang_match"
        "tests/cases/backend_compare/string_ops_basics"
        "tests/cases/backend_compare/stack_op_match"
        "tests/cases/backend_compare/subject_class_combined"
        "tests/cases/backend_compare/subject_self_method"
        "tests/cases/backend_compare/tail_recursion_math"
        "tests/cases/backend_compare/tier_make_compare"
        "tests/cases/backend_compare/tower_refine_chain"
        "tests/cases/backend_compare/trade_side_net"
        "tests/cases/backend_compare/tree_grow_recursive"
        "tests/cases/backend_compare/tree_walk_recursive"
        "tests/cases/backend_compare/triple_nested_class"
        "tests/cases/backend_compare/vault_fluent_chain"
        "tests/cases/backend_compare/vec_class_helper_chain"
        "tests/cases/backend_compare/worker_best_rate"
        "tests/cases/backend_compare/world_with_zones"
        "tests/cases/backend_compare/zone_minimal"
        "tests/cases/backend_compare/zone_with_subject_slot"
    )

    local use_default_cases=0
    if [[ "$#" -eq 0 ]]; then
        use_default_cases=1
        local -a missing_cases=()
        local -a missing_registered_cases=()
        local -A registered_case_set=()
        local discovered
        local registered
        local source_rel
        for registered in "${cases[@]}"; do
            registered_case_set["$registered"]=1
        done
        while IFS= read -r discovered; do
            source_rel="$(dirname "$discovered")"
            if [[ -z "${registered_case_set[$source_rel]+x}" ]]; then
                missing_cases+=("$source_rel")
            fi
        done < <(find tests/cases/backend_compare -mindepth 2 -maxdepth 2 -name main.pgy | LC_ALL=C sort)

        for registered in "${cases[@]}"; do
            if [[ ! -f "$registered/main.pgy" ]]; then
                missing_registered_cases+=("$registered")
            fi
        done

        if (( ${#missing_cases[@]} > 0 || ${#missing_registered_cases[@]} > 0 )); then
            if (( ${#missing_cases[@]} > 0 )); then
                echo "backend-compare: case inventory is missing registered entries:" >&2
                for discovered in "${missing_cases[@]}"; do
                    echo "  - $discovered" >&2
                done
                echo "Add each case to the default cases array, or pass explicit case arguments for a targeted run." >&2
            fi
            if (( ${#missing_registered_cases[@]} > 0 )); then
                echo "backend-compare: default cases array references missing cases:" >&2
                for registered in "${missing_registered_cases[@]}"; do
                    echo "  - $registered" >&2
                done
                echo "Add the missing fixture, or remove the stale registered entry." >&2
            fi
            return 1
        fi

        if [[ "${PGY_BACKEND_COMPARE_INVENTORY_ONLY:-0}" != "0" ]]; then
            echo "[backend-compare-inventory] default case inventory is complete"
            return 0
        fi
    fi

    if [[ "$#" -gt 0 ]]; then
        cases=("$@")
    fi

    local shard_total="${PGY_BACKEND_COMPARE_SHARD_TOTAL:-${PGY_BACKEND_COMPARE_SHARD_COUNT:-0}}"
    local shard_index="${PGY_BACKEND_COMPARE_SHARD_INDEX:-}"
    if (( use_default_cases )) && [[ "$shard_total" != "0" ]]; then
        if [[ ! "$shard_total" =~ ^[0-9]+$ || "$shard_total" -le 0 ]]; then
            echo "backend-compare: PGY_BACKEND_COMPARE_SHARD_TOTAL must be a positive integer" >&2
            return 1
        fi
        if [[ ! "$shard_index" =~ ^[0-9]+$ || "$shard_index" -ge "$shard_total" ]]; then
            echo "backend-compare: PGY_BACKEND_COMPARE_SHARD_INDEX must be in [0, $((shard_total - 1))]" >&2
            return 1
        fi

        local -a sharded_cases=()
        local case_index
        for case_index in "${!cases[@]}"; do
            if (( case_index % shard_total == shard_index )); then
                sharded_cases+=("${cases[$case_index]}")
            fi
        done
        echo "backend-compare: shard ${shard_index}/${shard_total} selected ${#sharded_cases[@]}/${#cases[@]} cases"
        cases=("${sharded_cases[@]}")
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
