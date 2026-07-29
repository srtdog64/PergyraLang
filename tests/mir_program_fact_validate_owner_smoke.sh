#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORCHESTRATOR="$ROOT_DIR/src/compiler/mir_program_validate.c"
FACT_VALIDATOR="$ROOT_DIR/src/compiler/mir_program_fact_validate.c"
FACT_HEADER="$ROOT_DIR/src/compiler/mir_program_fact_validate.h"

fail() {
    echo "[mir-program-fact-validate-owner] $*" >&2
    exit 1
}

for owner in "$ORCHESTRATOR" "$FACT_VALIDATOR" "$FACT_HEADER"; do
    [[ -f "$owner" ]] || fail "missing owner: $owner"
done
for owner in "$ORCHESTRATOR" "$FACT_VALIDATOR"; do
    lines="$(wc -l < "$owner")"
    (( lines <= 699 )) || fail "owner exceeds 699 LOC: $owner ($lines)"
done

exported_validators=(
    mir_validate_non_cfg_fallback_state
    mir_validate_function_param_flow_summaries
    mir_validate_resource_flow_symbols
    mir_validate_loop_flow_facts
    mir_validate_program_inventory_shape
    mir_validate_non_cfg_fallback_inventory
    mir_validate_receiver_carriage_facts
)
for validator in "${exported_validators[@]}"; do
    grep -Fq -- "$validator(" "$FACT_HEADER" ||
        fail "fact header lost validator: $validator"
    grep -Eq "^$validator\(" "$FACT_VALIDATOR" ||
        fail "fact owner lost definition: $validator"
    if grep -Eq "^$validator\(" "$ORCHESTRATOR"; then
        fail "orchestrator re-owned fact validation: $validator"
    fi
    grep -Fq -- "$validator(" "$ORCHESTRATOR" ||
        fail "orchestrator stopped consuming fact validator: $validator"
done

for diagnostic in \
    "records parameters without carriage facts" \
    "incomplete function parameter flow summary identity" \
    "resource-flow rows share identity" \
    "invalid loop-flow identity or range" \
    "receiver carriage has no unique exact declaration owner" \
    "non-CFG fallback inventory is stale" \
    "source-local type fact[%zu] is incomplete"; do
    grep -Fq -- "$diagnostic" "$FACT_VALIDATOR" ||
        fail "fact diagnostic drifted: $diagnostic"
done

grep -Fq -- "bool" "$FACT_VALIDATOR" || fail "fact owner has no typed verdict"
grep -Fq -- "return false;" "$FACT_VALIDATOR" || fail "fact owner lost fail-closed verdict"
grep -Eq "^mir_validate\(" "$ORCHESTRATOR" || fail "top-level validator moved"
if grep -Eq "^mir_validate\(" "$FACT_VALIDATOR"; then
    fail "fact owner re-owned top-level validation order"
fi

line_of() {
    grep -n -F -- "$1" "$ORCHESTRATOR" | head -n 1 | cut -d: -f1
}
assert_before() {
    local before after before_line after_line
    before="$1"
    after="$2"
    before_line="$(line_of "$before")"
    after_line="$(line_of "$after")"
    [[ -n "$before_line" && -n "$after_line" ]] ||
        fail "validation order term missing: $before -> $after"
    (( before_line < after_line )) ||
        fail "validation order drifted: $before -> $after"
}
assert_before "mir_validate_program_inventory_shape(mir" "mir_validate_receiver_carriage_facts(mir"
assert_before "mir_validate_non_cfg_fallback_state(routine" "mir_validate_resource_flow_symbols(routine"
assert_before "mir_validate_resource_flow_symbols(routine" "mir_validate_function_param_flow_summaries(routine"
assert_before "mir_validate_function_param_flow_summaries(routine" "mir_validate_loop_flow_facts(routine"
assert_before "mir_validate_loop_flow_facts(routine" "mir_validate_intent_execution_plan(routine"
assert_before "mir_validate_intent_execution_plan(routine" "mir_validate_cfg_contract_state(routine"
assert_before "mir_validate_intent_execution_program(mir" "mir_validate_non_cfg_fallback_inventory(mir"

for owner in "$ORCHESTRATOR" "$FACT_VALIDATOR"; do
    for forbidden in "parser_parse" "ParseExpr" "ast_capture_inline"; do
        if grep -Fq -- "$forbidden" "$owner"; then
            fail "AST/text fallback reopened in $owner: $forbidden"
        fi
    done
done

grep -Fq -- '$(COMPILER_DIR)/mir_program_fact_validate.c' "$ROOT_DIR/Makefile" ||
    fail "fact validator source missing from Makefile"
grep -Fq -- '$(BUILD_DIR)/compiler/mir_program_fact_validate.o' "$ROOT_DIR/Makefile" ||
    fail "fact validator object missing from MIR core link"

echo "[mir-program-fact-validate-owner] typed fact validation order is closed"
