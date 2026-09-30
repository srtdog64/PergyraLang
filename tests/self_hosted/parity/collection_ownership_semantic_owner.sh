#!/usr/bin/env bash
# Collection element ownership is decided by one semantic owner before MIR.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-collection-ownership"
FOCUS="${PGY_COLLECTION_OWNERSHIP_FOCUS:-all}"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy"
STATEMENT_TRANSITION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_statement_transition_owner.pgy"
IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_identity_owner.pgy"
STATE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_state_owner.pgy"
MEMBER_MOVE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy"
MEMBER_TRANSITION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy"
BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
DIRECT_MOVE_PROBE="tests/self_hosted/parity/fixture/collection_ownership_binding_move_direct_c_probe.pgy"
FIELD_FIXTURE_DIR="tests/self_hosted/parity/fixture/collection_field_lifetime"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
case "$FOCUS" in
    all|owned-parameter|owned-parameter-self-host|aggregate-field) ;;
    *) fail "unknown PGY_COLLECTION_OWNERSHIP_FOCUS=$FOCUS" ;;
esac
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
# The public launcher must consume the same selected driver, not a sibling
# installed binary when a private acceptance pair is supplied.
export PGY_SELF_DRIVER_BIN="$(pgy_path_for_compiler "$PGY" "$DRIVER")"

grep -Fq 'SemanticAstCollectionOwnershipVerdictFromResolvedFacts(' "$OWNER" ||
    fail "semantic collection ownership owner is missing"
grep -Fq 'SemanticExpressionGraphCallTargetSyntaxId(' "$IDENTITY_OWNER" ||
    fail "owner does not distinguish builtins from same-name declarations"
grep -Fq 'SemanticExpressionGraphRuntimeCallAbiId(' "$IDENTITY_OWNER" ||
    fail "owner does not consume carried runtime call identity"
grep -Fq 'SemanticAstScopedLocalBindingIdentityForGraphLeaf(' "$IDENTITY_OWNER" ||
    fail "owner joins collection state by spelling instead of binding identity"
grep -Fq 'import "ast_collection_ownership_statement_transition_owner.pgy";' \
    "$OWNER" ||
    fail "verdict owner does not import the collection statement transition owner"
grep -Fq 'SemanticAstCollectionStatementTransitions(' "$OWNER" ||
    fail "verdict owner does not consume collection statement transitions"
for statement_tag in TypedAstKindArrayPushStmtTag \
        TypedAstKindArraySetStmtTag TypedAstKindArrayPopStmtTag; do
    grep -Fq "$statement_tag()" "$STATEMENT_TRANSITION_OWNER" ||
        fail "statement transition owner ignores $statement_tag"
done
grep -Fq 'SemanticAstCollectionOwnershipVerdictFromResolvedFacts(' "$BUNDLE" ||
    fail "body admission does not consume collection ownership verdict"
grep -Fq 'SemanticAstCollectionMemberMoveIdentityForNode(' "$MEMBER_MOVE_OWNER" ||
    fail "aggregate member move lacks one stable identity owner"
grep -Fq 'SemanticAstCollectionFirstInvalidMemberMoveUse(' \
    "$MEMBER_TRANSITION_OWNER" ||
    fail "aggregate member move lacks ordered transition validation"
grep -Fq 'SemanticAstCollectionOriginClone()' "$STATE_OWNER" ||
    fail "explicit Clone origin is missing from the collection state owner"
grep -Fq 'SemanticAstCollectionOriginCallResult()' "$STATE_OWNER" ||
    fail "unknown call-result origin is missing from the collection state owner"
! grep -Fq 'Slot<' "$OWNER" "$IDENTITY_OWNER" "$STATE_OWNER" \
        "$MEMBER_MOVE_OWNER" "$MEMBER_TRANSITION_OWNER" ||
    fail "ordinary collection ownership imported Slot semantics"

mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection_ownership_semantic_owner.XXXXXX")"
WORK_REL=".tmp/self_hosted/${WORK_DIR##*/}"
echo "[$LABEL] focus=$FOCUS evidence=$WORK_REL launcher=$PGY driver=$DRIVER"

