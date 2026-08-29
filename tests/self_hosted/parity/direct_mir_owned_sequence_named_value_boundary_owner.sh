#!/usr/bin/env bash
# Native and self-host source admission agree on named-value boundaries for
# owned List/Queue values while the native fresh-constructor transfer remains.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-owned-sequence-named-value-boundary"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_owned_sequence_named_value_boundary"
WORK_DIR="$ROOT_DIR/$WORK_REL"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_named_value_boundary_verdict_owner.pgy"
SHAPE="$ROOT_DIR/src/self_hosted/semantic/array_type_shape_owner.pgy"
BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"

NEGATIVE_CASES=(
    "list:tests/self_hosted/fixtures/direct_mir_owned_list_int_unnamed_boundary_negative.pgy"
    "queue:tests/self_hosted/fixtures/direct_mir_owned_queue_int_unnamed_boundary_negative.pgy"
)
POSITIVE_CASES=(
    "list:tests/self_hosted/fixtures/direct_mir_owned_list_int_parameter.pgy"
    "queue:tests/self_hosted/fixtures/direct_mir_owned_queue_int_parameter.pgy"
)
NATIVE_FRESH_CASES=(
    "list-new:tests/self_hosted/fixtures/direct_mir_owned_list_int_fresh_constructor.pgy"
    "queue-new:tests/self_hosted/fixtures/direct_mir_owned_queue_int_fresh_constructor.pgy"
)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -f "$OWNER" && -f "$SHAPE" && -f "$BUNDLE" ]] ||
    fail "semantic owner set is incomplete"

grep -Fq 'SemanticOwnedSequenceElementType(type_name)' "$OWNER" ||
    fail "named boundary bypasses the canonical owned-sequence shape owner"
grep -Fq 'SemanticAstNamedValueBoundaryFreshOwnedSequenceConstructor(' "$OWNER" ||
    fail "fresh owned-sequence constructor policy is missing"
grep -Fq 'SemanticContextualBuiltinReturnTypeOpt(' "$OWNER" ||
    fail "fresh constructor policy bypasses the canonical builtin signature owner"
! grep -Eq 'type_name[[:space:]]*==[[:space:]]*"(List|Queue)<' "$OWNER" ||
    fail "named boundary contains a List/Queue type-name allowlist"
grep -Fq 'let prefixes: Array<String> = ["List<", "Queue<"]' "$SHAPE" ||
    fail "owned-sequence shape does not own List and Queue together"
grep -Fq '!IsSome(SemanticOwnedSequenceElementType("Slice<Int>"))' "$SHAPE" ||
    fail "owned-sequence contract does not exclude Slice"
grep -Fq 'SemanticAstNamedValueBoundaryVerdictFromResolvedFacts(' "$BUNDLE" ||
    fail "body semantic admission does not consume the verdict"
! grep -Fq 'direct MIR' "$OWNER" ||
    fail "semantic owner depends on a backend rejection"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

for entry in "${NEGATIVE_CASES[@]}"; do
    negative_case="${entry%%:*}"
    negative_source="${entry#*:}"
    native_c_rel="$WORK_REL/native-$negative_case.c"
    if (cd "$ROOT_DIR" && "$PGY" "$negative_source" \
        --native-pipeline --emit-c -o "$native_c_rel") \
        >"$WORK_DIR/native-$negative_case.out" \
        2>"$WORK_DIR/native-$negative_case.err"; then
        fail "native semantic boundary accepted unnamed $negative_case"
    fi
    [[ ! -e "$ROOT_DIR/$native_c_rel" ]] ||
        fail "native boundary published C for rejected $negative_case"
    grep -Fq 'must use a named variable' "$WORK_DIR/native-$negative_case.err" ||
        fail "native $negative_case lost the named-variable diagnostic"

    self_mir_rel="$WORK_REL/self-$negative_case.mir.json"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$negative_source" -o "$self_mir_rel") \
        >"$WORK_DIR/self-$negative_case.out" \
        2>"$WORK_DIR/self-$negative_case.err"; then
        fail "self-host semantic boundary accepted unnamed $negative_case"
    fi
    [[ ! -e "$ROOT_DIR/$self_mir_rel" ]] ||
        fail "self-host boundary published MIR for rejected $negative_case"
    grep -Fq 'named_value_boundary_argument_required' \
        "$WORK_DIR/self-$negative_case.out" "$WORK_DIR/self-$negative_case.err" ||
        fail "self-host $negative_case lost its stable diagnostic identity"
done

for entry in "${POSITIVE_CASES[@]}"; do
    positive_case="${entry%%:*}"
    positive_source="${entry#*:}"
    native_c_rel="$WORK_REL/native-$positive_case.c"
    (cd "$ROOT_DIR" && "$PGY" "$positive_source" \
        --native-pipeline --emit-c -o "$native_c_rel") \
        >"$WORK_DIR/native-$positive_case.out" \
        2>"$WORK_DIR/native-$positive_case.err" ||
        fail "native rejected named/default $positive_case control"
    [[ -s "$ROOT_DIR/$native_c_rel" ]] ||
        fail "native emitted no C for named/default $positive_case control"

    self_mir_rel="$WORK_REL/self-$positive_case.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$positive_source" -o "$self_mir_rel") \
        >"$WORK_DIR/self-$positive_case.out" \
        2>"$WORK_DIR/self-$positive_case.err" ||
        fail "self-host rejected named/default $positive_case control"
    [[ -s "$ROOT_DIR/$self_mir_rel" ]] ||
        fail "self-host emitted no MIR for named/default $positive_case control"
done

for entry in "${NATIVE_FRESH_CASES[@]}"; do
    fresh_case="${entry%%:*}"
    fresh_source="${entry#*:}"
    native_c_rel="$WORK_REL/native-$fresh_case.c"
    (cd "$ROOT_DIR" && "$PGY" "$fresh_source" \
        --native-pipeline --emit-c -o "$native_c_rel") \
        >"$WORK_DIR/native-$fresh_case.out" \
        2>"$WORK_DIR/native-$fresh_case.err" ||
        fail "native rejected fresh $fresh_case ownership transfer"
    [[ -s "$ROOT_DIR/$native_c_rel" ]] ||
        fail "native emitted no C for fresh $fresh_case ownership transfer"
done

echo "[$LABEL] native/self List+Queue rejection + named/default controls + native fresh constructors: PASS"
