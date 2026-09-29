#!/usr/bin/env bash
# Collection element ownership is decided by one semantic owner before MIR.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-collection-ownership"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy"
IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_identity_owner.pgy"
STATE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_state_owner.pgy"
MEMBER_MOVE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy"
MEMBER_TRANSITION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy"
BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
WORK_REL=".tmp/self_hosted/collection_ownership_semantic_owner"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

grep -Fq 'SemanticAstCollectionOwnershipVerdictFromResolvedFacts(' "$OWNER" ||
    fail "semantic collection ownership owner is missing"
grep -Fq 'SemanticExpressionGraphCallTargetSyntaxId(' "$IDENTITY_OWNER" ||
    fail "owner does not distinguish builtins from same-name declarations"
grep -Fq 'SemanticExpressionGraphRuntimeCallAbiId(' "$IDENTITY_OWNER" ||
    fail "owner does not consume carried runtime call identity"
grep -Fq 'SemanticAstScopedLocalBindingIdentityForGraphLeaf(' "$IDENTITY_OWNER" ||
    fail "owner joins collection state by spelling instead of binding identity"
grep -Fq 'TypedAstKindArrayPushStmtTag()' "$OWNER" ||
    fail "owner ignores parser-owned collection mutation statements"
grep -Fq 'SemanticAstCollectionOwnershipVerdictFromResolvedFacts(' "$BUNDLE" ||
    fail "body admission does not consume collection ownership verdict"
grep -Fq 'SemanticAstCollectionMemberMoveIdentityForNode(' "$MEMBER_MOVE_OWNER" ||
    fail "aggregate member move lacks one stable identity owner"
grep -Fq 'SemanticAstCollectionFirstInvalidMemberMoveUse(' \
    "$MEMBER_TRANSITION_OWNER" ||
    fail "aggregate member move lacks ordered transition validation"
grep -Fq 'SemanticAstCollectionOriginClone()' "$STATE_OWNER" ||
    fail "explicit Clone origin is missing from the collection state owner"
! grep -Fq 'Slot<' "$OWNER" "$IDENTITY_OWNER" "$STATE_OWNER" \
        "$MEMBER_MOVE_OWNER" "$MEMBER_TRANSITION_OWNER" ||
    fail "ordinary collection ownership imported Slot semantics"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

NEGATIVE_CASES=(
    borrowed_string_array_deep_drop
    empty_shallow_string_push_drop
    empty_mixed_string_push_without_drop
    empty_string_array_shallow_copy
    empty_string_array_assignment_shallow_copy
    empty_owned_string_double_drop
    inout_string_array_deep_drop
    map_keys_shallow_push
    map_keys_shallow_set
    map_keys_shallow_pop
    map_keys_shallow_copy
    map_keys_double_drop
    collection_field_owned_push
    collection_field_deep_drop
)

for name in "${NEGATIVE_CASES[@]}"; do
    source="tests/concept_semantics/hashmap/$name.pgy"
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
[[ "$(grep -Fc 'pgy_as_drop_owned(&values);' "$WORK_DIR/empty-valid.c")" == 1 ]] ||
    fail "explicit owned drop was duplicated by automatic cleanup"

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
    fi

    expected="$WORK_DIR/$name.expected"
    if [[ "$name" == array_clone_independence ]]; then
        printf '1\n99\n' >"$expected"
    elif [[ "$name" == string_array_clone_independence ]]; then
        printf 'alpha\n' >"$expected"
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

MOVE_NEGATIVE_CASES=(
    collection_field_use_after_move
    collection_field_restored_local_use
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
