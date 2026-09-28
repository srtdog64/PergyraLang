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
! grep -Fq 'Slot<' "$OWNER" "$IDENTITY_OWNER" ||
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

echo "[$LABEL] native fact -> installed self-host -> public C/LLVM ownership parity PASS"
