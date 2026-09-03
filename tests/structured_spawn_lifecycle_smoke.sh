#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate: Future/RemoteFuture completion handles are affine
# lexical obligations. The native semantic owner is the subject under test;
# self-host coverage must not be substituted for it here.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[structured-spawn] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_structured_spawn.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

for required in \
    "PGY_FUTURE_LIFECYCLE_LIVE" \
    "semantic_future_admit_spawn" \
    "semantic_future_require_function_retired" \
    "PGY_CODE_SEM_TASK_LIFECYCLE"; do
    if ! grep -R -Fq "$required" \
        "$ROOT_DIR/src/semantic/symbol_table.h" \
        "$ROOT_DIR/src/semantic/type_checker_future_lifecycle.c" \
        "$ROOT_DIR/src/semantic/diag_codes.h"; then
        echo "[structured-spawn] missing semantic owner pin: $required" >&2
        exit 1
    fi
done

positive_cases=(
    positive_joined
    positive_cancel_join
    positive_immediate_await
    positive_both_branches
    positive_parallel_single_join
    positive_static_if_true_join
    positive_static_if_false_else_join
    positive_static_if_unreachable_return
    positive_static_if_unreachable_break
    positive_known_single_iteration_join
    positive_known_single_iteration_continue_join
    positive_known_zero_iteration_then_join
    positive_static_while_break_join
    positive_static_match_join
    positive_static_match_unreachable_return
    positive_static_match_unreachable_break
    positive_own_transfer
    positive_own_remote_transfer
    positive_own_mixed_transfer
)
for case_name in "${positive_cases[@]}"; do
    source="$ROOT_DIR/tests/cases/structured_spawn_lifecycle/${case_name}.pgy"
    source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
    output_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/${case_name}.c")"
    if ! "$PGY" "$source_arg" --backend=c --emit-c -o "$output_arg" \
        >"$WORK_DIR/${case_name}.out" 2>"$WORK_DIR/${case_name}.err"; then
        echo "[structured-spawn] positive case failed: $case_name" >&2
        cat "$WORK_DIR/${case_name}.err" >&2
        exit 1
    fi
done

parallel_double_source="$ROOT_DIR/tests/cases/structured_spawn_lifecycle/negative_parallel_double_join.pgy"
parallel_double_arg="$(pgy_path_for_compiler "$PGY" "$parallel_double_source")"
for backend in c llvm; do
    parallel_double_out="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/negative_parallel_double_join_${backend}.bin")"
    parallel_double_log="$WORK_DIR/negative_parallel_double_join_${backend}.log"
    if "$PGY" "$parallel_double_arg" --backend="$backend" --error-format=json \
        -o "$parallel_double_out" >"$parallel_double_log" 2>&1; then
        echo "[structured-spawn] double parallel join unexpectedly succeeded ($backend)" >&2
        exit 1
    fi
    for required in \
        '"stage":"semantic"' \
        '"code":"PGY_SEM_PARALLEL_SLOT_CONFLICT"' \
        '"cause_ir":"semantic:parallel:resource_conflict"'; do
        if ! grep -Fq "$required" "$parallel_double_log"; then
            echo "[structured-spawn] double parallel join ($backend) missing: $required" >&2
            cat "$parallel_double_log" >&2
            exit 1
        fi
    done
    if grep -Fq '"stage":"internal"' "$parallel_double_log"; then
        echo "[structured-spawn] double parallel join ($backend) reached an internal failure" >&2
        cat "$parallel_double_log" >&2
        exit 1
    fi
done

