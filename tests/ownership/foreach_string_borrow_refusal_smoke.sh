#!/usr/bin/env bash
# A foreach binding over Array<String> (or a Slice<String> of one) borrows
# the element it names, as `let x = xs[i]` does. The native checker admitted
# a body that released that element (owned ArraySet, ArrayPop or
# ArrayDropOwnedStrings) and then read the binding, and the program read
# freed memory on C and LLVM
# (docs/truth/evidence/2026-11-20_native_foreach_string_borrow_refusal.md).
# A loan read at the top of the next iteration, after the previous one
# released its element, was admitted too: the loop was checked only once.
# Each negative must be refused at the read on every backend, with no
# executable; each positive must compile and print its expected output.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=foreach-string-borrow-refusal
pgy_require_runnable_binary_here "$LABEL" "$PGY"
BACKENDS="${PGY_FOREACH_STRING_BORROW_BACKENDS:-c llvm}"
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/foreach-string-borrow-refusal.XXXXXX)"
F=tests/cases/foreach_string_borrow
fail() { echo "[$LABEL] $*; evidence=$WORK" >&2; exit 1; }

RETIRED='borrows an Array<String> element that was replaced or released'
# fixture|position of the refused read|binding read there
NEGATIVES=(
    "set_while_bound_invalid|11:13|name"
    "pop_while_bound_invalid|12:13|name"
    "drop_while_bound_invalid|10:13|name"
    "slice_set_while_bound_invalid|11:13|name"
    "kept_after_set_invalid|13:9|kept"
    "index_pop_while_bound_invalid|9:9|name"
    "next_iteration_outer_read_invalid|10:13|first"
    "next_iteration_outer_read_while_invalid|10:13|first"
    "next_iteration_kept_read_invalid|10:13|kept"
)
# fixture|expected stdout, one line per word
POSITIVES=(
    "read_before_set_valid|5 6 99 2"
    "read_before_pop_valid|5 6 2"
    "push_then_set_valid|5 99 2"
    "outer_read_release_after_loop_valid|5 5 5 6 2"
    "single_iteration_range_valid|5 1"
)

for backend in $BACKENDS; do
    for entry in "${NEGATIVES[@]}"; do
        IFS='|' read -r name position binding <<<"$entry"
        exe="$WORK/$name.$backend"
        status=0
        "$PGY" --native-pipeline --backend="$backend" "$F/$name.pgy" -o "$exe" \
            >"$WORK/$name.$backend.out" 2>"$WORK/$name.$backend.err" || status=$?
        [[ "$status" == 1 ]] || fail "$name ($backend): expected a semantic refusal, exit $status"
        [[ ! -e "$exe" && ! -e "$exe.exe" ]] || fail "$name ($backend): built an executable for a refused program"
        grep -Fq "] $position - String '$binding' $RETIRED" "$WORK/$name.$backend.err" ||
            fail "$name ($backend): not refused at the read of '$binding' ($position)"
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
