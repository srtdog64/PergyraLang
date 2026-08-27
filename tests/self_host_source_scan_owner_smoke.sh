#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail() {
    echo "[self-host-source-scan] $*" >&2
    exit 1
}

require_text() {
    local file="$1" text="$2"
    grep -Fq "$text" "$file" || fail "missing '$text' in $file"
}

reject_region_text() {
    local file="$1" start="$2" text="$3"
    local region
    region="$(sed -n "/$start/,/^}/p" "$file")"
    if grep -Fq "$text" <<<"$region"; then
        fail "forbidden '$text' returned in $file::$start"
    fi
}

SOURCE_OWNER="src/self_hosted/lib/source_scan_owner.pgy"
PARSER_CURSOR="src/self_hosted/parser/cursor_owner.pgy"
SEMANTIC_SCAN="src/self_hosted/semantic/text_scan_owner.pgy"
OPERATOR_FACTS="src/self_hosted/semantic/expression_operator_fact_owner.pgy"
EXPR_TYPES="src/self_hosted/semantic/expr_type_owner.pgy"
EXPR_VALIDATION="src/self_hosted/semantic/expr_validation_owner.pgy"
CALLABLE_RESOLUTION="src/self_hosted/semantic/callable_resolution_owner.pgy"
GENERIC_CALL="src/self_hosted/semantic/ast_expression_graph_generic_call_owner.pgy"
IDENTITY_RESOLUTION="src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy"
CARRIED_CALLABLE_IDENTITY="src/self_hosted/semantic/ast_expression_carried_callable_identity_owner.pgy"
DELIMITED_FACTS="src/self_hosted/semantic/delimited_range_fact_owner.pgy"
CALL_CHECK="src/self_hosted/semantic/call_check_owner.pgy"
TYPE_CANONICAL="src/self_hosted/semantic/ast_type_name_canonical_owner.pgy"

for term in \
    "func SourceByteAt" \
    "func SourceByteOf" \
    "func SourceByteIsAlpha" \
    "func SourceByteIsDigit" \
    "func SourceByteIsAlphaNum" \
    "func SourceByteIsWhitespace"; do
    require_text "$SOURCE_OWNER" "$term"
done

require_text "$SOURCE_OWNER" \
    "let c: Int = SourceByteAt(content, n, i);"
reject_region_text "$SOURCE_OWNER" \
    "func SkipWhitespaceAndComments" "SourceCharAt("
reject_region_text "$SOURCE_OWNER" \
    "func SkipWhitespaceAndComments" "Substring("

for function in ReadIdent MatchKeyword ReadNumber ReadString ExpectOpt \
    ConsumeStmtTerminatorOpt; do
    reject_region_text "$PARSER_CURSOR" "func $function" "ParserCharAt("
done
reject_region_text "$PARSER_CURSOR" "func MatchKeyword" "Substring("
reject_region_text "$PARSER_CURSOR" "func ExpectOpt" "Substring("
require_text "$PARSER_CURSOR" \
    "SourceByteIsAlphaNum(SourceByteAt(content, n, i))"
require_text "$PARSER_CURSOR" \
    "SubEqualsWithLen(content, n, i, kl, kw)"

if grep -Fq "CharAt(content" "$SEMANTIC_SCAN"; then
    fail "semantic scanner reopened allocating character reads"
fi
require_text "$SEMANTIC_SCAN" \
    "SourceByteAt(content, limit, i)"
require_text "$SEMANTIC_SCAN" \
    "SubEqualsWithLen(content, n, i, kl, kw)"
require_text "$OPERATOR_FACTS" "struct SemanticTopLevelOperatorFacts"
require_text "$OPERATOR_FACTS" \
    "func SemanticTopLevelOperatorFactsFromExpression"
if grep -Fq "CharAt(" "$OPERATOR_FACTS"; then
    fail "operator fact owner reopened allocating character reads"
fi
for function in CheckUndefinedIdentifiers CheckLogicalOperands \
    CheckBinaryOperands; do
    reject_region_text "$EXPR_VALIDATION" "func $function" "CharAt("
done
reject_region_text "$EXPR_TYPES" "func TopLevelOpType" "CharAt("
require_text "$EXPR_TYPES" \
    "SemanticTopLevelOperatorFactsFromExpression(expr)"