# `own` transfer crosses a function ABI boundary. Compile and execute the
# local and remote handle cases with both production backends so MIR type-name
# carriage cannot silently regress to the former AST-only registry path.
backend_transfer_cases=(
    positive_own_transfer
    positive_own_remote_transfer
    positive_own_mixed_transfer
)
for case_name in "${backend_transfer_cases[@]}"; do
    source="$ROOT_DIR/tests/cases/structured_spawn_lifecycle/${case_name}.pgy"
    source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
    for backend in c llvm; do
        binary="$WORK_DIR/${case_name}_${backend}.exe"
        binary_arg="$(pgy_path_for_compiler "$PGY" "$binary")"
        if ! "$PGY" "$source_arg" --backend="$backend" -o "$binary_arg" \
            >"$WORK_DIR/${case_name}_${backend}.compile.out" \
            2>"$WORK_DIR/${case_name}_${backend}.compile.err"; then
            echo "[structured-spawn] $backend compile failed: $case_name" >&2
            cat "$WORK_DIR/${case_name}_${backend}.compile.err" >&2
            exit 1
        fi
        pgy_require_runnable_binary_here "structured-spawn" "$binary"
        set +e
        pgy_run_with_timeout 15 \
            "$WORK_DIR/${case_name}_${backend}.run" \
            "$WORK_DIR/${case_name}_${backend}.run.err" \
            "$binary"
        run_rc=$?
        set -e
        if [[ "$run_rc" -ne 0 ]]; then
            echo "[structured-spawn] $backend execution failed: $case_name (rc=$run_rc)" >&2
            cat "$WORK_DIR/${case_name}_${backend}.run.err" >&2
            exit 1
        fi
        if [[ -s "$WORK_DIR/${case_name}_${backend}.run.err" ]]; then
            echo "[structured-spawn] $backend execution wrote stderr: $case_name" >&2
            cat "$WORK_DIR/${case_name}_${backend}.run.err" >&2
            exit 1
        fi
        tr -d '\r' <"$WORK_DIR/${case_name}_${backend}.run" \
            >"$WORK_DIR/${case_name}_${backend}.normalized"
    done
    if ! cmp -s "$WORK_DIR/${case_name}_c.normalized" \
        "$WORK_DIR/${case_name}_llvm.normalized"; then
        echo "[structured-spawn] C/LLVM output mismatch: $case_name" >&2
        diff -u "$WORK_DIR/${case_name}_c.normalized" \
            "$WORK_DIR/${case_name}_llvm.normalized" >&2 || true
        exit 1
    fi
    case "$case_name" in
        positive_own_transfer) printf '1\n' >"$WORK_DIR/${case_name}.expected" ;;
        positive_own_remote_transfer) printf '11\n' >"$WORK_DIR/${case_name}.expected" ;;
        positive_own_mixed_transfer) printf '3\n11\n' >"$WORK_DIR/${case_name}.expected" ;;
        *) echo "[structured-spawn] missing exact output owner: $case_name" >&2; exit 1 ;;
    esac
    if ! cmp -s "$WORK_DIR/${case_name}.expected" \
        "$WORK_DIR/${case_name}_c.normalized"; then
        echo "[structured-spawn] unexpected runtime output: $case_name" >&2
        diff -u "$WORK_DIR/${case_name}.expected" \
            "$WORK_DIR/${case_name}_c.normalized" >&2 || true
        exit 1
    fi
done

negative_cases=(
    negative_fallthrough
    negative_return
    negative_nested_scope
    negative_branch_divergence
    negative_bare_spawn
    negative_mutable_future
    negative_borrowed_parameter
    negative_future_return
    negative_alias_binding
    negative_owned_parameter_drop
    negative_owned_remote_parameter_drop
    negative_loop_break
    negative_loop_zero_iteration
)
for case_name in "${negative_cases[@]}"; do
    source="$ROOT_DIR/tests/cases/structured_spawn_lifecycle/${case_name}.pgy"
    source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
    for backend in c llvm; do
        output_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/${case_name}_${backend}.bin")"
        log="$WORK_DIR/${case_name}_${backend}.log"
        if "$PGY" "$source_arg" --backend="$backend" --error-format=json \
            -o "$output_arg" >"$log" 2>&1; then
            echo "[structured-spawn] negative case unexpectedly succeeded: $case_name ($backend)" >&2
            exit 1
        fi
        for required in \
            '"stage":"semantic"' \
            '"code":"PGY_SEM_TASK_LIFECYCLE"' \
            '"cause_ir":"semantic:task:lifecycle"' \
            '"fix_source":"await-task-before-exit"'; do
            if ! grep -Fq "$required" "$log"; then
                echo "[structured-spawn] $case_name ($backend) missing: $required" >&2
                cat "$log" >&2
                exit 1
            fi
        done
        if grep -Fq '"stage":"internal"' "$log"; then
            echo "[structured-spawn] $case_name ($backend) reached an internal failure" >&2
            cat "$log" >&2
            exit 1
        fi
    done
