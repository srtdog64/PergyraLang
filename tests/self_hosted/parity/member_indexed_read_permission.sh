#!/usr/bin/env bash
# Current source analyzers run; supplied inputs never emit or execute.
set -Eeuo pipefail
trap 'status=$?; echo "[member-indexed-read] failure line=$LINENO status=$status evidence=${REL:-not-created}" >&2' ERR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here member-indexed-read "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/member-indexed-read.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[member-indexed-read] $*; evidence=$REL" >&2; exit 1; }
"${PYTHON_BIN:-python3}" scripts/source_size_count.py --caps <<'CAPS'
120	src/self_hosted/semantic/ast_collection_member_read_permission_owner.pgy
180	src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy
100	src/self_hosted/semantic/ast_collection_member_capture_obligation_owner.pgy
100	src/self_hosted/semantic/ast_collection_member_capture_source_owner.pgy
100	src/self_hosted/semantic/ast_collection_member_capture_site_owner.pgy
80	src/self_hosted/semantic/ast_collection_ownership_member_result_owner.pgy
30	src/self_hosted/semantic/ast_collection_ownership_member_restoration_owner.pgy
CAPS
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
PROBE=tests/self_hosted/fixtures/array_storage_call_preservation_probe.pgy
PUBLICATION=tests/self_hosted/fixtures/collection_member_publication_probe.pgy
SELECTED_SITE=tests/self_hosted/fixtures/collection_member_capture_site_probe.pgy
SOURCE_PROBE=tests/self_hosted/fixtures/collection_member_capture_source_probe.pgy
SCALAR_FIELDS=tests/cases/backend_compare/subject_class_dispatch/main.pgy
CAPTURES=(
    member_capture_direct_negative member_capture_transitive_negative member_capture_constructor_negative
    member_capture_constructor_index_negative member_capture_constructor_return_negative
    member_capture_constructor_temporary_negative member_capture_indexed_negative member_capture_index_target_negative
    member_capture_nominal_clone_negative member_capture_return_negative member_capture_unused_negative member_capture_length_negative
    member_capture_owned_move_positive member_capture_owned_constructor_positive member_capture_fresh_factory_positive
    member_capture_field_clone_positive member_capture_default_positive member_capture_inout_positive
    member_capture_array_push_negative member_capture_array_set_negative member_capture_array_push_index_negative
    member_capture_array_literal_negative member_capture_nested_array_literal_negative
    member_capture_owned_array_push_positive member_capture_owned_array_set_positive
    member_capture_owned_array_literal_positive member_capture_fresh_array_literal_positive
    member_capture_own_call_negative member_capture_struct_literal_negative member_capture_destructure_negative
    member_capture_map_store_negative member_capture_default_call_positive member_capture_ref_call_positive
    member_capture_owned_struct_literal_positive member_capture_owned_destructure_positive
)
MODES=(early capture-unready context-unready digest count read-partial escape-partial nested-invalid success)
REFUSALS=(duplicate array_reuse map_reuse)
ORIGINALS=(
    member_indexed_read_default_positive member_indexed_read_ref_positive member_indexed_read_forwarded_positive
    member_indexed_read_nested_forwarded_positive member_composed_views_positive
    member_indexed_read_root_copy_negative member_indexed_read_root_own_negative member_indexed_read_root_inout_negative
    member_indexed_read_deferred_negative member_indexed_read_forwarded_alias_negative member_indexed_read_forwarded_move_negative
    member_indexed_read_nested_alias_negative member_composed_views_alias_negative member_indexed_read_moved_negative
    member_indexed_read_mutator_negative member_indexed_read_value_formal_negative
)
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "${BASH_SOURCE[0]}" "$PROBE" "$PUBLICATION" "$SELECTED_SITE" "$SOURCE_PROBE" "$SCALAR_FIELDS" >"$WORK/runner.sha256"
for name in "${CAPTURES[@]}"; do sha256sum "$FIXTURES/$name.pgy"; done >"$WORK/capture-inputs.sha256"
for name in "${ORIGINALS[@]}"; do sha256sum "$FIXTURES/$name.pgy"; done >"$WORK/original-inputs.sha256"
for name in "${REFUSALS[@]}"; do sha256sum "tests/self_hosted/fixtures/collection_member_publication_${name}_negative.pgy"; done >"$WORK/publication-inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
date +%s >"$WORK/start.epoch"
echo "[member-indexed-read] fixed member/capture and terminal-publication targets; evidence=$REL"
publication_expected() {
    local mode=$1 ready=false found=false sizes=1:1:1
    if [[ "$mode" == early ]]; then ready=true; found=true; fi
    if [[ "$mode" == read-partial ]]; then sizes=1:2:1; fi
    if [[ "$mode" == escape-partial || "$mode" == nested-invalid ]]; then sizes=1:2:2; fi
    if [[ "$mode" == success ]]; then ready=true; sizes=2:3:3; fi
    printf 'result=%s:71:81:%s\ncapture=91:92\nrestored=2:101:102\nsites=2:111:112\nsizes=%s\nreadable:401=501\nblocked:202=7\nescape:302=902\n' "$found" "$ready" "$sizes"
    if [[ "$mode" == read-partial || "$mode" == escape-partial || "$mode" == nested-invalid || "$mode" == success ]]; then echo blocked:201=1; fi
    if [[ "$mode" == escape-partial || "$mode" == nested-invalid || "$mode" == success ]]; then echo escape:301=901; fi
    if [[ "$mode" == success ]]; then printf 'blocked:203=1\nescape:303=903\nreadable:602=503\n'; fi
}
observe() {
    timeout 15 "$observer" "$FIXTURES/$1.pgy" >"$WORK/$backend-$1.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$WORK/$backend-$1.raw" >"$WORK/$backend-$1.log"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" \
        -o "$observer" >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer did not compile"
    sha256sum "$observer" >>"$WORK/binaries.sha256"
    timeout 15 "$observer" "$SCALAR_FIELDS" >"$WORK/$backend-scalar-fields.raw" 2>&1 || fail "$backend scalar-field observer failed"
    tr -d '\r' <"$WORK/$backend-scalar-fields.raw" >"$WORK/$backend-scalar-fields.log"
    grep -Fxq 'body_ok=true' "$WORK/$backend-scalar-fields.log" || fail "$backend refused scalar receiver fields"
    for name in member_indexed_read_default_positive member_indexed_read_ref_positive member_indexed_read_forwarded_positive \
        member_indexed_read_nested_forwarded_positive member_composed_views_positive; do
        observe "$name"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused $name"
    done
    for name in member_indexed_read_root_copy_negative member_indexed_read_root_own_negative \
        member_indexed_read_root_inout_negative member_indexed_read_deferred_negative \
        member_indexed_read_forwarded_alias_negative member_indexed_read_forwarded_move_negative \
        member_indexed_read_nested_alias_negative member_composed_views_alias_negative; do
        observe "$name"
        grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted $name"
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$name lost ownership diagnosis"
        grep -Fq 'unproved_formal_indexed_read_entry' "$WORK/$backend-$name.log" || fail "$name did not reach indexed-read admission"
    done
    observe member_indexed_read_moved_negative
    grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-member_indexed_read_moved_negative.log" || fail 'moved field lost transfer diagnosis'
    grep -Fq 'member_use_after_move' "$WORK/$backend-member_indexed_read_moved_negative.log" || fail 'moved field bypassed ordered field owner'
    observe member_indexed_read_mutator_negative
    observe member_indexed_read_value_formal_negative
    grep -Fxq 'body_ok=false' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'readonly member crossed a value formal'
    grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal lost borrow diagnosis'
    grep -Fq 'boundary: default' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal bypassed the sequence boundary'
    grep -Fq 'callee: ReadValue' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal lost callee identity'
    grep -Fq 'argument_index: 0' "$WORK/$backend-member_indexed_read_value_formal_negative.log" || fail 'value formal lost ordinal identity'
    grep -Fxq 'body_diagnostic=inout_argument_not_variable' "$WORK/$backend-member_indexed_read_mutator_negative.log" || fail 'member read became copy-out authority'
    for name in "${CAPTURES[@]}"; do
        observe "$name"
        status=0
        timeout 30 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$FIXTURES/$name.pgy" \
            -o "$WORK/native-$backend-$name.exe" >"$WORK/native-$backend-$name.raw" 2>&1 || status=$?
        tr -d '\r' <"$WORK/native-$backend-$name.raw" >"$WORK/native-$backend-$name.log"
        printf '%s:%s=%s\n' "$backend" "$name" "$status" >>"$WORK/native-status.log"
        if [[ "$name" == *_positive ]]; then
            [[ "$status" == 0 ]] || fail "$backend native positive did not compile: $name"
            grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused capture control: $name"
        else
            [[ "$status" == 1 ]] || fail "$backend native refusal missing: $name (status=$status)"
            grep -Fq 'Borrowed ref boundary value' "$WORK/native-$backend-$name.log" || fail "$backend negative failed outside semantic borrow contract: $name"
            grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted capture: $name"
            grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$name.log" || fail "$backend lost capture diagnosis: $name"
        fi
    done
    publisher="$WORK/publication-$backend.exe"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PUBLICATION" \
        -o "$publisher" >"$WORK/$backend-publication.compile.log" 2>&1 || fail "$backend publisher did not compile"
    sha256sum "$publisher" >>"$WORK/binaries.sha256"
    for mode in "${MODES[@]}"; do
        timeout 15 "$publisher" "$mode" >"$WORK/$backend-publication-$mode.raw" 2>&1
        tr -d '\r' <"$WORK/$backend-publication-$mode.raw" >"$WORK/$backend-publication-$mode.log"
        LC_ALL=C sort "$WORK/$backend-publication-$mode.log" >"$WORK/$backend-publication-$mode.sorted"
        publication_expected "$mode" | LC_ALL=C sort >"$WORK/$mode.expected"
        cmp "$WORK/$backend-publication-$mode.sorted" "$WORK/$mode.expected" || fail "$backend publication carrier/order changed: $mode"
    done
    for name in "${REFUSALS[@]}"; do
        status=0
        timeout 30 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
            "tests/self_hosted/fixtures/collection_member_publication_${name}_negative.pgy" \
            -o "$WORK/$backend-publication-$name.exe" >"$WORK/$backend-publication-$name.raw" 2>&1 || status=$?
        [[ "$status" == 1 ]] || fail "$backend publisher transfer refused outside semantic contract: $name (status=$status)"
        tr -d '\r' <"$WORK/$backend-publication-$name.raw" >"$WORK/$backend-publication-$name.log"
        grep -Eq '\[ERROR\].*(own|transferred|released|moved|ownership)' "$WORK/$backend-publication-$name.log" || fail "$backend lost publisher ownership refusal: $name"
    done
    selected_site="$WORK/selected-site-$backend.exe"
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$SELECTED_SITE" \
        -o "$selected_site" >"$WORK/$backend-selected-site.compile.log" 2>&1 || fail "$backend selected-site probe did not compile"
    sha256sum "$selected_site" >>"$WORK/binaries.sha256"
    for mode in missing mode4; do
        timeout 15 "$selected_site" "$FIXTURES/member_capture_own_call_negative.pgy" "$mode" >"$WORK/$backend-selected-site-$mode.raw" 2>&1
        tr -d '\r' <"$WORK/$backend-selected-site-$mode.raw" >"$WORK/$backend-selected-site-$mode.log"
        formal_after=false; if [[ "$mode" == mode4 ]]; then formal_after=true; fi
        printf 'normal=true queries_are_stores=false\nmutation=%s formal_after=%s site_ready=false signature_match=false analysis_match=false\nPASS\n' \
            "$mode" "$formal_after" >"$WORK/selected-site-$mode.expected"
        cmp "$WORK/$backend-selected-site-$mode.log" "$WORK/selected-site-$mode.expected" || fail "$backend selected fact did not fail closed: $mode"
    done
    # Selected-call and source-type guards share one import-composed program.
    timeout 15 "$selected_site" "$SCALAR_FIELDS" source >"$WORK/$backend-capture-source.raw" 2>&1
    tr -d '\r' <"$WORK/$backend-capture-source.raw" >"$WORK/$backend-capture-source.log"
    printf 'scalar_returns=2 unknown_ready=false nominal_receiver_root=true missing_ready=false\nPASS\n' >"$WORK/capture-source.expected"
    cmp "$WORK/$backend-capture-source.log" "$WORK/capture-source.expected" || fail "$backend source facts did not fail closed"
done
for log in "$WORK"/c-member*.log "$WORK"/c-publication-*.log "$WORK"/c-selected-site-*.log "$WORK"/c-scalar-fields.log "$WORK"/c-capture-source.log; do
    name="${log##*/}"; cmp "$log" "$WORK/llvm-${name#c-}" || fail "C/LLVM complete output differs: $name"
done
for manifest in native runner original-inputs capture-inputs publication-inputs imports binaries; do
    sha256sum --quiet -c "$WORK/$manifest.sha256" || fail "$manifest changed during execution"
done
sha256sum "$WORK"/*.log "$WORK"/*.expected >"$WORK/observations.sha256"
date +%s >"$WORK/end.epoch"
echo "[member-indexed-read] PASS (16 original + ${#CAPTURES[@]} capture + scalar receiver source pairs, ${#MODES[@]} full publication pairs, 3 transfer refusals, 2 selected-fact guards and source-type/provenance guards/backend; component scope, not installed-driver proof); evidence=$REL"