# An own formal transfers storage, not necessarily its String elements. Its
# deep-release requirement must survive source-call forwarding and exact
# builtin identity. The production-only selector names its narrower scope;
# strict parity continues to expose any native bootstrap disagreement.
if [[ "$FOCUS" == owned-parameter || "$FOCUS" == owned-parameter-self-host ]]; then
    OWN_LANES=(public)
    [[ "$FOCUS" == owned-parameter ]] && OWN_LANES+=(native)
    for row in own_wrapper_borrowed_negative:borrow_boundary_escape \
            own_member_borrowed_negative:borrow_boundary_escape \
            own_inline_borrowed_negative:named_value_boundary_argument_required \
            own_formal_double_forward_negative:move_from_released \
            own_formal_read_after_forward_negative:move_from_released; do
        name="${row%%:*}"
        diagnostic="${row#*:}"
        source="$FIELD_FIXTURE_DIR/$name.pgy"
        self_rel="$WORK_REL/$name.mir.json"
        printf '%s\n' "preserved:$name:self" >"$ROOT_DIR/$self_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
            "$source" -o "$self_rel") >"$WORK_DIR/$name-self.out" \
            2>"$WORK_DIR/$name-self.err"; then
            fail "self-host accepted $name (expected $diagnostic)"
        fi
        [[ "$(cat "$ROOT_DIR/$self_rel")" == "preserved:$name:self" ]] ||
            fail "self-host refusal replaced the prior argument artifact for $name"
        grep -Fq "$diagnostic" \
            "$WORK_DIR/$name-self.out" "$WORK_DIR/$name-self.err" ||
            fail "self-host lost $diagnostic for $name"
        for backend in c llvm; do
            for lane in "${OWN_LANES[@]}"; do
                command=("$PGY")
                # Native text diagnostics omit stable codes. Read the owned
                # JSON diagnostic so a generic compiler refusal cannot pass.
                [[ "$lane" == native ]] && command+=(--native-pipeline --error-format=json)
                output_rel="$WORK_REL/$name-$lane-$backend.exe"
                command+=("$source" "--backend=$backend" -o "$output_rel")
                if (cd "$ROOT_DIR" && "${command[@]}") \
                    >"$WORK_DIR/$name-$lane-$backend.out" \
                    2>"$WORK_DIR/$name-$lane-$backend.err"; then
                    fail "$lane $backend accepted $name (expected $diagnostic)"
                fi
                [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
                    fail "$lane $backend published the rejected argument for $name"
                diagnostic_pattern="$diagnostic"
                case "$lane:$name:$diagnostic" in
                    native:own_member_borrowed_negative:*|native:own_inline_borrowed_negative:*)
                        diagnostic_pattern='must use a named variable' ;;
                    native:*:borrow_boundary_escape)
                        diagnostic_pattern='(borrow_boundary_escape|PGY_SEM_BORROW_ESCAPE)' ;;
                    native:*:move_from_released)
                        diagnostic_pattern='PGY_SEM_MOVE_FROM_RELEASED' ;;
                esac
                grep -Eq "$diagnostic_pattern" \
                    "$WORK_DIR/$name-$lane-$backend.out" \
                    "$WORK_DIR/$name-$lane-$backend.err" ||
                    fail "$lane $backend refusal lost $diagnostic for $name"
            done
        done
    done
    for row in shadow_owned_drop_callable_positive:compile-only \
            own_wrapper_owned_positive:owned-wrapper-retired \
            own_named_clone_positive:named-clone-retired; do
        name="${row%%:*}"
        source="$FIELD_FIXTURE_DIR/$name.pgy"
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
            "$source" -o "$WORK_REL/$name.mir.json") \
            >"$WORK_DIR/$name-self.out" 2>"$WORK_DIR/$name-self.err" ||
            fail "self-host rejected exact own-formal evidence for $name"
        [[ -s "$WORK_DIR/$name.mir.json" ]] ||
            fail "self-host emitted no own-formal MIR for $name"
        for backend in c llvm; do
            for lane in "${OWN_LANES[@]}"; do
                command=("$PGY")
                [[ "$lane" == native ]] && command+=(--native-pipeline)
                command+=("$source" "--backend=$backend" \
                    -o "$WORK_REL/$name-$lane-$backend.exe")
                [[ "${row#*:}" == compile-only ]] || command+=(--run)
                (cd "$ROOT_DIR" && "${command[@]}") \
                    >"$WORK_DIR/$name-$lane-$backend.out" \
                    2>"$WORK_DIR/$name-$lane-$backend.err" ||
                    fail "$lane $backend rejected exact own-formal evidence for $name"
                [[ -s "$WORK_DIR/$name-$lane-$backend.exe" ]] ||
                    fail "$lane $backend emitted no own-formal artifact for $name"
                if [[ "${row#*:}" != compile-only ]]; then
                    tr -d '\r' <"$WORK_DIR/$name-$lane-$backend.out" |
                        sed '/^pgy:/d' >"$WORK_DIR/$name-$lane-$backend.run"
                    [[ "$(cat "$WORK_DIR/$name-$lane-$backend.run")" == "${row#*:}" ]] ||
                        fail "$lane $backend own-formal output drifted for $name"
                fi
            done
        done
    done
    if [[ "$FOCUS" == owned-parameter ]]; then
        echo "[$LABEL] focused own-formal native/self-host/public C/LLVM parity PASS (not aggregate/full closure)"
    else
        echo "[$LABEL] focused own-formal self-host/public C/LLVM boundary PASS (not native parity or aggregate/full closure)"
    fi
    exit 0
fi

NEGATIVE_CASES=(
    borrowed_string_array_deep_drop
    borrowed_string_array_shallow_assignment
    empty_shallow_string_push_drop
    empty_mixed_string_push_without_drop
    empty_string_array_shallow_copy
    empty_string_array_assignment_shallow_copy
    empty_owned_string_double_drop
    inout_string_array_deep_drop
    map_keys_shallow_push
    map_keys_shallow_set
    map_keys_shallow_pop
    map_keys_double_drop
    collection_field_owned_push
    collection_field_deep_drop
    unknown_string_array_alias_drop
    unknown_string_array_assignment_without_drop
    collection_parameter_direct_field_deep_drop
)
if [[ "$FOCUS" == aggregate-field ]]; then
    NEGATIVE_CASES=(
        bundle_field_byvalue_borrowed_negative
        bundle_field_inout_borrowed_negative
        callable_table_borrowed_negative
    )
fi

for name in "${NEGATIVE_CASES[@]}"; do
    source="tests/concept_semantics/hashmap/$name.pgy"
    [[ "$FOCUS" == aggregate-field ]] && source="$FIELD_FIXTURE_DIR/$name.pgy"
    self_rel="$WORK_REL/self-$name.mir.json"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$source" -o "$self_rel") >"$WORK_DIR/self-$name.out" \
        2>"$WORK_DIR/self-$name.err"; then
        fail "installed self-host accepted $name"
    fi
    [[ ! -e "$ROOT_DIR/$self_rel" ]] ||
        fail "installed self-host published MIR for rejected $name"
    grep -Fq 'borrow_boundary_escape' \
        "$WORK_DIR/self-$name.out" "$WORK_DIR/self-$name.err" ||
        fail "installed self-host lost ownership diagnostic for $name"

    for backend in c llvm; do
        public_rel="$WORK_REL/public-$backend-$name.exe"
        if (cd "$ROOT_DIR" && "$PGY" "$source" "--backend=$backend" \
            -o "$public_rel") >"$WORK_DIR/public-$backend-$name.out" \
            2>"$WORK_DIR/public-$backend-$name.err"; then
            fail "public $backend path accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$public_rel" ]] ||
            fail "public $backend path published a rejected artifact for $name"
        grep -Fq 'borrow_boundary_escape' \
            "$WORK_DIR/public-$backend-$name.out" \
            "$WORK_DIR/public-$backend-$name.err" ||
            fail "public $backend path lost ownership diagnostic for $name"

        native_rel="$WORK_REL/native-$backend-$name.exe"
        if (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$source" \
            "--backend=$backend" -o "$native_rel") \
            >"$WORK_DIR/native-$backend-$name.out" \
            2>"$WORK_DIR/native-$backend-$name.err"; then
            fail "native $backend path accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$native_rel" ]] ||
            fail "native $backend path published a rejected artifact for $name"
        grep -Eq '\[ERROR\]|MIR validation failed' \
            "$WORK_DIR/native-$backend-$name.out" \
            "$WORK_DIR/native-$backend-$name.err" ||
            fail "native $backend path lost explicit diagnostic for $name"
    done
