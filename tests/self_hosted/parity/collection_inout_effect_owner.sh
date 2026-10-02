#!/usr/bin/env bash
# Current-source native C/LLVM analyzers; supplied programs are never emitted/run.
set -Eeuo pipefail
trap 'status=$?; echo "[collection-inout-effect] failed at line $LINENO (status $status); evidence: ${REL:-not-created}" >&2' ERR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-inout-effect "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-inout-effect.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
SOURCE_PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
IDENTITY_PROBE=tests/self_hosted/fixtures/collection_inout_effect_identity_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
INPUTS=(
    member_double_move_negative.pgy member_reuse_while_moved_negative.pgy
    member_distinct_moves_positive.pgy member_restore_then_move_positive.pgy
    inout_borrowed_push_drop_negative.pgy inout_borrowed_push_read_positive.pgy
    inout_owned_push_drop_positive.pgy inout_forward_owned_push_drop_positive.pgy
    inout_readonly_drop_positive.pgy inout_shadow_drop_negative.pgy inout_shadow_read_positive.pgy
    inout_cycle_drop_negative.pgy inout_cycle_read_positive.pgy inout_unknown_then_copy_negative.pgy
    inout_borrowed_then_copy_negative.pgy inout_retired_empty_copy_negative.pgy
    inout_retired_clone_copy_negative.pgy inout_moved_unknown_drop_negative.pgy
    inout_branch_borrow_drop_negative.pgy inout_zero_loop_copy_positive.pgy
    inout_alias_borrow_drop_negative.pgy inout_element_escape_drop_negative.pgy
    inout_branch_own_then_copy_negative.pgy inout_own_formal_retired_copy_negative.pgy
    inout_loop_copy_then_own_negative.pgy inout_copy_then_own_positive.pgy
    inout_nested_own_argument_copy_negative.pgy
    inout_deferred_copy_own_negative.pgy inout_deferred_copy_drop_negative.pgy inout_deferred_read_positive.pgy
    inout_index_eq_owned_positive.pgy inout_index_ne_owned_positive.pgy inout_index_borrowed_read_positive.pgy
    inout_index_forward_copy_positive.pgy inout_index_return_drop_negative.pgy inout_index_alias_escape_drop_negative.pgy
    inout_index_append_drop_negative.pgy inout_index_write_drop_negative.pgy inout_index_deferred_own_negative.pgy
    inout_index_branch_own_negative.pgy inout_index_before_own_positive.pgy inout_index_after_unknown_negative.pgy
    inout_index_before_unknown_positive.pgy inout_index_role_eq_drop_negative.pgy inout_index_role_ne_drop_negative.pgy
    inout_index_after_drop_negative.pgy inout_index_own_formal_drop_negative.pgy inout_index_nested_own_negative.pgy
    inout_index_formal_unknown_negative.pgy inout_index_formal_deferred_unknown_negative.pgy
)
for i in "${!INPUTS[@]}"; do INPUTS[i]="$FIXTURES/${INPUTS[i]}"; done
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$SOURCE_PROBE" "$IDENTITY_PROBE" \
    src/self_hosted/semantic/ast_collection_formal_effect_identity_owner.pgy \
    src/self_hosted/semantic/ast_collection_formal_effect_owner.pgy \
    src/self_hosted/semantic/ast_collection_call_effect_owner.pgy \
    src/self_hosted/semantic/ast_collection_call_retirement_owner.pgy \
    src/self_hosted/semantic/ast_expression_graph_call_argument_edge_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_identity_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy >"$WORK/owners.sha256"
