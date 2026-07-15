#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

require_text() {
    local file="$1" text="$2"
    grep -Fq "$text" "$file" || {
        echo "[self-host-text-builder] missing '$text' in $file" >&2
        exit 1
    }
}

reject_region_text() {
    local file="$1" start="$2" text="$3"
    local region
    region="$(sed -n "/$start/,/^}/p" "$file")"
    if grep -Fq "$text" <<<"$region"; then
        echo "[self-host-text-builder] forbidden '$text' returned in $file::$start" >&2
        exit 1
    fi
}

require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    'let output: TextBuilder = TextBuilderNew(4096);'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    'TextBuilderAppend(output, defs[definition_index]);'
require_text "src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy" \
    'let out: TextBuilder = TextBuilderNew(n + 1);'
require_text "src/self_hosted/codegen/text/expr_scan.pgy" \
    'SubEqualsWithLen(s, n, i, fl, from)'
require_text "src/self_hosted/codegen/emission/literal_rewrite.pgy" \
    'SubEqualsWithLen(e, n, i, 4, "true")'
require_text "src/self_hosted/codegen/text/expr_scan.pgy" \
    'SubEqualsWithLen(e, n, i, 12, "ArrayLength(")'

reject_region_text "src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy" \
    'func RewriteBindingRefs' 'out = Concat(out'
reject_region_text "src/self_hosted/codegen/text/expr_scan.pgy" \
    'func ReplaceAll(' 'out = Concat(out'
reject_region_text "src/self_hosted/codegen/emission/literal_rewrite.pgy" \
    'func RewriteBoolLiterals' 'Substring(e, i, 4) == "true"'
reject_region_text "src/self_hosted/codegen/emission/literal_rewrite.pgy" \
    'func RewriteNoneLiteral' 'Substring(e, i, 4) == "None"'
reject_region_text "src/self_hosted/codegen/text/expr_scan.pgy" \
    'func RewriteArrayLength' 'Substring(e, i, 12) == "ArrayLength("'
reject_region_text "src/self_hosted/codegen/text/expr_scan.pgy" \
    'func FindTopLevelOp2' 'Substring(s, i, 2) == op'
if grep -Fq 'func ReplaceAllOutsideStrings' \
    "src/self_hosted/codegen/text/expr_scan.pgy"; then
    echo "[self-host-text-builder] repeated runtime-call scan owner returned" >&2
    exit 1
fi

require_text "src/self_hosted/compiler/expected/abi_layout_rows.txt" \
    '19|TextBuilder|PgyTextBuilder|data,length,capacity,finished|none|none|single_owner_linear'
require_text "src/self_hosted/compiler/expected/runtime_call_abi_rows.txt" \
    '241|selfhost-c-text-builder|finish|pgy_text_builder_finish|function|generated_runtime_helper|builder_ptr_allocator_ptr_to_string'
require_text "src/compiler/mir_text_builder_abi.c" \
    '"pgy_text_builder_new_export"'
require_text "src/compiler/mir_text_builder_abi.c" \
    'MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID'
require_text "tests/cases/text_builder_owner/nested_append.pgy" \
    'TextBuilderAppend(text, "nested");'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"required_max_peak_private_mb": 1250.0'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"all_six_runs_byte_identical": true'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"allocation_free_token_compare"'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"all_four_runs_byte_identical": true'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"llvm_candidate_byte_identical": true'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"all_two_runs_byte_identical": true'
require_text "benchmarks/selfhost_codegen_text_builder_evidence.json" \
    '"gen2_gen3_byte_identical": true'

EVIDENCE="benchmarks/selfhost_codegen_text_builder_evidence.json"
owner_set_sha256() {
    {
        for file in "$@"; do
            printf '%s:' "$file"
            sed 's/\r$//' "$file" | sha256sum | awk '{ print toupper($1) }'
        done
    } | sha256sum | awk '{ print toupper($1) }'
}
owner_hash="$(owner_set_sha256 \
        src/self_hosted/codegen/emission/program_emit.pgy \
        src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy \
        src/self_hosted/codegen/text/expr_scan.pgy \
        src/self_hosted/codegen/emission/expr_rewrite.pgy \
        src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy \
        src/self_hosted/codegen/runtime_abi/text_builder_runtime_owner.pgy \
        src/runtime/pgy_runtime_text_builder_inline.h)"
current_region="$(sed -n '/"current_semantic_bundle"/,/^  }/p' "$EVIDENCE")"
grep -Fq "\"owner_set_sha256\": \"$owner_hash\"" <<<"$current_region" || {
    echo "[self-host-text-builder] current semantic-bundle owner hash drifted" >&2
    exit 1
}
current_max="$(sed -n '/"current_semantic_bundle"/,/^  }/s/.*"max_peak_private_mb": \([0-9.]*\).*/\1/p' "$EVIDENCE")"
current_required_max="$(sed -n '/"current_semantic_bundle"/,/^  }/s/.*"required_max_peak_private_mb": \([0-9.]*\).*/\1/p' "$EVIDENCE")"
awk -v sample="$current_max" -v required="$current_required_max" \
    'BEGIN { exit !(sample > 0 && sample <= required) }' || {
    echo "[self-host-text-builder] current semantic-bundle evidence exceeded its ceiling" >&2
    exit 1
}

baseline_max="$(sed -n '/"baseline"/,/}/s/.*"peak_private_mb": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { max = 0 } { gsub(/ /, ""); if ($1 + 0 > max) max = $1 + 0 } END { print max }')"
previous_max="$(sed -n '/"text_builder_rung_2"/,/}/s/.*"peak_private_mb": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { max = 0 } { gsub(/ /, ""); if ($1 + 0 > max) max = $1 + 0 } END { print max }')"
candidate_max="$(sed -n '/"single_pass_runtime_rewrite"/,/}/s/.*"peak_private_mb": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { max = 0 } { gsub(/ /, ""); if ($1 + 0 > max) max = $1 + 0 } END { print max }')"
declared_max="$(sed -n '/"single_pass_runtime_rewrite"/,/}/s/.*"max_peak_private_mb": \([0-9.]*\).*/\1/p' "$EVIDENCE")"
required_max="$(sed -n '/"single_pass_runtime_rewrite"/,/}/s/.*"required_max_peak_private_mb": \([0-9.]*\).*/\1/p' "$EVIDENCE")"
awk -v baseline="$baseline_max" -v previous="$previous_max" -v sample="$candidate_max" \
    -v declared="$declared_max" -v required="$required_max" \
    'BEGIN { exit !(sample == declared && declared <= required && declared < previous && previous < baseline) }' || {
    echo "[self-host-text-builder] benchmark evidence relationships drifted" >&2
    exit 1
}
compare_max="$(sed -n '/"allocation_free_token_compare"/,/}/s/.*"max_peak_private_mb": \([0-9.]*\).*/\1/p' "$EVIDENCE")"
compare_required_max="$(sed -n '/"allocation_free_token_compare"/,/}/s/.*"required_max_peak_private_mb": \([0-9.]*\).*/\1/p' "$EVIDENCE")"
awk -v sample="$compare_max" -v required="$compare_required_max" \
    'BEGIN { exit !(sample <= required) }' || {
    echo "[self-host-text-builder] allocation-free compare evidence exceeded its ceiling" >&2
    exit 1
}
grep -Eq '"sha256": "[A-F0-9]{64}"' "$EVIDENCE" || {
    echo "[self-host-text-builder] benchmark evidence is missing a canonical SHA-256" >&2
    exit 1
}

echo "[self-host-text-builder] emission owner, single-pass runtime rewrite, ABI rows, and nested-mutation contract ok"
