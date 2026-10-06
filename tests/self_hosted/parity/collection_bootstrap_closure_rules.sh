#!/usr/bin/env bash
# Bootstrap-closure ownership precision falsifiers: loop-local constructor
# field escape, statement mutation after a proved shallow call, readonly local
# struct field lending, block-local owned literal transfer, comparison before an
# own-formal literal transfer and function-exit tail consumption. Each positive has refusals that keep the old boundary.
# Current C/LLVM analyzers run; supplied source inputs never emit or execute.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-bootstrap-closure "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-bootstrap-closure.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[collection-bootstrap-closure] $*; evidence=$REL" >&2; exit 1; }
FIXTURES=tests/self_hosted/parity/fixture/collection_bootstrap_closure
SOURCE_PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
observe() {
    "$observer" "$FIXTURES/$1.pgy" diagnostic >"$WORK/$backend-$1.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$WORK/$backend-$1.raw" >"$WORK/$backend-$1.log"
}
refused() {
    local name="$1" boundary="$2"
    observe "$name"
    grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
    grep -Fq -- "$boundary" "$WORK/$backend-$name.log" || fail "$name lost its $boundary refusal"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$SOURCE_PROBE" \
        -o "$observer" >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer did not compile"
    for name in loop_constructor_field_escape_positive inout_then_push_positive \
        local_root_field_read_positive local_root_scalar_and_lend_positive \
        local_root_terminal_aggregate_positive block_literal_transfer_positive \
        exit_tail_consumption_positive own_formal_compare_then_transfer_positive; do
        observe "$name"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused $name"
    done
    refused loop_constructor_late_push_negative '- boundary: ArrayPush'
    refused outer_array_loop_escape_negative '- boundary: ArrayPush'
    refused push_after_consume_negative 'owned_argument_storage_not_live'
    refused push_after_deep_drop_negative '- boundary: ArrayPush'
    refused local_root_alias_negative 'unproved_formal_indexed_read_entry'
    refused local_root_push_negative 'unproved_formal_indexed_read_entry'
    refused local_root_reassign_negative 'unproved_formal_indexed_read_entry'
    refused local_root_consume_negative 'unproved_formal_indexed_read_entry'
    refused local_root_open_alias_mutation_negative 'unproved_formal_indexed_read_entry'
    refused block_literal_outer_source_negative 'owned_string_drop'
    refused block_literal_conditional_drop_negative 'owned_string_drop'
    refused block_literal_use_after_drop_negative 'owned_string_drop'
    refused nonterminal_consumption_negative 'owned_argument_storage_not_live'
    refused exit_tail_same_expression_use_negative 'owned_argument_use_after_move'
    refused break_tail_consumption_negative 'owned_argument_storage_not_live'
    refused own_formal_compare_after_transfer_negative 'owned_string_drop'
done
echo "[collection-bootstrap-closure] PASS (C/LLVM analysis: 8 positives and 16 refusals each; not installed-driver proof); evidence=$REL"
