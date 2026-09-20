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
BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
WORK_REL=".tmp/self_hosted/collection_ownership_semantic_owner"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

grep -Fq 'SemanticAstCollectionOwnershipVerdictFromResolvedFacts(' "$OWNER" ||
    fail "semantic collection ownership owner is missing"
grep -Fq 'SemanticExpressionGraphCallTargetSyntaxId(' "$OWNER" ||
    fail "owner does not distinguish builtins from same-name declarations"
grep -Fq 'SemanticExpressionGraphRuntimeCallAbiId(' "$OWNER" ||
    fail "owner does not consume carried runtime call identity"
grep -Fq 'SemanticAstScopedLocalBindingIdentityForGraphLeaf(' "$OWNER" ||
    fail "owner joins collection state by spelling instead of binding identity"
grep -Fq 'TypedAstKindArrayPushStmtTag()' "$OWNER" ||
    fail "owner ignores parser-owned collection mutation statements"
grep -Fq 'SemanticAstCollectionOwnershipVerdictFromResolvedFacts(' "$BUNDLE" ||
    fail "body admission does not consume collection ownership verdict"
! grep -Fq 'Slot<' "$OWNER" || fail "ordinary collection ownership imported Slot semantics"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

NEGATIVE_CASES=(
    borrowed_string_array_deep_drop
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
    done
done

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