done

if [[ "$FOCUS" == aggregate-field ]]; then
    # Actual release-owner controls and the FromArtifact producer are candidates
    # for the still-open field seam, not default CI or installed DRV-2 evidence.
    for row in callable_table_empty_release_positive:empty-table-retired \
            callable_table_owned_release_positive:owned-table-retired \
            callable_table_from_artifact_release_probe:producer-table-retired; do
        name="${row%%:*}"
        source="$FIELD_FIXTURE_DIR/$name.pgy"
        mir_rel="$WORK_REL/$name.mir.json"
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
            "$source" -o "$mir_rel") >"$WORK_DIR/$name-self.out" \
            2>"$WORK_DIR/$name-self.err" ||
            fail "self-host rejected the actual callable-table lifecycle for $name"
        [[ -s "$ROOT_DIR/$mir_rel" ]] ||
            fail "self-host emitted no callable-table MIR for $name"
        printf '%s\n' "${row#*:}" >"$WORK_DIR/$name.expected"
        for backend in c llvm; do
            for lane in public native; do
                command=("$PGY")
                [[ "$lane" == native ]] && command+=(--native-pipeline)
                command+=("$source" "--backend=$backend" --run \
                    -o "$WORK_REL/$name-$lane-$backend.exe")
                (cd "$ROOT_DIR" && "${command[@]}") \
                    >"$WORK_DIR/$name-$lane-$backend.out" \
                    2>"$WORK_DIR/$name-$lane-$backend.err" ||
                    fail "$lane $backend rejected callable-table release for $name"
                tr -d '\r' <"$WORK_DIR/$name-$lane-$backend.out" |
                    sed '/^pgy:/d' >"$WORK_DIR/$name-$lane-$backend.run"
                cmp -s "$WORK_DIR/$name.expected" \
                    "$WORK_DIR/$name-$lane-$backend.run" ||
                    fail "$lane $backend callable-table retirement drifted for $name"
            done
        done
    done
    echo "[$LABEL] focused aggregate-field producer/release C/LLVM parity PASS (not whole-compiler/full SoT closure)"
    exit 0
fi

UNKNOWN_SOURCE="tests/concept_semantics/hashmap/unknown_string_array_drop.pgy"
UNKNOWN_SELF_REL="$WORK_REL/unknown-self.mir.json"
printf '%s\n' preserved:unknown:self >"$ROOT_DIR/$UNKNOWN_SELF_REL"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$UNKNOWN_SOURCE" -o "$UNKNOWN_SELF_REL") \
    >"$WORK_DIR/unknown-self.out" 2>"$WORK_DIR/unknown-self.err"; then
    fail "installed self-host accepted unknown-provenance deep drop"
fi
[[ "$(cat "$ROOT_DIR/$UNKNOWN_SELF_REL")" == preserved:unknown:self ]] ||
    fail "installed self-host refusal replaced the prior unknown artifact"
grep -Fq 'borrow_boundary_escape' \
    "$WORK_DIR/unknown-self.out" "$WORK_DIR/unknown-self.err" ||
    fail "installed self-host lost the unknown-provenance diagnostic"
for backend in c llvm; do
    output_rel="$WORK_REL/unknown-public-$backend.exe"
    rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$PGY" "$UNKNOWN_SOURCE" "--backend=$backend" \
        -o "$output_rel") >"$WORK_DIR/unknown-public-$backend.out" \
        2>"$WORK_DIR/unknown-public-$backend.err"; then
        fail "public $backend accepted unknown-provenance deep drop"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
        fail "public $backend refusal left a stale unknown artifact"
    grep -Fq 'borrow_boundary_escape' \
        "$WORK_DIR/unknown-public-$backend.out" \
        "$WORK_DIR/unknown-public-$backend.err" ||
        fail "public $backend lost the unknown-provenance diagnostic"
done

EMPTY_VALID="tests/concept_semantics/hashmap/empty_owned_string_push_drop_valid.pgy"
EMPTY_MIR_REL="$WORK_REL/empty-valid.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$EMPTY_VALID" \
    -o "$EMPTY_MIR_REL") >"$WORK_DIR/empty-valid-self.out" \
    2>"$WORK_DIR/empty-valid-self.err" ||
    fail "installed self-host rejected exact empty-to-owned transition"
[[ -s "$ROOT_DIR/$EMPTY_MIR_REL" ]] ||
    fail "exact empty-to-owned transition emitted no MIR"
[[ "$(grep -Fc '"kind":"owned-string-push"' "$ROOT_DIR/$EMPTY_MIR_REL")" == 1 ]] ||
    fail "owned-string push receipt did not attach exactly once"
[[ "$(grep -Fc '"kind":"drop"' "$ROOT_DIR/$EMPTY_MIR_REL")" == 1 ]] ||
    fail "drop receipt did not attach exactly once"
grep -Fq '"collection_ownership_receipt":null' "$ROOT_DIR/$EMPTY_MIR_REL" ||
    fail "receipt-free instructions lost the explicit null wire shape"

