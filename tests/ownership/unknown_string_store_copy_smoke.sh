#!/usr/bin/env bash
# A String of unknown source (a ListGet loan, an element of a Slice view)
# stored where it outlives its owner's release: pushed into an Array that
# does not own its elements, kept in a struct field, or given to a callee
# that keeps it in its result. The native checker
# admitted these stores shallow, and once ListSet, ListRemove or an owned
# ArraySet freed the element the program read freed memory on C and LLVM
# (docs/truth/evidence/2026-11-20_native_unknown_string_store_copy.md).
# Each program now stores a copy, so it must compile and print the String it
# stored; a filler String allocated after the free makes a stale read print
# the filler instead.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=unknown-string-store-copy
pgy_require_runnable_binary_here "$LABEL" "$PGY"
BACKENDS="${PGY_UNKNOWN_STRING_STORE_BACKENDS:-c llvm}"
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/unknown-string-store-copy.XXXXXX)"
F=tests/cases/unknown_string_store
fail() { echo "[$LABEL] $*; evidence=$WORK" >&2; exit 1; }

# fixture|expected stdout, one line per word
POSITIVES=(
    "list_get_pushed_then_set_copied_valid|5 77 1"
    "list_let_pushed_then_set_copied_valid|5 77 1"
    "list_get_field_then_set_copied_valid|5 77 2"
    "list_let_field_then_remove_copied_valid|5 77 1"
    "slice_let_pushed_then_set_copied_valid|5 77 1"
    "slice_element_pushed_then_set_copied_valid|5 77 1"
    "slice_element_field_then_set_copied_valid|5 77 2"
    "slice_element_named_retainer_then_set_copied_valid|5 77 2"
    "slice_foreach_pushed_then_set_copied_valid|5 77 1"
    "list_let_pushed_into_owning_array_valid|5 77 2"
)

for backend in $BACKENDS; do
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
echo "[$LABEL] backends: $BACKENDS; ${#POSITIVES[@]} admissions each PASS"
