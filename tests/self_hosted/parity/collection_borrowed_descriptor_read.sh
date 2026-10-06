#!/usr/bin/env bash
# Descriptor read admission plus native value checks for copied type scalars.
# Supplied analyzer inputs are never emitted or executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-borrowed-descriptor-read "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-borrowed-descriptor-read.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[collection-borrowed-descriptor-read] $*; evidence=$REL" >&2; exit 1; }
"${PYTHON_BIN:-python3}" scripts/source_size_count.py --caps <<'CAPS'
115	src/self_hosted/semantic/ast_collection_call_argument_verdict_owner.pgy
400	src/self_hosted/semantic/ast_collection_ownership_scan_owner.pgy
600	src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy
210	src/self_hosted/semantic/ast_collection_call_argument_effect_verdict_owner.pgy
200	src/self_hosted/semantic/ast_collection_call_effect_owner.pgy
170	src/self_hosted/semantic/ast_collection_formal_descriptor_retention_owner.pgy
45	src/self_hosted/semantic/ast_collection_descriptor_retention_entry_owner.pgy
140	src/self_hosted/semantic/ast_collection_terminal_storage_effect_owner.pgy
100	src/self_hosted/semantic/ast_collection_call_retirement_owner.pgy
40	src/self_hosted/semantic/ast_collection_repeated_local_generation_owner.pgy
110	src/self_hosted/semantic/ast_collection_owned_generation_order_owner.pgy
110	src/self_hosted/semantic/ast_collection_owned_argument_admission_owner.pgy
CAPS
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
observe() {
    "$observer" "$FIXTURES/$1.pgy" >"$WORK/$backend-$1.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$WORK/$backend-$1.raw" >"$WORK/$backend-$1.log"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/array_storage_call_preservation_probe.pgy \
        -o "$observer" >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer did not compile"
    for name in inout_borrowed_push_indexed_read_positive inout_owned_push_indexed_read_positive own_generation_indexed_read_positive \
        inout_borrowed_loop_growth_positive inout_borrowed_conditional_loop_growth_positive \
        inout_borrowed_loop_terminal_transfer_positive storage_opaque_return_read_positive \
        borrow_formal_fresh_clone_positive inout_terminal_short_circuit_growth_positive \
        own_terminal_cleanup_positive aggregate_terminal_owned_return_positive \
        inout_terminal_owned_mutation_read_positive inout_terminal_formal_mutation_chain_positive \
        inout_owned_loop_conditional_generation_positive inout_owned_loop_nested_read_positive \
        inout_owned_loop_nested_terminal_cleanup_positive inout_owned_accumulator_loop_positive \
        borrow_formal_indexed_scalar_copy_positive borrow_formal_selected_scalar_copy_positive \
        borrow_formal_mapped_owned_accumulator_positive inout_member_owned_clone_positive \
        member_scalar_owned_accumulator_positive participant_loop_scalar_copy_positive \
        borrow_formal_direct_owned_seed_positive borrow_formal_aggregate_scalar_copy_positive \
        borrow_formal_sync_typed_result_cleanup_positive borrow_formal_member_array_snapshot_positive \
        borrow_formal_row_scalar_copy_positive inout_use_unique_scalar_copy_positive; do
        observe "$name"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused $name"
    done
    for name in borrow_formal_alias_sibling_read_negative borrow_formal_alias_unknown_read_negative \
        borrow_formal_alias_deferred_read_negative own_generation_nonterminal_cleanup_negative \
        own_generation_double_transfer_negative own_event_read_after_negative inout_event_loop_push_drop_negative \
        inout_borrowed_push_sibling_indexed_negative inout_borrowed_push_deferred_indexed_negative \
        inout_borrowed_loop_consume_negative inout_borrowed_loop_sibling_negative \
        inout_terminal_hides_later_retention_negative \
        inout_repeated_descriptor_retention_negative \
        inout_terminal_compound_retention_negative \
        inout_terminal_formal_retention_negative inout_terminal_shallow_sibling_negative \
        aggregate_terminal_borrowed_return_negative aggregate_release_source_reuse_negative \
        aggregate_release_duplicate_storage_negative \
        inout_terminal_owned_mutation_sibling_negative inout_terminal_owned_mutation_capture_negative \
        own_generation_nested_loop_negative inout_event_loop_fresh_defer_negative \
        inout_owned_loop_nested_consumption_negative inout_owned_loop_retired_read_negative \
        own_generation_terminal_hides_consumption_negative \
        inout_owned_loop_nested_break_cleanup_negative inout_owned_loop_nested_continue_cleanup_negative \
        inout_owned_loop_nested_terminal_reuse_negative inout_shallow_accumulator_owned_mutation_negative \
        borrow_formal_indexed_scalar_call_unproved_negative borrow_formal_selected_scalar_return_negative \
        borrow_formal_mapped_shallow_accumulator_negative inout_member_unproved_ownership_negative \
        member_scalar_shallow_accumulator_negative participant_loop_scalar_borrow_negative \
        borrow_formal_retained_owned_seed_negative borrow_formal_aggregate_scalar_return_negative \
        borrow_formal_deferred_typed_result_cleanup_negative borrow_formal_member_array_return_negative \
        borrow_formal_row_scalar_return_negative inout_use_unique_scalar_raw_negative; do
        observe "$name"
        grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
        grep -Eq '^body_diagnostic=(borrow_boundary_escape|move_from_released)$' "$WORK/$backend-$name.log" || fail "$name lost ownership diagnosis"
        case "$name" in
            inout_use_unique_scalar_raw_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost formal mutation diagnosis"
                # The raw insertion makes AppendUnique's text formal retaining, so
                # the borrowed element is refused where it enters that formal.
                grep -Fq 'unproved_formal_element_use_entry' "$WORK/$backend-$name.log" || fail "$name bypassed raw insertion refusal" ;;
            borrow_formal_member_array_return_negative)
                grep -Fq -- '- callee: Publish' "$WORK/$backend-$name.log" || fail "$name lost ordinary publication boundary"
                grep -Fq -- '- argument_index: 0' "$WORK/$backend-$name.log" || fail "$name lost physical argument identity" ;;
            borrow_formal_deferred_typed_result_cleanup_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost execution-context diagnosis"
                grep -Fq 'unproved_formal_execution_context' "$WORK/$backend-$name.log" || fail "$name bypassed deferred context refusal" ;;
            participant_loop_scalar_borrow_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost unproved-callee diagnosis"
                grep -Fq 'boundary: ArrayPushOwnedString' "$WORK/$backend-$name.log" || fail "$name bypassed repeated call-effect refusal" ;;
            member_scalar_shallow_accumulator_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost shallow-element diagnosis"
                grep -Fq 'boundary: owned_string_drop' "$WORK/$backend-$name.log" || fail "$name bypassed copied-element obligation" ;;
            inout_member_unproved_ownership_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost mutable-entry diagnosis"
                grep -Fq 'unproved_inout_copy_entry' "$WORK/$backend-$name.log" || fail "$name bypassed owned element/storage entry" ;;
            inout_owned_loop_nested_terminal_reuse_negative)
                grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-$name.log" || fail "$name lost transfer-use diagnosis"
                grep -Fq 'owned_argument_use_after_move' "$WORK/$backend-$name.log" || fail "$name bypassed ordered transfer use" ;;
            inout_owned_loop_nested_break_cleanup_negative|inout_owned_loop_nested_continue_cleanup_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost unique-entry diagnosis"
                grep -Fq 'owned_argument_storage_not_live' "$WORK/$backend-$name.log" || fail "$name bypassed unique-entry proof" ;;
            borrow_formal_row_scalar_return_negative|borrow_formal_aggregate_scalar_return_negative|borrow_formal_retained_owned_seed_negative|borrow_formal_indexed_scalar_call_unproved_negative|borrow_formal_selected_scalar_return_negative|borrow_formal_mapped_shallow_accumulator_negative)
                grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost borrowed-element diagnosis"
                grep -Fq 'unproved_formal_element_use_entry' "$WORK/$backend-$name.log" || fail "$name bypassed scalar argument proof" ;;
        esac
    done
    "$observer" tests/self_hosted/fixtures/mir_match_fact_snapshot_probe.pgy \
        >"$WORK/$backend-match-owner-source.log" 2>&1 || fail "$backend match owner source observer failed"
    grep -Fxq 'body_ok=true' "$WORK/$backend-match-owner-source.log" || fail "$backend refused imported match owner"
    "$observer" tests/self_hosted/fixtures/mir_runtime_abi_local_type_scalar_probe.pgy \
        >"$WORK/$backend-runtime-abi-source.log" 2>&1 || fail "$backend runtime ABI source observer failed"
    grep -Fxq 'body_ok=true' "$WORK/$backend-runtime-abi-source.log" || fail "$backend refused imported runtime ABI owner"