for backend in c llvm; do
    direct_rel="$WORK_REL/empty-valid-direct.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$EMPTY_MIR_REL" -o "$direct_rel") \
        >"$WORK_DIR/empty-valid-direct-$backend.out" \
        2>"$WORK_DIR/empty-valid-direct-$backend.err" ||
        fail "direct $backend consumer rejected the exact receipt MIR"
    [[ -s "$ROOT_DIR/$direct_rel" ]] ||
        fail "direct $backend consumer emitted no artifact"
done
[[ "$(grep -Fc '    pgy_as_drop_owned(&pgy_local_0);' \
    "$WORK_DIR/empty-valid-direct.c")" == 1 ]] ||
    fail "direct C did not emit exactly one explicit owned drop"
! grep -Fq 'pgy_as_drop_storage(&pgy_local_0)' \
    "$WORK_DIR/empty-valid-direct.c" ||
    fail "direct C added automatic cleanup after explicit drop"
[[ "$(grep -Fc '  call void @pgy_as_drop_owned(ptr %pgy.local.0)' \
    "$WORK_DIR/empty-valid-direct.llvm")" == 1 ]] ||
    fail "direct LLVM did not emit exactly one explicit owned drop"
! grep -Fq 'call void @pgy_as_drop_storage(ptr %pgy.local.0)' \
    "$WORK_DIR/empty-valid-direct.llvm" ||
    fail "direct LLVM added automatic cleanup after explicit drop"

python "$ROOT_DIR/tests/self_hosted/parity/collection_ownership_receipt_mutations.py" \
    "$ROOT_DIR/$EMPTY_MIR_REL" "$WORK_DIR"
for mutation in missing-push missing-drop all-missing wrong-binding wrong-kind \
        moved-receipt duplicate-receipt wrong-source-binding; do
    for backend in c llvm; do
        mutated_rel="$WORK_REL/$mutation.mir.json"
        output_rel="$WORK_REL/$mutation.$backend"
        marker="preserved:$mutation:$backend"
        printf '%s\n' "$marker" >"$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation-$backend.out" \
            2>"$WORK_DIR/$mutation-$backend.err"; then
            fail "direct $backend consumer accepted $mutation"
        fi
        [[ "$(cat "$ROOT_DIR/$output_rel")" == "$marker" ]] ||
            fail "direct $backend refusal replaced the prior artifact for $mutation"
        grep -Eq '(CODEGEN ERROR|MIR-LOWER ERROR):' \
            "$WORK_DIR/$mutation-$backend.out" \
            "$WORK_DIR/$mutation-$backend.err" ||
            fail "direct $backend refusal lost its diagnostic for $mutation"
    done
done

for backend in c llvm; do
    (cd "$ROOT_DIR" && "$PGY" "$EMPTY_VALID" "--backend=$backend" --run \
        -o "$WORK_REL/empty-valid-$backend.exe") \
        >"$WORK_DIR/empty-valid-$backend.out" \
        2>"$WORK_DIR/empty-valid-$backend.err" ||
        fail "public $backend path rejected exact empty-to-owned transition"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$EMPTY_VALID" \
        "--backend=$backend" --run \
        -o "$WORK_REL/empty-valid-native-$backend.exe") \
        >"$WORK_DIR/empty-valid-native-$backend.out" \
        2>"$WORK_DIR/empty-valid-native-$backend.err" ||
        fail "native $backend path rejected exact empty-to-owned transition"
done

CONDITIONAL_VALID="tests/concept_semantics/hashmap/empty_owned_string_conditional_push_drop_valid.pgy"
CONDITIONAL_MIR_REL="$WORK_REL/conditional-valid.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$CONDITIONAL_VALID" -o "$CONDITIONAL_MIR_REL") \
    >"$WORK_DIR/conditional-valid-self.out" \
    2>"$WORK_DIR/conditional-valid-self.err" ||
    fail "installed self-host rejected multi-block empty-to-owned transition"
[[ "$(grep -Fc '"kind":"owned-string-push"' \
    "$ROOT_DIR/$CONDITIONAL_MIR_REL")" == 1 ]] ||
    fail "multi-block owned-string push receipt did not attach exactly once"
[[ "$(grep -Fc '"kind":"drop"' "$ROOT_DIR/$CONDITIONAL_MIR_REL")" == 1 ]] ||
    fail "multi-block drop receipt did not attach exactly once"

for backend in c llvm; do
    direct_rel="$WORK_REL/conditional-valid-direct.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$CONDITIONAL_MIR_REL" -o "$direct_rel") \
        >"$WORK_DIR/conditional-valid-direct-$backend.out" \
        2>"$WORK_DIR/conditional-valid-direct-$backend.err" ||
        fail "direct $backend consumer rejected multi-block receipt MIR"
    [[ -s "$ROOT_DIR/$direct_rel" ]] ||
        fail "direct $backend consumer emitted no multi-block artifact"
done
[[ "$(grep -Fc '    pgy_as_drop_owned(&pgy_local_0);' \
    "$WORK_DIR/conditional-valid-direct.c")" == 1 ]] ||
    fail "direct C did not emit exactly one multi-block owned drop"
! grep -Fq 'pgy_as_drop_storage(&pgy_local_0)' \
    "$WORK_DIR/conditional-valid-direct.c" ||
    fail "direct C retained legacy cleanup for a tracked multi-block local"
[[ "$(grep -Fc '  call void @pgy_as_drop_owned(ptr %pgy.local.0)' \
    "$WORK_DIR/conditional-valid-direct.llvm")" == 1 ]] ||
    fail "direct LLVM did not emit exactly one multi-block owned drop"
! grep -Fq 'call void @pgy_as_drop_storage(ptr %pgy.local.0)' \
    "$WORK_DIR/conditional-valid-direct.llvm" ||
    fail "direct LLVM retained legacy cleanup for a tracked multi-block local"

