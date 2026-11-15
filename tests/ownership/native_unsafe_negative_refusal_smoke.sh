#!/usr/bin/env bash
# The native checker refuses lifetime negatives it admitted while their
# programs freed or read freed memory under ASan
# (docs/truth/evidence/2026-11-14_native_unsafe_negative_admissions_remeasured.md
# and the four-leg sweep of 2026-11-15), and still admits the safe
# neighbours of each refusal.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=native-unsafe-negative-refusal
pgy_require_runnable_binary_here "$LABEL" "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/native-unsafe-negative-refusal.XXXXXX)"
F=tests/self_hosted/parity/fixture/collection_field_lifetime
fail() { echo "[$LABEL] $*; evidence=$WORK" >&2; exit 1; }

# fixture|refusal text (one mechanism per text)
MOVED_LATER='is passed to this call and moved by a later argument of the same call'
DEFER_READ='A defer cleanup reads'
OWN_RELEASED='holds String storage that an own hand-off or ArrayDropOwnedStrings released'
OWN_NOT_OWNED='takes ownership of'
OWN_PRODUCER='comes from a call whose result is not a proved owned allocation'
SIBLING_READ='is passed to a call that releases'
CHANGED_LATER='is passed to this call and changed by argument'
UNEXCLUSIVE_RELEASE='is passed to a routine that releases its storage'
NEGATIVES=(
    "inout_index_nested_own_negative|$MOVED_LATER"
    "inout_nested_own_argument_copy_negative|$MOVED_LATER"
    "inout_index_deferred_own_negative|$DEFER_READ"
    "inout_deferred_copy_own_negative|$DEFER_READ"
    "owned_string_join_alias_negative|$OWN_RELEASED"
    "owned_string_local_literal_transfer_alias_negative|$OWN_RELEASED"
    "owned_string_local_literal_transfer_reuse_negative|$OWN_RELEASED"
    "owned_string_member_actual_reused_negative|$OWN_RELEASED"
    "owned_string_named_actual_after_negative|$OWN_RELEASED"
    "owned_string_named_actual_duplicate_negative|$OWN_RELEASED"
    "owned_string_literal_transfer_borrowed_negative|$OWN_NOT_OWNED"
    "owned_string_local_literal_transfer_borrowed_negative|$OWN_NOT_OWNED"
    "owned_string_named_actual_borrowed_negative|$OWN_NOT_OWNED"
    "owned_string_member_actual_borrowed_negative|$OWN_NOT_OWNED"
    "owned_string_local_literal_transfer_allocator_formal_negative|$OWN_NOT_OWNED"
    "owned_string_local_literal_transfer_shadow_negative|$OWN_PRODUCER"
    "owned_string_literal_transfer_aliased_result_negative|$OWN_PRODUCER"
    "read_temporary_releasing_sibling_negative|$SIBLING_READ"
    "exclusive_focus_reads_grown_field_negative|$CHANGED_LATER"
    "exclusive_mutating_sibling_call_negative|$CHANGED_LATER"
    "bundle_field_inout_borrowed_negative|$UNEXCLUSIVE_RELEASE"
    "callable_table_borrowed_negative|$UNEXCLUSIVE_RELEASE"
    "empty_capture_field_assignment_negative|$UNEXCLUSIVE_RELEASE"
    "empty_capture_nested_sibling_alias_negative|$UNEXCLUSIVE_RELEASE"
    "empty_capture_nonempty_negative|$UNEXCLUSIVE_RELEASE"
)
POSITIVES=(
    owned_string_named_actual_positive owned_string_member_actual_positive
    owned_string_local_literal_transfer_positive owned_string_literal_transfer_positive
    owned_string_join_actual_positive inout_deferred_read_positive
    allocator_domain_string_read_positive exclusive_focus_update_positive
    empty_capture_two_fields_positive
)
# A callee that grows a local copy of a by-value formal's field moves that
# field; the caller's other copies of it are copied where they were taken.
COPIED=(exclusive_array_copy_negative exclusive_copy_alias_negative
    exclusive_dead_alias_partial_negative exclusive_grown_field_holder_negative
    exclusive_holder_rebind_branch_negative exclusive_later_holder_negative
    exclusive_plain_alias_grown_alias_negative exclusive_plain_alias_grown_source_negative)
