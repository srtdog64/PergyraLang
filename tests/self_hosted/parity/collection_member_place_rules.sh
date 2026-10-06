#!/usr/bin/env bash
# Member-place ownership falsifiers: one storage place is a root binding plus
# an exact field path at any depth. A nested Array<String> place of a readonly
# formal or a local may be lent for reading while no occurrence of the root or
# of a path prefix can alias, write, consume or release it. A deep-released
# aggregate keeps the release unproved while any String element or nominal
# sub-place of it may escape its reading expression, including through a lent
# callee formal. A String formal that only compares, concatenates or forwards
# its text to another such formal admits a borrowed element like a borrowed
# builtin argument. A plain parameter is a readonly root: its field storage is
# the caller's, so mutating it needs inout or own. Current C/LLVM analyzers
# run; inputs never emit or execute.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-member-place "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-member-place.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[collection-member-place] $*; evidence=$REL" >&2; exit 1; }
FIXTURES=tests/self_hosted/parity/fixture/collection_member_place
SOURCE_PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
observe() {
    "$observer" "$FIXTURES/$1.pgy" diagnostic >"$WORK/$backend-$1.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$WORK/$backend-$1.raw" >"$WORK/$backend-$1.log"
}
refused_code() {
    local name="$1" code="$2" fact="$3"
    observe "$name"
    grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
    grep -Fxq "body_diagnostic=$code" "$WORK/$backend-$name.log" || fail "$name lost its $code refusal"
    grep -Fxq -- "$fact" "$WORK/$backend-$name.log" || fail "$name lost its $fact fact"
}
refused() {
    local name="$1" boundary="$2"
    observe "$name"
    grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
    grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$name.log" || fail "$name lost its $boundary refusal"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$SOURCE_PROBE" \
        -o "$observer" >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer did not compile"
    for name in nested_local_read_positive nested_ref_formal_read_positive \
        nested_default_formal_read_positive nested_prefix_readonly_lend_positive \
        nested_index_length_lend_positive nested_struct_lend_positive \
        release_after_compare_and_copy_positive text_formal_reader_element_positive \
        text_formal_forward_positive text_formal_interpolation_positive \
        own_param_field_update_positive inout_param_field_update_positive \
        plain_param_scalar_field_assign_positive nested_read_with_element_binding_positive \
        release_after_same_root_field_alias_positive; do
        observe "$name"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused $name"
    done
    for name in release_after_callee_element_return_negative release_after_callee_element_push_negative \
        release_after_local_element_binding_negative release_after_local_element_push_negative \
        release_after_constructor_capture_negative release_after_identity_call_negative \
        release_inout_root_element_binding_negative release_extracted_nested_element_negative \
        release_after_other_root_field_alias_negative; do
        refused "$name" aggregate_release_element_borrow
    done
    for name in nested_prefix_alias_negative nested_sequence_push_negative \
        nested_prefix_reassign_negative nested_formal_root_alias_negative \
        nested_prefix_lend_alias_negative nested_inout_root_negative; do
        refused "$name" unproved_formal_indexed_read_entry
    done
    for name in text_formal_retaining_forward_negative text_formal_tostring_binding_negative \
        text_formal_writable_callee_negative; do
        refused "$name" unproved_formal_element_use_entry
    done
    for name in plain_param_field_push_negative plain_param_field_push_void_negative; do
        refused_code "$name" value_param_collection_mutation '- mode: default'
    done
done
echo "[collection-member-place] PASS (C/LLVM analysis: 15 positives and 20 refusals each; not installed-driver proof); evidence=$REL"