CONDITIONAL_MUTATIONS="$WORK_DIR/conditional-mutations"
python "$ROOT_DIR/tests/self_hosted/parity/collection_ownership_receipt_mutations.py" \
    "$ROOT_DIR/$CONDITIONAL_MIR_REL" "$CONDITIONAL_MUTATIONS"
for mutation in missing-push missing-drop all-missing wrong-binding wrong-kind \
        moved-receipt duplicate-receipt wrong-source-binding; do
    for backend in c llvm; do
        mutated="$CONDITIONAL_MUTATIONS/$mutation.mir.json"
        output_rel="$WORK_REL/conditional-$mutation.$backend"
        marker="preserved:conditional:$mutation:$backend"
        printf '%s\n' "$marker" >"$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated" -o "$output_rel") \
            >"$WORK_DIR/conditional-$mutation-$backend.out" \
            2>"$WORK_DIR/conditional-$mutation-$backend.err"; then
            fail "direct $backend consumer accepted multi-block $mutation"
        fi
        [[ "$(cat "$ROOT_DIR/$output_rel")" == "$marker" ]] ||
            fail "multi-block refusal replaced the prior $backend artifact"
        grep -Eq '(CODEGEN ERROR|MIR-LOWER ERROR):' \
            "$WORK_DIR/conditional-$mutation-$backend.out" \
            "$WORK_DIR/conditional-$mutation-$backend.err" ||
            fail "multi-block $backend refusal lost its diagnostic"
    done
done

for backend in c llvm; do
    for lane in public native; do
        output_rel="$WORK_REL/conditional-valid-$lane-$backend.exe"
        command=("$PGY")
        [[ "$lane" == native ]] && command+=(--native-pipeline)
        command+=("$CONDITIONAL_VALID" "--backend=$backend" --run \
            -o "$output_rel")
        (cd "$ROOT_DIR" && "${command[@]}") \
            >"$WORK_DIR/conditional-valid-$lane-$backend.out" \
            2>"$WORK_DIR/conditional-valid-$lane-$backend.err" ||
            fail "$lane $backend rejected multi-block empty-to-owned transition"
    done
done

(cd "$ROOT_DIR" && "$PGY" "$EMPTY_VALID" --backend=c --emit-c \
    -o "$WORK_REL/empty-valid.c") >"$WORK_DIR/empty-valid-c.out" \
    2>"$WORK_DIR/empty-valid-c.err" ||
    fail "installed self-host failed to emit C for exact transition"
[[ "$(grep -Ec '^[[:space:]]+pgy_as_drop_owned\(&[[:alnum:]_]+\);$' \
    "$WORK_DIR/empty-valid.c")" == 1 ]] ||
    fail "public C did not emit exactly one explicit owned drop"
! grep -Eq '^[[:space:]]+pgy_as_drop_storage\(&[[:alnum:]_]+\);$' \
    "$WORK_DIR/empty-valid.c" ||
    fail "explicit owned drop retained automatic storage cleanup"

VALID="tests/concept_semantics/hashmap/map_keys_owned_drop_valid.pgy"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$VALID" \
    -o "$WORK_REL/valid.mir.json") >"$WORK_DIR/valid-self.out" \
    2>"$WORK_DIR/valid-self.err" || fail "installed self-host rejected valid owned drop"
[[ -s "$WORK_DIR/valid.mir.json" ]] || fail "valid owned drop emitted no MIR"

for backend in c llvm; do
    (cd "$ROOT_DIR" && "$PGY" "$VALID" "--backend=$backend" --run \
        -o "$WORK_REL/valid-$backend.exe") \
        >"$WORK_DIR/valid-$backend.out" 2>"$WORK_DIR/valid-$backend.err" ||
        fail "public $backend path rejected valid owned drop"
done

# Explicit Clone and aggregate member moves are source-level ownership
# transitions, not backend guesses. Exercise the installed self-host producer,
# public route, and native oracle on both targets.
MOVE_CLONE_CASES=(
    array_clone_independence
    string_array_clone_independence
    collection_field_move_valid
    collection_field_restore_valid
    borrowed_string_array_shallow_copy
    unknown_string_array_alias_without_drop
    map_keys_shallow_copy
)
for name in "${MOVE_CLONE_CASES[@]}"; do
    source="tests/concept_semantics/hashmap/$name.pgy"
    mir_rel="$WORK_REL/$name.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$source" -o "$mir_rel") >"$WORK_DIR/$name-self.out" \
        2>"$WORK_DIR/$name-self.err" ||
        fail "installed self-host rejected $name"
    [[ -s "$ROOT_DIR/$mir_rel" ]] ||
        fail "installed self-host emitted no MIR for $name"
    if [[ "$name" == string_array_clone_independence ]]; then
        grep -Fq '"origin":"clone"' "$ROOT_DIR/$mir_rel" ||
            fail "Clone ownership origin was not carried into MIR"
    elif [[ "$name" == collection_field_move_valid ||
            "$name" == collection_field_restore_valid ]]; then
        grep -Fq '"origin":"member-move"' "$ROOT_DIR/$mir_rel" ||
            fail "aggregate member-move origin was not carried into MIR"
    elif [[ "$name" == borrowed_string_array_shallow_copy ||
            "$name" == unknown_string_array_alias_without_drop ||
            "$name" == map_keys_shallow_copy ]]; then
        grep -Fq '"origin":"binding"' "$ROOT_DIR/$mir_rel" ||
            fail "local declaration move origin was not carried into MIR"
        grep -Fq '"disposition":"retired"' "$ROOT_DIR/$mir_rel" ||
            fail "local declaration move did not retire its source"
    fi

    expected="$WORK_DIR/$name.expected"
    if [[ "$name" == array_clone_independence ]]; then
        printf '1\n99\n' >"$expected"
    elif [[ "$name" == string_array_clone_independence ]]; then
        printf 'alpha\n' >"$expected"
    elif [[ "$name" == borrowed_string_array_shallow_copy ||
            "$name" == unknown_string_array_alias_without_drop ||
            "$name" == map_keys_shallow_copy ]]; then
        printf '1\n' >"$expected"
    else
        printf 'moved\n' >"$expected"
    fi
    for backend in c llvm; do
        for lane in public native; do
            output_rel="$WORK_REL/$name-$lane-$backend.exe"
            command=("$PGY")
            [[ "$lane" == native ]] && command+=(--native-pipeline)
            command+=("$source" "--backend=$backend" --run -o "$output_rel")
            (cd "$ROOT_DIR" && "${command[@]}") \
                >"$WORK_DIR/$name-$lane-$backend.out" \
                2>"$WORK_DIR/$name-$lane-$backend.err" ||
                fail "$lane $backend path rejected $name"
            tr -d '\r' <"$WORK_DIR/$name-$lane-$backend.out" |
                sed '/^pgy:/d' >"$WORK_DIR/$name-$lane-$backend.run"
            cmp -s "$expected" "$WORK_DIR/$name-$lane-$backend.run" ||
                fail "$lane $backend runtime output drifted for $name"
        done
    done