# Programs elsewhere, by path from the repository root. A String read
# through an aggregate member and held past a call that releases or replaces
# that field: bound, built into a constructor, handed back by a call or
# pushed. A member reassigned to a literal and then handed to an own formal.
# An inout aggregate with borrowed parts lent to a declaration with no body.
# The defer cleanups that free storage a later return, own hand-off or
# keeping call took (the weekly flow-summary gate pins their exact sites;
# here they run on every push). An Array dropped while an ArraySort result
# still shares its storage; a permutation of such a shared result is
# admitted (valid_array_builtins).
P=tests/self_hosted/parity/fixture
D=tests/cases/function_param_flow_summary
HELD_PAST_RELEASE='holds a String borrowed from aggregate field'
NO_BODY='which has no body'
SHARED_DROP='borrowed or aliased storage cannot be released'
DEFER_EXIT='A defer cleanup releases Array'
DEFER_PENDING='requires preserved array storage; a prior call may retain or rebind'
PATH_NEGATIVES=(
    "$P/collection_member_place/release_after_constructor_capture_negative|$HELD_PAST_RELEASE"
    "$P/collection_member_place/release_after_identity_call_negative|$HELD_PAST_RELEASE"
    "$P/collection_member_place/release_after_local_element_push_negative|$HELD_PAST_RELEASE"
    "$P/collection_member_place/release_after_local_element_binding_negative|$HELD_PAST_RELEASE"
    "$P/owned_string_argument_sources/reassigned_member_negative|$OWN_NOT_OWNED"
    "$D/extern_inout_borrowed_parts_invalid|$NO_BODY"
    "$D/defer_array_drop_block_kept_invalid|$DEFER_PENDING"
    "$D/defer_array_drop_reader_then_keep_invalid|$DEFER_PENDING"
    "$D/defer_array_drop_return_keep_invalid|$DEFER_PENDING"
    "$D/defer_own_handoff_cleanup_kept_invalid|$DEFER_PENDING"
    "$D/defer_owned_strings_return_constructor_invalid|$DEFER_EXIT"
    "$D/array_result_owned_defer_return_invalid|$DEFER_EXIT"
    "$D/array_sort_result_alias_drop_invalid|$SHARED_DROP"
    "$D/array_reverse_shared_result_after_drop_invalid|$SHARED_DROP"
)
PATH_POSITIVES=(
    "$P/collection_member_place/release_after_compare_and_copy_positive"
    "$P/collection_member_place/release_local_last_use_before_helper_retirement_positive"
    "$P/owned_string_argument_sources/binary_field_positive"
    "$D/return_loan_mutated_literal_opaque_slot_valid"
    "$D/defer_array_drop_reads_valid"
    "$D/defer_owned_strings_read_valid"
    "$D/defer_own_handoff_cleanup_reads_valid"
    src/self_hosted/semantic/fixture/valid_array_builtins
)
refused() {
    local name status=0
    name="$(basename "$1")"
    "$PGY" --native-pipeline --mir-json "$1.pgy" >"$WORK/$name.mir.json" \
        2>"$WORK/$name.err" || status=$?
    [[ "$status" == 1 ]] || fail "$name: expected a semantic refusal, exit $status"
    [[ ! -s "$WORK/$name.mir.json" ]] || fail "$name: published MIR for a refused program"
    grep -Fq "$2" "$WORK/$name.err" || fail "$name: refused for another reason"
}
admitted() {
    local name
    name="$(basename "$1")"
    "$PGY" --native-pipeline --mir-json "$1.pgy" >"$WORK/$name.mir.json" \
        2>"$WORK/$name.err" || fail "$name: safe program refused"
    [[ -s "$WORK/$name.mir.json" ]] || fail "$name: no MIR for an admitted program"
}
for entry in "${NEGATIVES[@]}"; do refused "$F/${entry%%|*}" "${entry#*|}"; done
for entry in "${PATH_NEGATIVES[@]}"; do refused "${entry%%|*}" "${entry#*|}"; done
for name in "${POSITIVES[@]}"; do admitted "$F/$name"; done
for path in "${PATH_POSITIVES[@]}"; do admitted "$path"; done
for name in "${COPIED[@]}"; do
    "$PGY" --native-pipeline --mir-json "$F/$name.pgy" >"$WORK/$name.mir.json" \
        2>"$WORK/$name.err" || fail "$name: copy landing refused"
    grep -Fq '"Clone"' "$WORK/$name.mir.json" || fail "$name: no Clone in MIR"
done
echo "[$LABEL] $(( ${#NEGATIVES[@]} + ${#PATH_NEGATIVES[@]} )) refusals, $(( ${#POSITIVES[@]} + ${#PATH_POSITIVES[@]} )) admissions, ${#COPIED[@]} copy landings PASS"
