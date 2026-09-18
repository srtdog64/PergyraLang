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

require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    'let output: TextBuilder = TextBuilderNew(4096);'
require_text "src/self_hosted/codegen/emission/program_function_definition_block_owner.pgy" \
    'TextBuilderAppend(output, owned_definition[0]);'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    'TextBuilderAppend(output, definition_block);'
for retired_expression_text_owner in \
    src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy \
    src/self_hosted/codegen/emission/literal_rewrite.pgy \
    src/self_hosted/codegen/emission/expr_rewrite.pgy \
    src/self_hosted/codegen/text/expr_scan.pgy; do
    [[ ! -e "$retired_expression_text_owner" ]] || {
        echo "[self-host-text-builder] retired expression text owner returned: $retired_expression_text_owner" >&2
        exit 1
    }
done
require_text "src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy" \
    'func RewriteExprFromSemanticGraphTracked('
require_text "src/self_hosted/codegen/emission/expression_c_text_materialization_owner.pgy" \
    'let output: TextBuilder = TextBuilderNew(64);'
require_text "src/self_hosted/codegen/emission/expression_c_text_materialization_owner.pgy" \
    'TextBuilderAppend(output, operator);'

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

echo "[self-host-text-builder] graph emission, ABI rows, and nested-mutation contract ok"