done

# A MIR binding move is owned by the direct plan. The source local is retired,
# and the destination alone releases borrowed, unknown, or MapKeys storage.
(cd "$ROOT_DIR" && "$PGY" "$DIRECT_MOVE_PROBE" --native-pipeline \
    --backend=c -o "$WORK_REL/binding-move-direct-backend-probe.exe") \
    >"$WORK_DIR/binding-move-direct-backend-probe.compile" 2>&1 || {
    tail -c 65536 "$WORK_DIR/binding-move-direct-backend-probe.compile" >&2
    fail "native pipeline could not build the direct binding-move backend probe"
}
MAP_KEYS_BROADENED_REL="$WORK_REL/map-keys-retirement-broadened.mir.json"
python - "$ROOT_DIR/$WORK_REL/valid.mir.json" \
        "$ROOT_DIR/$MAP_KEYS_BROADENED_REL" <<'PY'
import json, pathlib, sys
source, output = map(pathlib.Path, sys.argv[1:])
program = json.loads(source.read_text(encoding="utf-8"))
facts = program["routines"][0]["collection_ownership_facts"]
assert len(facts) == 1
facts[0]["origin"] = "clone"
facts[0]["element_ownership"] = "owned-elements"
output.write_text(json.dumps(program, separators=(",", ":")), encoding="utf-8")
PY
for backend in c llvm; do
    output_rel="$WORK_REL/map-keys-retirement-broadened.$backend"
    marker="preserved:map-keys-retirement-broadened:$backend"
    printf '%s\n' "$marker" >"$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$WORK_DIR/binding-move-direct-backend-probe.exe" \
        "$MAP_KEYS_BROADENED_REL" "$backend" "$output_rel") \
        >"$WORK_DIR/map-keys-retirement-broadened-$backend.out" \
        2>"$WORK_DIR/map-keys-retirement-broadened-$backend.err"; then
        fail "direct $backend broadly admitted a non-MapKeys retirement drop"
    fi
    [[ "$(cat "$ROOT_DIR/$output_rel")" == "$marker" ]] ||
        fail "direct $backend broad-retirement refusal replaced its artifact"
    grep -Fq 'stage=collection_ownership_transition' \
        "$WORK_DIR/map-keys-retirement-broadened-$backend.out" \
        "$WORK_DIR/map-keys-retirement-broadened-$backend.err" ||
        fail "direct $backend broad-retirement refusal lost its stage"
done
RUNTIME_FEATURE_FLAGS=()
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) ;;
    Darwin) RUNTIME_FEATURE_FLAGS+=(-D_DARWIN_C_SOURCE -D_XOPEN_SOURCE=700) ;;
    *) RUNTIME_FEATURE_FLAGS+=(-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
        -D_DEFAULT_SOURCE) ;;
esac
"$CLANG" -std=c11 -DPGY_LLVM_ENABLED "${RUNTIME_FEATURE_FLAGS[@]}" \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
    -o "$WORK_DIR/collection-runtime.o" || fail "collection runtime object did not compile"
for name in borrowed_string_array_shallow_copy \
        unknown_string_array_alias_without_drop string_array_clone_independence; do
    mir_rel="$WORK_REL/$name.mir.json"
    expected=1
    [[ "$name" == string_array_clone_independence ]] && expected=alpha
    for backend in c llvm; do
        output_rel="$WORK_REL/$name-direct.$backend"
        (cd "$ROOT_DIR" && "$WORK_DIR/binding-move-direct-backend-probe.exe" \
            "$mir_rel" "$backend" "$output_rel") \
            >"$WORK_DIR/$name-direct-$backend.out" \
            2>"$WORK_DIR/$name-direct-$backend.err" ||
            fail "direct $backend owner rejected the admitted lifetime for $name"
        artifact="$ROOT_DIR/$output_rel"
        if [[ "$name" == string_array_clone_independence ]]; then
            for local_row in 0 1; do
                drop="pgy_as_drop_owned(&pgy_local_$local_row);"
                [[ "$backend" == llvm ]] &&
                    drop="call void @pgy_as_drop_owned(ptr %pgy.local.$local_row)"
                [[ "$(grep -Fc "$drop" "$artifact")" == 1 ]] ||
                    fail "direct $backend did not release the genuine Clone/source once"
            done
        else
            destination="pgy_as_drop_storage(&pgy_local_1);"
            source_drop="pgy_as_drop_storage(&pgy_local_0);"
            if [[ "$backend" == llvm ]]; then
                destination='call void @pgy_as_drop_storage(ptr %pgy.local.1)'
                source_drop='call void @pgy_as_drop_storage(ptr %pgy.local.0)'
            fi
            [[ "$(grep -Fc "$destination" "$artifact")" == 1 ]] ||
                fail "direct $backend did not release the moved destination once for $name"
            ! grep -Fq "$source_drop" "$artifact" ||
                fail "direct $backend released the retired move source for $name"
            ! grep -Eq '(pgy_as_drop_owned\(&pgy_local_1|call void @pgy_as_drop_owned\(ptr %pgy.local.1)' \
                "$artifact" || fail "direct $backend deep-dropped unproved elements for $name"
            ! grep -Fq 'pgy_as values' "$artifact" ||
                fail "direct $backend reconstructed AST storage for $name"
        fi
        executable="$WORK_DIR/$name-direct-$backend.exe"
        if [[ "$backend" == c ]]; then
            "$CC" -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
                "$artifact" -o "$executable" ||
                fail "generated direct C did not compile for $name"
        else
            "$CLANG" -x ir "$artifact" -x none "$WORK_DIR/collection-runtime.o" \
                -pthread -lm -o "$executable" ||
                fail "generated direct LLVM did not link for $name"
        fi
        "$executable" >"$WORK_DIR/$name-direct-$backend.run" ||
            fail "generated direct $backend did not run for $name"
        [[ "$(tr -d '\r\n' <"$WORK_DIR/$name-direct-$backend.run")" == "$expected" ]] ||
            fail "generated direct $backend output drifted for $name"
    done
