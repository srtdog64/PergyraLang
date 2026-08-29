#!/usr/bin/env bash
# Native and self-host source admission reject unnamed Array<T> values before
# MIR/C publication while named/default Array and copy-only String remain.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-array-named-value-boundary"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_array_named_value_boundary"
WORK_DIR="$ROOT_DIR/$WORK_REL"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_named_value_boundary_verdict_owner.pgy"
SHAPE="$ROOT_DIR/src/self_hosted/semantic/array_type_shape_owner.pgy"
BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"

NEGATIVE_CASES=(
    "string-call:tests/self_hosted/fixtures/direct_mir_owned_array_string_unnamed_boundary_negative.pgy"
    "string-literal:tests/self_hosted/fixtures/direct_mir_owned_array_string_literal_boundary_negative.pgy"
    "int-call:tests/self_hosted/fixtures/direct_mir_owned_array_int_unnamed_boundary_negative.pgy"
    "int-literal:tests/self_hosted/fixtures/direct_mir_owned_array_int_literal_boundary_negative.pgy"
)
POSITIVE_CASES=(
    "named-string:tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter.pgy"
    "named-int:tests/self_hosted/fixtures/direct_mir_owned_array_int_parameter.pgy"
    "default-array:tests/self_hosted/fixtures/direct_mir_array_int_value_parameter.pgy"
    "copy-only:tests/self_hosted/fixtures/direct_mir_owned_string_parameter.pgy"
)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -f "$OWNER" && -f "$SHAPE" && -f "$BUNDLE" ]] ||
    fail "semantic owner set is incomplete"

grep -Fq 'SemanticAstNamedValueBoundaryVerdictFromResolvedFacts(' "$OWNER" ||
    fail "named value-boundary owner is missing"
grep -Fq 'SemanticArrayElementType(type_name)' "$OWNER" ||
    fail "named boundary bypasses the canonical Array shape owner"
! grep -Fq 'type_name == "Array<' "$OWNER" ||
    fail "named boundary contains an element-type allowlist"
grep -Fq '!IsSome(SemanticArrayElementType("Array<Int"))' "$SHAPE" ||
    fail "Array shape contract omits malformed-spelling rejection"
grep -Fq 'SemanticExpressionGraphPlaceKind(' "$OWNER" ||
    fail "owner does not consume the carried expression-place fact"
grep -Fq 'SemanticAstFunctionParamModeAt(' "$OWNER" ||
    fail "owner does not consume the carried parameter mode"
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
    grep -Fq 'must use a named variable' \
        "$WORK_DIR/native-$negative_case.err" ||
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
        "$WORK_DIR/self-$negative_case.out" \
        "$WORK_DIR/self-$negative_case.err" ||
        fail "self-host $negative_case lost its stable diagnostic identity"
    grep -Fq 'bind the value to a local variable' \
        "$WORK_DIR/self-$negative_case.out" \
        "$WORK_DIR/self-$negative_case.err" ||
        fail "self-host $negative_case lost the repair guidance"
done

for entry in "${POSITIVE_CASES[@]}"; do
    positive_case="${entry%%:*}"
    positive_source="${entry#*:}"
    output_rel="$WORK_REL/$positive_case.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$positive_source" -o "$output_rel") \
        >"$WORK_DIR/$positive_case.out" \
        2>"$WORK_DIR/$positive_case.err" || {
            cat "$WORK_DIR/$positive_case.out" \
                "$WORK_DIR/$positive_case.err" >&2
            fail "$positive_case control was rejected"
        }
    [[ -s "$ROOT_DIR/$output_rel" ]] ||
        fail "$positive_case control emitted no MIR"
done

echo "[$LABEL] native/self Array family rejection + named/default/copy-only controls: PASS"