done
echo "[collection-borrowed-descriptor-read] PASS (C/LLVM analysis: 28 fixture positives, 41 compile-only refusals and imported match/runtime ABI owners each; not installed-driver proof); evidence=$REL"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/collection_definition_type_scalar_probe.pgy \
        -o "$REL/type-scalar-$backend.exe" >"$WORK/$backend-type-scalar.compile.log" 2>&1 ||
        fail "$backend type scalar value probe did not compile"
    timeout 30 "$WORK/type-scalar-$backend.exe" >"$WORK/$backend-type-scalar.raw" 2>&1 ||
        fail "$backend type scalar value probe failed"
    tr -d '\r' <"$WORK/$backend-type-scalar.raw" >"$WORK/$backend-type-scalar.log"
    [[ "$(cat "$WORK/$backend-type-scalar.log")" == 'ARRAY TYPE SCALAR COPY PASS' ]] ||
        fail "$backend String-array classification changed"
done
echo "[collection-borrowed-descriptor-read] native C/LLVM copied type classification values PASS; not whole definition/installed proof"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$FIXTURES/borrow_formal_sync_typed_result_cleanup_positive.pgy" \
        -o "$REL/typed-result-$backend.exe" >"$WORK/$backend-typed-result.compile.log" 2>&1 ||
        fail "$backend typed result cleanup did not compile"
    timeout 30 "$WORK/typed-result-$backend.exe" >"$WORK/$backend-typed-result.raw" 2>&1 ||
        fail "$backend typed result cleanup failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-typed-result.raw")" == 'TYPED VERDICT CLEANUP PASS' ]] ||
        fail "$backend typed success/failure result changed"
