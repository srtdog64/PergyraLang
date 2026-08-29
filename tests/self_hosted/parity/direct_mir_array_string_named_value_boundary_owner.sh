#!/usr/bin/env bash
# Native and self-host source admission reject unnamed borrow-tracked values
# before MIR/C publication while named ArrayString and copy-only String remain.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-array-string-named-value-boundary"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_array_string_named_value_boundary"
WORK_DIR="$ROOT_DIR/$WORK_REL"
NEGATIVE_CALL="tests/self_hosted/fixtures/direct_mir_owned_array_string_unnamed_boundary_negative.pgy"
NEGATIVE_LITERAL="tests/self_hosted/fixtures/direct_mir_owned_array_string_literal_boundary_negative.pgy"
NAMED="tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter.pgy"
COPY_ONLY="tests/self_hosted/fixtures/direct_mir_owned_string_parameter.pgy"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_named_value_boundary_verdict_owner.pgy"
BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -f "$OWNER" && -f "$BUNDLE" ]] || fail "semantic owner is missing"

grep -Fq 'SemanticAstNamedValueBoundaryVerdictFromResolvedFacts(' "$OWNER" ||
    fail "named value-boundary owner is missing"
grep -Fq 'SemanticExpressionGraphPlaceKind(' "$OWNER" ||
    fail "owner does not consume the carried expression-place fact"
grep -Fq 'SemanticAstFunctionParamModeAt(' "$OWNER" ||
    fail "owner does not consume the carried parameter mode"
grep -Fq 'SemanticAstNamedValueBoundaryVerdictFromResolvedFacts(' "$BUNDLE" ||
    fail "body semantic admission does not consume the verdict"
! grep -Fq 'direct MIR scalar program extension is invalid' "$OWNER" ||
    fail "semantic owner depends on the backend rejection"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

for negative_case in call-result literal; do
    negative_source="$NEGATIVE_CALL"
    if [[ "$negative_case" == literal ]]; then
        negative_source="$NEGATIVE_LITERAL"
    fi
    native_c_rel="$WORK_REL/native-$negative_case.c"
    if (cd "$ROOT_DIR" && "$PGY" "$negative_source" \
        --native-pipeline --emit-c -o "$native_c_rel") \
        >"$WORK_DIR/native-$negative_case.out" \
        2>"$WORK_DIR/native-$negative_case.err"; then
        fail "native semantic boundary accepted the unnamed $negative_case ArrayString value"
    fi
    [[ ! -e "$ROOT_DIR/$native_c_rel" ]] ||
        fail "native boundary published C for rejected $negative_case source"
    grep -Fq 'must use a named variable' \
        "$WORK_DIR/native-$negative_case.err" ||
        fail "native $negative_case rejection lost the named-variable diagnostic"

    self_mir_rel="$WORK_REL/self-$negative_case.mir.json"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$negative_source" -o "$self_mir_rel") \
        >"$WORK_DIR/self-$negative_case.out" \
        2>"$WORK_DIR/self-$negative_case.err"; then
        fail "self-host semantic boundary accepted the unnamed $negative_case ArrayString value"
    fi
    [[ ! -e "$ROOT_DIR/$self_mir_rel" ]] ||
        fail "self-host boundary published MIR for rejected $negative_case source"
    grep -Fq 'named_value_boundary_argument_required' \
        "$WORK_DIR/self-$negative_case.out" \
        "$WORK_DIR/self-$negative_case.err" ||
        fail "self-host $negative_case rejection lost its stable diagnostic identity"
    grep -Fq 'bind the value to a local variable' \
        "$WORK_DIR/self-$negative_case.out" \
        "$WORK_DIR/self-$negative_case.err" ||
        fail "self-host $negative_case rejection lost the repair guidance"
done

for positive in named copy-only; do
    source="$NAMED"
    if [[ "$positive" == copy-only ]]; then source="$COPY_ONLY"; fi
    output_rel="$WORK_REL/$positive.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" \
        -o "$output_rel") >"$WORK_DIR/$positive.out" \
        2>"$WORK_DIR/$positive.err" || {
            cat "$WORK_DIR/$positive.out" "$WORK_DIR/$positive.err" >&2
            fail "$positive control was rejected"
        }
    [[ -s "$ROOT_DIR/$output_rel" ]] ||
        fail "$positive control emitted no MIR"
done

echo "[$LABEL] native/self rejection + named/copy-only controls: PASS"
