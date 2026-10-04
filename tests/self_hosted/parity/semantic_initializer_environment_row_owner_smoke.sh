#!/usr/bin/env bash
# Ratchets initializer environments to one owned lexical epoch per local row.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INITIALIZER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
RETIRED_CURSOR="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_environment_cursor_owner.pgy"
PROBE="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/main.pgy"
PARITY="$ROOT_DIR/tests/self_hosted/parity/initializer_projection_probe_parity.sh"
LABEL="self-host-parity:initializer-environment-row"

fail() {
    echo "[$LABEL] $*" >&2
    exit 1
}

for path in "$INITIALIZER" "$PROBE" "$PARITY"; do
    [[ -f "$path" ]] || fail "missing $path"
done
[[ ! -e "$RETIRED_CURSOR" ]] ||
    fail "cross-row initializer environment cursor returned"

initializer_body="$(sed -n \
    '/func SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsObservedWithFunctionTables(/,/^}/p' \
    "$INITIALIZER")"
[[ -n "$initializer_body" ]] || fail "initializer production body is missing"

for required in \
    'while i < SemanticAstLocalBindingCount(locals)' \
    'let names: Array<String> = [];' \
    'let types: Array<String> = [];' \
    'let modes: Array<String> = [];' \
    'SemanticAstExpressionSeedEnumValues(' \
    'SemanticAstExpressionSeedOwnerFieldsFromAdmittedConstructors(' \
    'SemanticAstExpressionSeedParameters(' \
    'SemanticAstExpressionSeedParameterModes(' \
    'SemanticAstExpressionSeedVisibleLocals(' \
    'SemanticAstExpressionSeedVisibleLocalModes(' \
    'SemanticAstExpressionSeedVisibleIterationRows(' \
    'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    'SemanticAstExpressionEnvironmentClear(names, types, modes);'; do
    grep -Fq "$required" <<<"$initializer_body" ||
        fail "initializer row environment lost $required"
done

loop_line="$(grep -n -F 'while i < SemanticAstLocalBindingCount(locals)' \
    <<<"$initializer_body" | head -n 1 | cut -d: -f1)"
names_line="$(grep -n -F 'let names: Array<String> = [];' \
    <<<"$initializer_body" | head -n 1 | cut -d: -f1)"
[[ -n "$loop_line" && -n "$names_line" && "$names_line" -gt "$loop_line" ]] ||
    fail "initializer environment is not created inside the row epoch"

for forbidden in \
    'SemanticAstInitializerEnvironmentCursor' \
    'SemanticAstInitializerEnvironmentTruncateTransient' \
    'SemanticAstExpressionEnvironmentReset(' \
    'SemanticAstExpressionEnvironmentTruncateOwned(' \
    'ArrayPop(names)' \
    'ArrayPop(types)' \
    'ArrayPop(modes)'; do
    if grep -Fq "$forbidden" <<<"$initializer_body"; then
        fail "initializer row environment regained retired path: $forbidden"
    fi
done

for fixture in \
    '--environment-outer-read-positive' \
    '--environment-nested-exit-positive' \
    '--environment-destructure-atomic-positive' \
    '--environment-self-reference' \
    '--environment-sibling-leak'; do
    grep -Fq -- "$fixture" "$PROBE" ||
        fail "executable environment fixture is missing: $fixture"
    grep -Fq -- "${fixture#--}" "$PARITY" ||
        fail "C/LLVM environment parity route is missing: $fixture"
done

echo "[$LABEL] initializer rows own and retire independent lexical environments"
