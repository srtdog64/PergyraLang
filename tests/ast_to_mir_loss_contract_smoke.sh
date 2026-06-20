#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail() {
    echo "[ast-to-mir-loss-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$rel" ]] || fail "missing required file: $rel"
}

forbid_matches() {
    local label="$1"
    local pattern="$2"
    shift 2
    local matches

    matches="$(grep -RInE "$pattern" "$@" \
        --include='*.c' --include='*.h' 2>/dev/null || true)"
    if [[ -n "$matches" ]]; then
        echo "$matches" >&2
        fail "$label"
    fi
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$rel" || fail "$rel missing term: $term"
}

require_file "src/compiler/mir_source_shape.c"
require_file "src/compiler/mir_public_surface.c"
require_file "src/compiler/mir_lifecycle.c"
require_file "src/compiler/mir_fact_surface_validate.c"
require_file "src/codegen/llvm_mir_block_emit.c"
require_file "src/codegen/transpiler_mir_block_emit.c"
require_file "tests/cfg_body_dataflow_smoke.sh"
require_file "tests/mir_declaration_inventory_smoke.sh"

forbid_matches \
    "codegen/compiler reintroduced direct source_ast semantic reads" \
    "source_ast" \
    src/codegen src/compiler

forbid_matches \
    "codegen reintroduced declaration source_decl payload reads" \
    "mir_decl_header_source_decl|mir_routine_source_decl_of_type" \
    src/codegen

forbid_matches \
    "SSA DEF assignment type inference reintroduced AST target fallback" \
    "ast_identifier_name\\(inst->expr1\\)" \
    src/codegen src/compiler

forbid_matches \
    "MIR validators must consume source-shape/fact owners, not raw payload tests" \
    "mir_instruction_source_payload\\(|&& inst->ast != NULL" \
    src/compiler/mir_fact_surface_validate.c \
    src/compiler/mir_fact_terminator_validate.c

payload_refs="$(grep -RIn "mir_instruction_source_payload" \
    src/compiler src/codegen --include='*.c' --include='*.h' 2>/dev/null || true)"
payload_bad=""
while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    case "$line" in
        src/compiler/mir.h:*|\
        src/compiler/mir_source_shape.c:*|\
        src/compiler/mir_public_surface.c:*|\
        src/compiler/mir_lifecycle.c:*)
            ;;
        *)
            payload_bad+="$line"$'\n'
            ;;
    esac
done <<< "$payload_refs"
if [[ -n "$payload_bad" ]]; then
    echo "$payload_bad" >&2
    fail "source payload access must stay inside the MIR source-shape/provenance owner allowlist"
fi

require_text "src/compiler/mir_fact_surface_validate.c" \
    "source payload without surface usage facts"
require_text "src/compiler/mir_fact_surface_validate.c" \
    "missing MIR value expression fact"
require_text "src/compiler/mir_fact_surface_validate.c" \
    "ASSIGN is missing MIR assignment expression facts"
require_text "tests/cfg_body_dataflow_smoke.sh" \
    "MIR SSA use edges must consume MIR expr facts, not source payload fallback"
require_text "tests/cfg_body_dataflow_smoke.sh" \
    "noncfg-fallbacks: total=0 routines=0 recorded=yes"
require_text "tests/mir_declaration_inventory_smoke.sh" \
    "LLVM boundary call lowering must use explicit MIR signature facts"

echo "[ast-to-mir-loss-contract] AST->MIR semantic loss boundary is source-gated"