ACTUAL_INPUT="$FIXTURES/callable_table_from_artifact_release_probe.pgy"
sha256sum "${INPUTS[@]}" "$FIXTURES/inout_index_identity_input.pgy" "$ACTUAL_INPUT" >"$WORK/inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$SOURCE_PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-source.exe" >"$WORK/$backend-source.compile" 2>&1
    for input in "${INPUTS[@]}"; do
        name="${input##*/}"
        timeout 30 "$WORK/$backend-source.exe" "$input" >"$WORK/$backend-$name.raw" 2>"$WORK/$backend-$name.err"
        tr -d '\r' <"$WORK/$backend-$name.raw" >"$WORK/$backend-$name.run"
        [[ ! -s "$WORK/$backend-$name.err" ]]
        case "$name" in
            member_*_negative.pgy) printf 'body_ok=false\nbody_diagnostic=move_from_released\n' >"$WORK/expected" ;;
            *_negative.pgy) printf 'body_ok=false\nbody_diagnostic=borrow_boundary_escape\n' >"$WORK/expected" ;;
            *_positive.pgy) printf 'body_ok=true\nbody_diagnostic=\n' >"$WORK/expected" ;;
            *) echo "unclassified inout fixture: $name" >&2; exit 1 ;;
        esac
        cmp "$WORK/expected" "$WORK/$backend-$name.run"
    done
    timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/member_double_move_negative.pgy" diagnostic \
        >"$WORK/$backend-diagnostic.raw" 2>"$WORK/$backend-diagnostic.err"
    tr -d '\r' <"$WORK/$backend-diagnostic.raw" >"$WORK/$backend-diagnostic.run"
    [[ ! -s "$WORK/$backend-diagnostic.err" ]]
    grep -Fxq 'body_ok=false' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_atom=second' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_value=bundle.values' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_function=Main' "$WORK/$backend-diagnostic.run"
    grep -Fxq "body_module=$FIXTURES/member_double_move_negative.pgy" "$WORK/$backend-diagnostic.run"
    grep -Eq '^body_syntax=[0-9]+$' "$WORK/$backend-diagnostic.run"
    for mode_case in missing extra unknown; do
        args=()
        case "$mode_case" in
            missing) printf 'expected source path and optional diagnostic mode\n' >"$WORK/expected" ;;
            extra) args=("$FIXTURES/member_double_move_negative.pgy" diagnostic extra)
                printf 'expected source path and optional diagnostic mode\n' >"$WORK/expected" ;;
            unknown) args=("$FIXTURES/member_double_move_negative.pgy" unknown)
                printf 'unknown source observation mode\n' >"$WORK/expected" ;;
        esac
        if timeout 30 "$WORK/$backend-source.exe" ${args[@]+"${args[@]}"} >"$WORK/$backend-mode-$mode_case.raw" 2>"$WORK/$backend-mode-$mode_case.err"; then
            echo "source observer accepted $mode_case arguments" >&2; exit 1
        else
            [[ "$?" -eq 2 ]]
        fi
        [[ ! -s "$WORK/$backend-mode-$mode_case.err" ]]
        tr -d '\r' <"$WORK/$backend-mode-$mode_case.raw" >"$WORK/$backend-mode-$mode_case.run"
        cmp "$WORK/expected" "$WORK/$backend-mode-$mode_case.run"
    done
    timeout 120 "$PGY" --native-pipeline "$IDENTITY_PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-identity.exe" >"$WORK/$backend-identity.compile" 2>&1
    for ((mutation=0; mutation<=12; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/inout_forward_owned_push_drop_positive.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=13; mutation<=15; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/inout_index_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    timeout 30 "$WORK/$backend-identity.exe" "$ACTUAL_INPUT" -1 \
        >"$WORK/$backend-actual-producer.raw" 2>"$WORK/$backend-actual-producer.err"
    tr -d '\r' <"$WORK/$backend-actual-producer.raw" | LC_ALL=C sort >"$WORK/$backend-actual-producer.run"
    [[ ! -s "$WORK/$backend-actual-producer.err" ]]
    printf 'row_index:0=3\nseed:1=2\nseed:2=2\nseed:3=2\ntables:3=2\ntables:4=2\ntables:5=2\n' >"$WORK/expected"
    cmp "$WORK/expected" "$WORK/$backend-actual-producer.run"
    echo "[collection-inout-effect] native-$backend: ${#INPUTS[@]} source admissions, four observer mode, sixteen identity/boundary and seven actual formal checks PASS"
done
sha256sum -c "$WORK/native.sha256"
sha256sum -c "$WORK/owners.sha256"
sha256sum -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
echo "[collection-inout-effect] evidence: $REL (analyze-only inputs)"
