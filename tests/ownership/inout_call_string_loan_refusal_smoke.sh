#!/usr/bin/env bash
# A String borrowed from an Array<String> or List<String> element and read
# after a call whose inout formal frees that element (an owned ArraySet or
# ArrayPop, ListSet or ListRemove in the callee or in a routine it hands the
# formal to) read freed memory on native C and LLVM: the native checker
# retired loans only at a builtin call on the receiver itself
# (docs/truth/evidence/2026-11-20_native_inout_call_string_loan_retirement.md).
# A loan of an exact Array source must now be refused at the read on every
# backend, with no executable. A List loan has no storage source of its own;
# it is copied where it is defined, as after a direct ListSet, so its
# program prints the value it read. A loan read before the call, a callee
# that only reads or pushes, and a call on another path stay admitted.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=inout-call-string-loan-refusal
pgy_require_runnable_binary_here "$LABEL" "$PGY"
BACKENDS="${PGY_INOUT_CALL_STRING_LOAN_BACKENDS:-c llvm}"
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/inout-call-string-loan-refusal.XXXXXX)"
F=tests/cases/inout_call_string_loan
fail() { echo "[$LABEL] $*; evidence=$WORK" >&2; exit 1; }

RETIRED='borrows an Array or List element that a call to'
# fixture|position of the refused read|binding read there|callee named
NEGATIVES=(
    "array_set_in_callee_invalid|13:9|name|Replace"
    "array_pop_in_callee_invalid|12:9|name|Shrink"
    "callee_declared_later_invalid|9:9|name|Replace"
    "forwarded_pop_invalid|15:9|name|Outer"
    "call_in_branch_invalid|14:9|name|Replace"
    "loop_read_before_call_invalid|13:13|name|Replace"
    "loop_continue_path_invalid|19:13|name|Replace"
    "reassigned_local_invalid|17:9|name|Replace"
)
# fixture|expected stdout, one line per word
POSITIVES=(
    "read_before_call_valid|5 99 2"
    "inout_reader_valid|5 5 2"
    "inout_push_valid|5 3"
    "other_array_loan_valid|6 3"
    "else_branch_read_valid|5 2"
    "early_return_branch_valid|5 2"
    "loop_call_then_break_valid|5 5 2"
    "list_set_in_callee_copied_valid|5 2"
    "list_remove_in_callee_copied_valid|5 1"
    "list_forwarded_set_copied_valid|5 2"
    "list_loop_read_before_call_copied_valid|5 5 2"
    "list_reader_valid|5 5 2"
)

for backend in $BACKENDS; do
    for entry in "${NEGATIVES[@]}"; do
        IFS='|' read -r name position binding callee <<<"$entry"
        exe="$WORK/$name.$backend"
        status=0
        "$PGY" --native-pipeline --backend="$backend" "$F/$name.pgy" -o "$exe" \
            >"$WORK/$name.$backend.out" 2>"$WORK/$name.$backend.err" || status=$?
        [[ "$status" == 1 ]] || fail "$name ($backend): expected a semantic refusal, exit $status"
        [[ ! -e "$exe" && ! -e "$exe.exe" ]] || fail "$name ($backend): built an executable for a refused program"
        grep -Fq "] $position - String '$binding' $RETIRED '$callee'" "$WORK/$name.$backend.err" ||
            fail "$name ($backend): not refused at the read of '$binding' ($position) after '$callee'"
    done
    for entry in "${POSITIVES[@]}"; do
        IFS='|' read -r name expected <<<"$entry"
        exe="$WORK/$name.$backend"
        "$PGY" --native-pipeline --backend="$backend" "$F/$name.pgy" -o "$exe" \
            >"$WORK/$name.$backend.out" 2>"$WORK/$name.$backend.err" ||
            fail "$name ($backend): safe program refused"
        "$exe" >"$WORK/$name.$backend.run" 2>&1 || fail "$name ($backend): exit $?"
        actual="$(tr -d '\r' <"$WORK/$name.$backend.run" | tr '\n' ' ' | sed 's/ *$//')"
        [[ "$actual" == "$expected" ]] ||
            fail "$name ($backend): printed '$actual', expected '$expected'"
    done
done
echo "[$LABEL] backends: $BACKENDS; ${#NEGATIVES[@]} refusals, ${#POSITIVES[@]} admissions each PASS"