done
echo "[collection-borrowed-descriptor-read] native C/LLVM typed success/failure cleanup values PASS; not installed-driver proof"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        "$FIXTURES/borrow_formal_member_array_snapshot_positive.pgy" \
        -o "$REL/member-snapshot-$backend.exe" >"$WORK/$backend-member-snapshot.compile.log" 2>&1 ||
        fail "$backend member array snapshot did not compile"
    timeout 30 "$WORK/member-snapshot-$backend.exe" >"$WORK/$backend-member-snapshot.raw" 2>&1 ||
        fail "$backend member array snapshot failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-member-snapshot.raw")" == 'MEMBER ARRAY SNAPSHOT PASS' ]] ||
        fail "$backend array snapshot changed with the source array"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM array snapshot independent of source mutation PASS; no member-array cleanup or installed-driver claim"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/mir_collection_binding_type_scalar_probe.pgy \
        -o "$REL/mir-binding-type-$backend.exe" >"$WORK/$backend-mir-binding-type.compile.log" 2>&1 ||
        fail "$backend MIR binding type scalar did not compile"
    timeout 30 "$WORK/mir-binding-type-$backend.exe" >"$WORK/$backend-mir-binding-type.raw" 2>&1 ||
        fail "$backend MIR binding type scalar failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-mir-binding-type.raw")" == 'MIR BINDING TYPE SCALAR COPY PASS' ]] ||
        fail "$backend binding type value or missing/duplicate guard changed"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM MIR binding type values with explicit caller copies and missing/duplicate guards PASS; no ordinary String-result lifetime or installed-driver claim"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/mir_match_fact_snapshot_probe.pgy \
        -o "$REL/mir-match-$backend.exe" >"$WORK/$backend-mir-match.compile.log" 2>&1 ||
        fail "$backend MIR match fact snapshot did not compile"
    timeout 30 "$WORK/mir-match-$backend.exe" >"$WORK/$backend-mir-match.raw" 2>&1 ||
        fail "$backend MIR match fact snapshot failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-mir-match.raw")" == 'MIR MATCH FACT SNAPSHOT PASS' ]] ||
        fail "$backend match rows changed after input cleanup or invalid attachment"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM match fact snapshot and invalid-case no-mutation PASS; not installed-driver proof"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/mir_runtime_abi_local_type_scalar_probe.pgy \
        -o "$REL/mir-runtime-abi-$backend.exe" >"$WORK/$backend-mir-runtime-abi.compile.log" 2>&1 ||
        fail "$backend runtime ABI type scalar did not compile"
    timeout 30 "$WORK/mir-runtime-abi-$backend.exe" >"$WORK/$backend-mir-runtime-abi.raw" 2>&1 ||
        fail "$backend runtime ABI type scalar failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-mir-runtime-abi.raw")" == 'MIR RUNTIME ABI TYPE SCALAR COPY PASS' ]] ||
        fail "$backend runtime ABI last-binding lookup or missing/malformed guard changed"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM runtime ABI type values with explicit caller copies and last-binding/missing/malformed guards PASS; not installed-driver proof"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/mir_instruction_use_snapshot_probe.pgy \
        -o "$REL/mir-instruction-use-$backend.exe" >"$WORK/$backend-mir-instruction-use.compile.log" 2>&1 ||
        fail "$backend MIR instruction use snapshot did not compile"
    timeout 30 "$WORK/mir-instruction-use-$backend.exe" >"$WORK/$backend-mir-instruction-use.raw" 2>&1 ||
        fail "$backend MIR instruction use snapshot failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-mir-instruction-use.raw")" == 'MIR INSTRUCTION USE SNAPSHOT PASS' ]] ||
        fail "$backend retained uses or instruction/block ranges changed"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM instruction use snapshot and empty append offsets PASS; no destination deep-drop or installed-driver claim"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/mir_local_ref_snapshot_probe.pgy \
        -o "$REL/mir-local-ref-$backend.exe" >"$WORK/$backend-mir-local-ref.compile.log" 2>&1 ||
        fail "$backend MIR LocalRef snapshot did not compile"
    timeout 30 "$WORK/mir-local-ref-$backend.exe" >"$WORK/$backend-mir-local-ref.raw" 2>&1 ||
        fail "$backend MIR LocalRef snapshot failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-mir-local-ref.raw")" == 'MIR LOCAL REF SNAPSHOT PASS' ]] ||
        fail "$backend LocalRef selection, retained values, guards or append ranges changed"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM LocalRef snapshot, shadowing/shape/repeat guards and append ranges PASS; no destination deep-drop or installed-driver claim"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/mir_unique_use_snapshot_probe.pgy \
        -o "$REL/mir-unique-use-$backend.exe" >"$WORK/$backend-mir-unique-use.compile.log" 2>&1 ||
        fail "$backend MIR unique-use snapshot did not compile"
    timeout 30 "$WORK/mir-unique-use-$backend.exe" >"$WORK/$backend-mir-unique-use.raw" 2>&1 ||
        fail "$backend MIR unique-use snapshot failed"
    [[ "$(tr -d '\r' <"$WORK/$backend-mir-unique-use.raw")" == 'MIR UNIQUE USE SNAPSHOT PASS' ]] ||
        fail "$backend use text/order/deduplication or invalid-graph guard changed"
done

echo "[collection-borrowed-descriptor-read] native C/LLVM unique-use snapshot, deduplication/order and invalid-graph no-mutation PASS; no destination deep-drop or installed-driver claim"
