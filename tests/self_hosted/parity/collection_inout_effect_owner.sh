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
    src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy >"$WORK/owners.sha256"
sha256sum "${INPUTS[@]}" >"$WORK/inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$SOURCE_PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-source.exe" >"$WORK/$backend-source.compile" 2>&1
    timeout 120 "$PGY" --native-pipeline "$IDENTITY_PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-identity.exe" >"$WORK/$backend-identity.compile" 2>&1
    for input in "${INPUTS[@]}"; do
        name="${input##*/}"
        timeout 30 "$WORK/$backend-source.exe" "$input" >"$WORK/$backend-$name.raw" 2>"$WORK/$backend-$name.err"
        tr -d '\r' <"$WORK/$backend-$name.raw" >"$WORK/$backend-$name.run"
        [[ ! -s "$WORK/$backend-$name.err" ]]
        case "$name" in
            *_negative.pgy) printf 'body_ok=false\nbody_diagnostic=borrow_boundary_escape\n' >"$WORK/expected" ;;
            *_positive.pgy) printf 'body_ok=true\nbody_diagnostic=\n' >"$WORK/expected" ;;
            *) echo "unclassified inout fixture: $name" >&2; exit 1 ;;
        esac
        cmp "$WORK/expected" "$WORK/$backend-$name.run"
    done
    for ((mutation=0; mutation<=12; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/inout_forward_owned_push_drop_positive.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    echo "[collection-inout-effect] native-$backend: ${#INPUTS[@]} source admissions and thirteen identity/boundary checks PASS"
done
sha256sum -c "$WORK/native.sha256"
sha256sum -c "$WORK/owners.sha256"
sha256sum -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
echo "[collection-inout-effect] evidence: $REL (analyze-only inputs)"
