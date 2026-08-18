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
require_text "src/self_hosted/codegen/emission/program_function_definition_block_owner.pgy" \
    'TextBuilderAppend(output, owned_definition[0]);'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    'TextBuilderAppend(output, definition_block);'
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

echo "[self-host-text-builder] emission owner, ABI rows, and nested-mutation contract ok"
