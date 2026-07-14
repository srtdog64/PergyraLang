#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$file"; then
        echo "[slot-analyzer-host-index] missing '$text' in $file" >&2
        exit 1
    fi
}

forbid_text() {
    local file="$1"
    local text="$2"
    if grep -Fq -- "$text" "$file"; then
        echo "[slot-analyzer-host-index] forbidden '$text' in $file" >&2
        exit 1
    fi
}

require_text src/semantic/slot_analyzer_lookup.c \
    "semantic_host_index_find_decl_by_name("
require_text src/semantic/slot_analyzer.c \
    "SlotFunctionLookup lookup = {sa->ctx, sa->program_root};"
require_text src/semantic/slot_analyzer.c \
    "SlotFunctionLookup lookup = {ctx, program_root};"
require_text src/semantic/type_checker_call_contract_helpers.c \
    "legacy_ast_param_summary_program(ctx), ctx);"
require_text docs/184_legacy_slot_interprocedural_hash_lookup.md \
    "Taming and Dissecting Recursions Through Interprocedural Weak Topological Ordering"

# The generic semantic wrapper can reopen a program-root compatibility path.
# The compiler-owned slot lane must consume the hash owner directly.
forbid_text src/semantic/slot_analyzer_lookup.c \
    "semantic_find_function_decl_by_name("
forbid_text src/semantic/slot_analyzer_access.c \
    "ast_program_statement_count("
forbid_text src/semantic/slot_analyzer_escape.c \
    "ast_program_statement_count("

echo "[slot-analyzer-host-index] hash owner and no-fallback gates passed"
