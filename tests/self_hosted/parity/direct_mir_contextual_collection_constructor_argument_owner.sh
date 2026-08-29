#!/usr/bin/env bash
# Matching direct List/Queue/Set constructors consume the carried parameter
# type before call-argument admission; wrong family and arity remain closed.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-contextual-collection-constructor-argument"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_contextual_collection_constructor_argument"
WORK_DIR="$ROOT_DIR/$WORK_REL"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_contextual_builtin_type_owner.pgy"
CALL_CHECK="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy"
EXPRESSION_VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
INITIALIZER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"

MATCHING_CASES=(
    "list:tests/self_hosted/fixtures/direct_mir_owned_list_int_fresh_constructor.pgy"
    "queue:tests/self_hosted/fixtures/direct_mir_owned_queue_int_fresh_constructor.pgy"
    "set:tests/self_hosted/fixtures/direct_mir_set_int_fresh_constructor.pgy"
)
WRONG_FAMILY="tests/self_hosted/fixtures/direct_mir_list_int_wrong_queue_constructor_negative.pgy"
BAD_ARITY="tests/self_hosted/fixtures/direct_mir_list_int_fresh_constructor_arity_negative.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -f "$OWNER" && -f "$CALL_CHECK" && -f "$EXPRESSION_VERDICT" &&
    -f "$INITIALIZER" ]] ||
    fail "contextual owner set is incomplete"

grep -Fq 'func SemanticContextualBuiltinExpectedTypeFromGraph(' "$OWNER" ||
    fail "contextual expected-type owner is missing"
grep -Fq 'SemanticContextualBuiltinReturnTypeOpt(' "$OWNER" ||
    fail "contextual type bypasses the canonical builtin signature owner"
grep -Fq 'SemanticContextualBuiltinExpectedTypeFromGraph(' "$CALL_CHECK" ||
    fail "call-argument checker does not consume contextual type facts"
grep -Fq 'let expected_type: Option<String> = Some(expected);' "$CALL_CHECK" ||
    fail "call-argument checker does not carry its expected type explicitly"
grep -Fq 'SemanticExpressionGraphContextualCallArgumentsOwned(' \
    "$EXPRESSION_VERDICT" ||
    fail "expression verdict does not select the contextual graph consumer"
grep -Fq 'graph_call_value_owned' "$EXPRESSION_VERDICT" ||
    fail "contextual call arguments still fall through to source-text checks"
grep -Fq 'SemanticContextualBuiltinExpectedTypeFromGraph(' "$INITIALIZER" ||
    fail "initializer consumer drifted from the shared contextual owner"
! grep -Eq 'actual[[:space:]]*=[[:space:]]*expected' "$CALL_CHECK" ||
    fail "call-argument checker contains a generic expected-type coercion"
! grep -Eq '(List|Queue|Set)<Unknown>' "$CALL_CHECK" ||
    fail "call-argument checker owns a collection-name allowlist"

[[ "$WORK_DIR" == "$ROOT_DIR/.tmp/self_hosted/direct_mir_contextual_collection_constructor_argument" ]] ||
    fail "refusing to clean an unexpected work directory"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

for entry in "${MATCHING_CASES[@]}"; do
    matching_case="${entry%%:*}"
    matching_source="${entry#*:}"
    native_c_rel="$WORK_REL/native-$matching_case.c"
    (cd "$ROOT_DIR" && "$PGY" "$matching_source" \
        --native-pipeline --emit-c -o "$native_c_rel") \
        >"$WORK_DIR/native-$matching_case.out" \
        2>"$WORK_DIR/native-$matching_case.err" ||
        fail "native rejected matching fresh $matching_case constructor"
    [[ -s "$ROOT_DIR/$native_c_rel" ]] ||
        fail "native emitted no C for matching fresh $matching_case constructor"

    self_mir_rel="$WORK_REL/self-$matching_case.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$matching_source" -o "$self_mir_rel") \
        >"$WORK_DIR/self-$matching_case.out" \
        2>"$WORK_DIR/self-$matching_case.err" ||
        fail "self-host rejected matching fresh $matching_case constructor"
    [[ -s "$ROOT_DIR/$self_mir_rel" ]] ||
        fail "self-host emitted no MIR for matching fresh $matching_case constructor"
done

wrong_mir_rel="$WORK_REL/self-wrong-family.mir.json"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$WRONG_FAMILY" -o "$wrong_mir_rel") \
    >"$WORK_DIR/self-wrong-family.out" \
    2>"$WORK_DIR/self-wrong-family.err"; then
    fail "self-host accepted a wrong constructor family"
fi
[[ ! -e "$ROOT_DIR/$wrong_mir_rel" ]] ||
    fail "self-host published MIR for a wrong constructor family"
grep -Fq 'Code: call_arg_type_mismatch' \
    "$WORK_DIR/self-wrong-family.out" "$WORK_DIR/self-wrong-family.err" ||
    fail "wrong-family diagnostic identity drifted"
grep -Fq -- '- actual: Queue<Unknown>' \
    "$WORK_DIR/self-wrong-family.out" "$WORK_DIR/self-wrong-family.err" ||
    fail "wrong-family actual type was overwritten by context"

native_arity_c_rel="$WORK_REL/native-bad-arity.c"
if (cd "$ROOT_DIR" && "$PGY" "$BAD_ARITY" \
    --native-pipeline --emit-c -o "$native_arity_c_rel") \
    >"$WORK_DIR/native-bad-arity.out" \
    2>"$WORK_DIR/native-bad-arity.err"; then
    fail "native accepted nonzero ListNew arity"
fi
[[ ! -e "$ROOT_DIR/$native_arity_c_rel" ]] ||
    fail "native published C for nonzero ListNew arity"
grep -Fq "expects 0 argument(s), got 1" "$WORK_DIR/native-bad-arity.err" ||
    fail "native nonzero-arity diagnostic drifted"

self_arity_mir_rel="$WORK_REL/self-bad-arity.mir.json"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$BAD_ARITY" -o "$self_arity_mir_rel") \
    >"$WORK_DIR/self-bad-arity.out" \
    2>"$WORK_DIR/self-bad-arity.err"; then
    fail "self-host accepted nonzero ListNew arity"
fi
[[ ! -e "$ROOT_DIR/$self_arity_mir_rel" ]] ||
    fail "self-host published MIR for nonzero ListNew arity"
grep -Fq 'Code:' "$WORK_DIR/self-bad-arity.out" \
    "$WORK_DIR/self-bad-arity.err" ||
    fail "self-host nonzero-arity rejection is not inspectable"

echo "[$LABEL] matching List+Queue+Set + wrong-family/arity negatives: PASS"