require_text "$EXPR_VALIDATION" \
    "SemanticTopLevelOperatorFactsFromExpression(text)"
require_text "$CALLABLE_RESOLUTION" \
    "func SemanticCallableNameRangeValid"
require_text "$CALLABLE_RESOLUTION" \
    'return StringReplace(source_name, ".", "_");'
require_text "$CALLABLE_RESOLUTION" \
    "func SemanticCallableCanonicalDeclaredNameEquals("
callable_predicate_region="$(sed -n \
    '/func SemanticCallableCanonicalDeclaredNameEquals/,/^}/p' \
    "$CALLABLE_RESOLUTION")"
grep -Fq "SubEqualsWithLen(" <<<"$callable_predicate_region" ||
    fail "canonical callable comparison lost exact range ownership"
if grep -Fq "Concat(" <<<"$callable_predicate_region"; then
    fail "canonical callable comparison reopened owned string materialization"
fi
for callable_compare_owner in \
    "$GENERIC_CALL:func SemanticGenericCallSignatureIndex" \
    "$IDENTITY_RESOLUTION:func SemanticExpressionDirectTargetSyntaxId" \
    "$CARRIED_CALLABLE_IDENTITY:func SemanticExpressionDeclaredCallableSyntaxId"
do
    callable_compare_path="${callable_compare_owner%%:*}"
    callable_compare_function="${callable_compare_owner#*:}"
    callable_compare_region="$(sed -n \
        "/$callable_compare_function/,/^}/p" "$callable_compare_path")"
    grep -Fq "SemanticCallableCanonicalDeclaredNameEquals(" \
        <<<"$callable_compare_region" ||
        fail "$callable_compare_function lost allocation-free canonical comparison"
    if grep -Fq "SemanticCallableCanonicalDeclaredName(" \
        <<<"$callable_compare_region"; then
        fail "$callable_compare_function reopened canonical name materialization"
    fi
done
if grep -Fq "CharAt(" "$CALLABLE_RESOLUTION"; then
    fail "callable resolution reopened allocating character reads"
fi
if grep -Fq "Substring(" "$CALLABLE_RESOLUTION"; then
    fail "callable resolution reopened segment copies"
fi
require_text "$DELIMITED_FACTS" "struct SemanticDelimitedRangeFacts"
require_text "$DELIMITED_FACTS" \
    "func SemanticNestedCommaRangeFactsFromSource"
if grep -Fq "SemanticCallArgumentRangeFactsFromSource" "$DELIMITED_FACTS" \
    "$EXPR_TYPES" "$CALL_CHECK"; then
    fail "call-only delimiter producer alias returned"
fi
require_text "$DELIMITED_FACTS" \
    "func SemanticSignatureRangeFactsFromSource"
if grep -Fq "CharAt(" "$DELIMITED_FACTS"; then
    fail "delimited range owner reopened allocating character reads"
fi
require_text "$CALL_CHECK" \
    "SemanticNestedCommaRangeFactsFromSource(args_src)"
require_text "$CALL_CHECK" "ArgCountFromFacts(argument_facts)"
require_text "$CALL_CHECK" "ParamCountFromFacts(signature_facts, sig)"
for function in CompareCallArgs ParamCount ArgCount FirstArg; do
    reject_region_text "$CALL_CHECK" "func $function" "CharAt("
done
for function in ParamTypes NthExpected CallArgAt; do
    reject_region_text "$EXPR_TYPES" "func $function" "CharAt("
done
require_text "$TYPE_CANONICAL" \
    'import "delimited_range_fact_owner.pgy";'
require_text "$TYPE_CANONICAL" \
    "SemanticNestedCommaRangeFactsFromSource(inner)"
if grep -Fq "CharAt(" "$TYPE_CANONICAL"; then
    fail "type canonical owner reopened allocating character reads"
fi

EVIDENCE="benchmarks/selfhost_source_scan_owner_evidence.json"
owner_set_sha256() {
    {
        for file in "$@"; do
            printf '%s:' "$file"
            sed 's/\r$//' "$file" | sha256sum | awk '{ print toupper($1) }'
        done
    } | sha256sum | awk '{ print toupper($1) }'
}
owner_hash="$(owner_set_sha256 \
        "$SOURCE_OWNER" \
        "$PARSER_CURSOR" \
        "$SEMANTIC_SCAN")"