done

use_after_cases=(
    negative_use_after_own_transfer
    negative_cancel_after_own_transfer
    negative_second_own_transfer
)
for case_name in "${use_after_cases[@]}"; do
    source="$ROOT_DIR/tests/cases/structured_spawn_lifecycle/${case_name}.pgy"
    source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
    for backend in c llvm; do
        output_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/${case_name}_${backend}.bin")"
        log="$WORK_DIR/${case_name}_${backend}.log"
        if "$PGY" "$source_arg" --backend="$backend" --error-format=json \
            -o "$output_arg" >"$log" 2>&1; then
            echo "[structured-spawn] use after own transfer unexpectedly succeeded: $case_name ($backend)" >&2
            exit 1
        fi
        for required in \
            '"stage":"semantic"' \
            '"code":"PGY_SEM_MOVE_FROM_RELEASED"' \
            '"cause_ir":"semantic:move:from_released"'; do
            if ! grep -Fq "$required" "$log"; then
                echo "[structured-spawn] $case_name ($backend) missing: $required" >&2
                cat "$log" >&2
                exit 1
            fi
        done
        if grep -Eq 'PGY_SEM_TYPE_MISMATCH|PGY_SEM_BUILTIN_ARGS_INVALID|"stage":"internal"' "$log"; then
            echo "[structured-spawn] $case_name ($backend) emitted a cascade diagnostic" >&2
            cat "$log" >&2
            exit 1
        fi
        error_count="$(grep -o '"severity":"error"' "$log" | wc -l | tr -d '[:space:]')"
        if [[ "$error_count" != "1" ]]; then
            echo "[structured-spawn] $case_name ($backend) expected one owned error, got $error_count" >&2
            cat "$log" >&2
            exit 1
        fi
    done
done

repeated_source="$ROOT_DIR/tests/cases/structured_spawn_lifecycle/negative_repeated_unavailable_use.pgy"
repeated_arg="$(pgy_path_for_compiler "$PGY" "$repeated_source")"
for backend in c llvm; do
    repeated_out="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/negative_repeated_unavailable_use_${backend}.bin")"
    repeated_log="$WORK_DIR/negative_repeated_unavailable_use_${backend}.log"
    if "$PGY" "$repeated_arg" --backend="$backend" --error-format=json \
        -o "$repeated_out" >"$repeated_log" 2>&1; then
        echo "[structured-spawn] repeated unavailable Future use unexpectedly succeeded ($backend)" >&2
        exit 1
    fi
    for required in \
        '"stage":"semantic"' \
        '"code":"PGY_SEM_TASK_LIFECYCLE"' \
        '"cause_ir":"semantic:task:lifecycle"'; do
        if ! grep -Fq "$required" "$repeated_log"; then
            echo "[structured-spawn] repeated unavailable use ($backend) missing: $required" >&2
            cat "$repeated_log" >&2
            exit 1
        fi
    done
    if grep -Eq 'PGY_SEM_TYPE_MISMATCH|PGY_SEM_BUILTIN_ARGS_INVALID|"stage":"internal"' "$repeated_log"; then
        echo "[structured-spawn] repeated unavailable use ($backend) emitted a cascade diagnostic" >&2
        cat "$repeated_log" >&2
        exit 1
    fi
    error_count="$(grep -o '"severity":"error"' "$repeated_log" | wc -l | tr -d '[:space:]')"
    if [[ "$error_count" != "1" ]]; then
        echo "[structured-spawn] repeated unavailable use ($backend) expected one owned error, got $error_count" >&2
        cat "$repeated_log" >&2
        exit 1
    fi
done

echo "[structured-spawn] affine completion handles are joined or explicitly transferred"
