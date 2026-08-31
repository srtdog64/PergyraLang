#!/usr/bin/env bash

# Sourced by domain_runtime_zone_sync_execution_owner.sh after it has built the
# current codegen executable. Zone value-carriage policy belongs to semantic
# admission, never to a C preprocessor mode.

: "${ROOT_DIR:?zone carriage gate requires ROOT_DIR}"
: "${BUILD_DIR:?zone carriage gate requires BUILD_DIR}"
: "${CODEGEN_BIN:?zone carriage gate requires CODEGEN_BIN}"

CARRIAGE_BUILD_DIR="$BUILD_DIR/zone-value-carriage"
mkdir -p "$CARRIAGE_BUILD_DIR"

for negative_case in copy reassign; do
    if [[ "$negative_case" == copy ]]; then
        negative_source="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_copy_threadsafe_rejected.pgy"
        negative_code='zone_value_copy_requires_transfer'
    else
        negative_source="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_reassign_threadsafe_rejected.pgy"
        negative_code='zone_value_reassignment_requires_transfer'
    fi
    negative_out="$CARRIAGE_BUILD_DIR/$negative_case.out"
    negative_err="$CARRIAGE_BUILD_DIR/$negative_case.err"
    if "$CODEGEN_BIN" --source "${negative_source#"$ROOT_DIR/"}" \
        >"$negative_out" 2>"$negative_err"; then
        echo "$negative_case zone carriage escaped semantic admission" >&2
        exit 1
    fi
    grep -Fq "Code: $negative_code" "$negative_out" "$negative_err"
    if grep -Fq 'Code: unregistered_diagnostic_code' \
        "$negative_out" "$negative_err"; then
        echo "$negative_case zone carriage used an unregistered diagnostic" >&2
        exit 1
    fi
    if grep -Eq '^#include |^typedef struct|^static void .*_sync\(' \
        "$negative_out"; then
        echo "$negative_case zone carriage leaked partial C" >&2
        exit 1
    fi
done

ZONE_CARRIAGE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_zone_value_carriage_verdict_owner.pgy"
ZONE_PARAMETER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_zone_parameter_boundary_verdict_owner.pgy"
BODY_BUNDLE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
STATEMENT_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/stmt_emit.pgy"

grep -Fq 'func SemanticAstZoneValueCarriageVerdictFromAdmittedFacts(' \
    "$ZONE_CARRIAGE_OWNER"
grep -Fq 'SemanticAstZoneValueCarriageVerdictFromAdmittedFacts(' \
    "$ZONE_PARAMETER_OWNER"
grep -Fq 'SemanticAstZoneCarriageVerdictFromAdmittedFacts(' \
    "$BODY_BUNDLE_OWNER"
grep -Fq 'CodegenSemanticZoneLocalFresh(body_types, idx)' "$STATEMENT_OWNER"
if grep -Fq 'func CodegenFreshZoneLocal(' "$STATEMENT_OWNER"; then
    echo "zone fresh identity returned to statement-emission inference" >&2
    exit 1
fi
if grep -Fq 'Pergyra zone local copy requires an admitted transfer plan' \
    "$STATEMENT_OWNER" || \
   grep -Fq 'Pergyra zone reassignment requires an admitted transfer plan' \
    "$STATEMENT_OWNER"; then
    echo "zone carriage policy returned to C statement emission" >&2
    exit 1
fi

echo "[domain-runtime-zone-carriage-admission] copy/reassignment semantic admission and backend error ratchet: PASS"