done

MAP_KEYS_MOVE_C_REL="$WORK_REL/map_keys_shallow_copy-direct.c"
MAP_KEYS_MOVE_LLVM_REL="$WORK_REL/map_keys_shallow_copy-direct.ll"
for backend in c llvm; do
    output_rel="$MAP_KEYS_MOVE_C_REL"
    [[ "$backend" == llvm ]] && output_rel="$MAP_KEYS_MOVE_LLVM_REL"
    (cd "$ROOT_DIR" && "$WORK_DIR/binding-move-direct-backend-probe.exe" \
        "$WORK_REL/map_keys_shallow_copy.mir.json" "$backend" "$output_rel") \
        >"$WORK_DIR/map-keys-direct-$backend.out" \
        2>"$WORK_DIR/map-keys-direct-$backend.err" || {
        cat "$WORK_DIR/map-keys-direct-$backend.out" \
            "$WORK_DIR/map-keys-direct-$backend.err" >&2
        fail "direct binding-move $backend owner rejected MapKeys"
    }
done
MAP_KEYS_MOVE_C="$ROOT_DIR/$MAP_KEYS_MOVE_C_REL"
MAP_KEYS_MOVE_LLVM="$ROOT_DIR/$MAP_KEYS_MOVE_LLVM_REL"
[[ "$(grep -Fc 'pgy_self_map_keys_HashMap_String_Int(&(pgy_local_0))' \
    "$MAP_KEYS_MOVE_C")" == 1 ]] || fail "direct C lost the MapKeys bridge"
[[ "$(grep -Fc 'pgy_as_drop_owned(&pgy_local_2);' \
    "$MAP_KEYS_MOVE_C")" == 1 ]] || fail "direct C did not drop the moved snapshot once"
! grep -Fq 'pgy_as_drop_owned(&pgy_local_1);' "$MAP_KEYS_MOVE_C" ||
    fail "direct C dropped the retired MapKeys source"
[[ "$(grep -Fc 'pgy_map_drop_int(&pgy_local_0);' \
    "$MAP_KEYS_MOVE_C")" == 1 ]] || fail "direct C did not drop the map once"
! grep -Fq 'pgy_as values' "$MAP_KEYS_MOVE_C" ||
    fail "direct C fell back to reconstructed AST storage for MapKeys"
"$CC" -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    "$MAP_KEYS_MOVE_C" -o "$WORK_DIR/map-keys-direct-c.exe" ||
    fail "generated MapKeys C did not compile"
"$WORK_DIR/map-keys-direct-c.exe" >"$WORK_DIR/map-keys-direct-c.run" ||
    fail "generated MapKeys C did not run"
[[ "$(tr -d '\r\n' <"$WORK_DIR/map-keys-direct-c.run")" == 1 ]] ||
    fail "generated MapKeys C output drifted"

[[ "$(grep -Fc 'declare void @pgy_map_drop_raw_export(ptr)' \
    "$MAP_KEYS_MOVE_LLVM")" == 1 ]] || fail "direct LLVM lost the map drop ABI"
[[ "$(grep -Fc 'call void @pgy_map_keys_raw_export(ptr %pgy.local.0' \
    "$MAP_KEYS_MOVE_LLVM")" == 1 ]] || fail "direct LLVM lost the MapKeys bridge"
[[ "$(grep -Fc 'call void @pgy_as_drop_owned(ptr %pgy.local.2)' \
    "$MAP_KEYS_MOVE_LLVM")" == 1 ]] || fail "direct LLVM did not drop the moved snapshot once"
! grep -Fq 'call void @pgy_as_drop_owned(ptr %pgy.local.1)' \
    "$MAP_KEYS_MOVE_LLVM" || fail "direct LLVM dropped the retired MapKeys source"
[[ "$(grep -Fc 'call void @pgy_map_drop_raw_export(ptr %pgy.local.0)' \
    "$MAP_KEYS_MOVE_LLVM")" == 1 ]] || fail "direct LLVM did not drop the map once"
"$CLANG" -x ir "$MAP_KEYS_MOVE_LLVM" -x none \
    "$WORK_DIR/collection-runtime.o" -pthread -lm \
    -o "$WORK_DIR/map-keys-direct-llvm.exe" || fail "generated MapKeys LLVM did not link"
"$WORK_DIR/map-keys-direct-llvm.exe" >"$WORK_DIR/map-keys-direct-llvm.run" ||
    fail "generated MapKeys LLVM did not run"