require_text "$EVIDENCE" "\"owner_set_sha256\": \"$owner_hash\""
require_text "$EVIDENCE" '"parser_fixtures": 188'
require_text "$EVIDENCE" '"semantic_fixtures": 111'
require_text "$EVIDENCE" '"integrated_driver_c_llvm_byte_identical": true'

operator_owner_hash="$(owner_set_sha256 "$OPERATOR_FACTS")"
require_text "$EVIDENCE" "\"owner_set_sha256\": \"$operator_owner_hash\""
require_text "$EVIDENCE" '"char_at_reduction_percent": 60.4'
require_text "$EVIDENCE" \
    '"performance_verdict": "cpu-neutral-allocation-surface-reduction"'

callable_owner_hash="$(owner_set_sha256 \
    "$CALLABLE_RESOLUTION" "$GENERIC_CALL" "$IDENTITY_RESOLUTION" \
    "$CARRIED_CALLABLE_IDENTITY")"
require_text "$EVIDENCE" "\"owner_set_sha256\": \"$callable_owner_hash\""
require_text "$EVIDENCE" '"char_at_calls_after": 776073'
require_text "$EVIDENCE" '"cumulative_char_at_reduction_percent": 72.8'
require_text "$EVIDENCE" \
    '"performance_verdict": "cpu-inconclusive-allocation-surface-reduction"'

delimited_owner_hash="$(owner_set_sha256 "$DELIMITED_FACTS")"
require_text "$EVIDENCE" "\"owner_set_sha256\": \"$delimited_owner_hash\""
require_text "$EVIDENCE" '"char_at_calls_after": 424152'
require_text "$EVIDENCE" '"cumulative_char_at_reduction_percent": 85.1'
require_text "$EVIDENCE" \
    '"performance_verdict": "cpu-neutral-shared-range-facts"'

type_canonical_owner_hash="$(owner_set_sha256 "$TYPE_CANONICAL")"
require_text "$EVIDENCE" \
    "\"owner_set_sha256\": \"$type_canonical_owner_hash\""
require_text "$EVIDENCE" '"char_at_calls_after": 337974'
require_text "$EVIDENCE" '"cumulative_char_at_reduction_percent": 88.1'
require_text "$EVIDENCE" \
    '"performance_verdict": "cpu-neutral-type-range-facts"'

baseline_min="$(sed -n '/"baseline"/,/}/s/.*"elapsed_ms": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { min = 999999999 } { gsub(/ /, ""); if ($1 + 0 < min) min = $1 + 0 } END { print min }')"
candidate_max="$(sed -n '/"byte_scan_owner"/,/}/s/.*"elapsed_ms": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { max = 0 } { gsub(/ /, ""); if ($1 + 0 > max) max = $1 + 0 } END { print max }')"
required_max="$(sed -n '/"byte_scan_owner"/,/}/s/.*"required_max_elapsed_ms": \([0-9]*\).*/\1/p' "$EVIDENCE")"
awk -v baseline="$baseline_min" -v candidate="$candidate_max" \
    -v required="$required_max" \
    'BEGIN { exit !(candidate < baseline && candidate <= required) }' ||
    fail "source-scan benchmark evidence relationships drifted"

operator_max="$(sed -n '/"semantic_operator_fact_owner"/,/}/s/.*"elapsed_ms": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { max = 0 } { gsub(/ /, ""); if ($1 + 0 > max) max = $1 + 0 } END { print max }')"
operator_required="$(sed -n '/"semantic_operator_fact_owner"/,/}/s/.*"required_max_elapsed_ms": \([0-9]*\).*/\1/p' "$EVIDENCE")"
awk -v candidate="$operator_max" -v required="$operator_required" \
    'BEGIN { exit !(candidate <= required) }' ||
    fail "semantic operator fact benchmark exceeded its measured guardrail"

echo "[self-host-source-scan] byte/code and operator-fact owners, parity evidence, and allocation-free hot scans ok"