[[ "$(tr -d '\r\n' <"$WORK_DIR/map-keys-direct-llvm.run")" == 1 ]] ||
    fail "generated MapKeys LLVM output drifted"

HASHMAP_ALIAS_SOURCE="tests/self_hosted/parity/fixture/direct_mir_hashmap_alias_lifetime_unproved.pgy"
HASHMAP_ALIAS_MIR_REL="$WORK_REL/hashmap-alias-lifetime-unproved.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$HASHMAP_ALIAS_SOURCE" -o "$HASHMAP_ALIAS_MIR_REL") \
    >"$WORK_DIR/hashmap-alias-self.out" \
    2>"$WORK_DIR/hashmap-alias-self.err" ||
    fail "installed self-host did not produce the HashMap alias falsifier"
for backend in c llvm; do
    output_rel="$WORK_REL/hashmap-alias-lifetime-unproved.$backend"
    marker="preserved:hashmap-alias:$backend"
    printf '%s\n' "$marker" >"$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$WORK_DIR/binding-move-direct-backend-probe.exe" \
        "$HASHMAP_ALIAS_MIR_REL" "$backend" "$output_rel") \
        >"$WORK_DIR/hashmap-alias-$backend.out" \
        2>"$WORK_DIR/hashmap-alias-$backend.err"; then
        fail "direct $backend silently admitted unproved HashMap alias lifetime"
    fi
    [[ "$(cat "$ROOT_DIR/$output_rel")" == "$marker" ]] ||
        fail "HashMap alias refusal replaced the prior $backend artifact"
    grep -Fq 'program_readiness=27' \
        "$WORK_DIR/hashmap-alias-$backend.out" \
        "$WORK_DIR/hashmap-alias-$backend.err" ||
        fail "direct $backend lost the HashMap lifetime refusal identity"
done

# The first fixture executes only the early return. The second actually reaches
# the closing brace; static cleanup counts alone do not cover that exit.
for row in hashmap-early-return:2 hashmap-normal-exit:1; do
    name="${row%%:*}"
    source="tests/self_hosted/parity/fixture/direct_mir_hashmap_early_return_cleanup.pgy"
    [[ "$name" == hashmap-normal-exit ]] &&
        source="$FIELD_FIXTURE_DIR/hashmap_normal_exit_positive.pgy"
    mir_rel="$WORK_REL/$name.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$source" -o "$mir_rel") >"$WORK_DIR/$name-self.out" \
        2>"$WORK_DIR/$name-self.err" ||
        fail "self-host did not produce the $name fixture"
    for backend in c llvm; do
        output_rel="$WORK_REL/$name.$backend"
        (cd "$ROOT_DIR" && "$WORK_DIR/binding-move-direct-backend-probe.exe" \
            "$mir_rel" "$backend" "$output_rel") \
            >"$WORK_DIR/$name-$backend.out" \
            2>"$WORK_DIR/$name-$backend.err" ||
            fail "direct $backend rejected bounded $name cleanup"
        drop='pgy_map_drop_int(&pgy_local_0);'
        [[ "$backend" == llvm ]] &&
            drop='call void @pgy_map_drop_raw_export(ptr %pgy.local.0)'
        [[ "$(grep -Fc "$drop" "$ROOT_DIR/$output_rel")" == "${row#*:}" ]] ||
            fail "direct $backend emitted the wrong cleanup count for $name"
        executable="$WORK_DIR/$name-$backend.exe"
        if [[ "$backend" == c ]]; then
            "$CC" -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
                "$ROOT_DIR/$output_rel" -o "$executable" ||
                fail "generated $name C did not compile"
        else
            "$CLANG" -x ir "$ROOT_DIR/$output_rel" -x none \
                "$WORK_DIR/collection-runtime.o" -pthread -lm -o "$executable" ||
                fail "generated $name LLVM did not link"
        fi
        "$executable" >"$WORK_DIR/$name-$backend.run" ||
            fail "generated $name $backend did not run"
        [[ "$(tr -d '\r\n' <"$WORK_DIR/$name-$backend.run")" == 1 ]] ||
            fail "generated $name $backend output drifted"
    done
done

MOVE_NEGATIVE_CASES=(
    collection_field_use_after_move
    collection_field_restored_local_use
    borrowed_string_array_move_use_after_move
)
for name in "${MOVE_NEGATIVE_CASES[@]}"; do
    source="tests/concept_semantics/hashmap/$name.pgy"
    self_rel="$WORK_REL/$name-self.mir.json"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$source" -o "$self_rel") >"$WORK_DIR/$name-self.out" \
        2>"$WORK_DIR/$name-self.err"; then
        fail "installed self-host accepted $name"
    fi
    [[ ! -e "$ROOT_DIR/$self_rel" ]] ||
        fail "installed self-host published MIR for rejected $name"
    grep -Fq 'move_from_released' \
        "$WORK_DIR/$name-self.out" "$WORK_DIR/$name-self.err" ||
        fail "installed self-host lost move diagnostic for $name"

    for backend in c llvm; do
        for lane in public native; do
            output_rel="$WORK_REL/$name-$lane-$backend.exe"
            command=("$PGY")
            [[ "$lane" == native ]] && command+=(--native-pipeline)
            command+=("$source" "--backend=$backend" -o "$output_rel")
            if (cd "$ROOT_DIR" && "${command[@]}") \
                >"$WORK_DIR/$name-$lane-$backend.out" \
                2>"$WORK_DIR/$name-$lane-$backend.err"; then
                fail "$lane $backend path accepted $name"
            fi
            [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
                fail "$lane $backend path published rejected $name"
            grep -Eq '(move_from_released|was moved|moved or released)' \
                "$WORK_DIR/$name-$lane-$backend.out" \
                "$WORK_DIR/$name-$lane-$backend.err" ||
                fail "$lane $backend path lost move diagnostic for $name"
        done
    done
done

echo "[$LABEL] native fact -> installed self-host -> public C/LLVM ownership parity PASS"
