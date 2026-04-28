#!/usr/bin/env bash
# Regression gate for C/LLVM declaration-side MIR inventory usage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[mir-decl-inventory] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing required file: $rel"
}

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

for rel in \
    "src/codegen/llvm_internal.h" \
    "src/codegen/llvm_inventory_internal.h" \
    "src/codegen/llvm_pipeline.c" \
    "src/codegen/llvm_domain.c" \
    "src/codegen/llvm_backend.h" \
    "src/codegen/llvm_register.c" \
    "src/codegen/transpiler.h" \
    "src/codegen/transpiler.c" \
    "src/compiler/mir.h" \
    "src/compiler/mir_lower_public_api.h" \
    "src/compiler/mir_decl_headers.h" \
    "docs/100_beta_readiness_checklist.md" \
    "TODO.md"; do
    require_file "$rel"
done

for term in \
    "llvm_active_inventory" \
    "llvm_active_routine_inventory" \
    "llvm_mir_routine_inventory_from_program" \
    "llvm_find_decl_header_in_context" \
    "llvm_find_host_decl_header_in_context" \
    "llvm_host_decl_method_metadata" \
    "llvm_find_host_method_metadata_in_context" \
    "llvm_routine_inventory_get" \
    "llvm_find_decl_in_active_inventory" \
    "llvm_find_host_decl_in_active_inventory" \
    "llvm_find_host_method_decl_in_context" \
    "llvm_find_host_decl_methods_in_context" \
    "llvm_active_nominal_inventory" \
    "llvm_active_domain_inventory" \
    "mir_find_decl_header(ctx->mir, name)" \
    "llvm_is_host_decl_type"; do
    require_term "src/codegen/llvm_inventory_internal.h" "$term"
done

for term in "mir_active_inventory" "mir_active_externs"; do
    require_term "src/compiler/mir.h" "$term"
    require_term "src/compiler/mir_lower_public_api.h" "$term"
done

for rel in "src/codegen/llvm_inventory_internal.h" "src/codegen/transpiler.h"; do
    require_term "$rel" "mir_active_inventory(ctx->mir, decl_type, &nodes, &count)"
    require_term "$rel" "mir_active_externs(ctx->mir, &nodes, &count)"
done

for term in \
    "llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count)" \
    "llvm_find_host_decl_methods_in_context(ctx, cls_name, &methods, &method_count)" \
    "MIR-only LLVM path missing routine for class method" \
    "MIR-only LLVM path missing routine for function" \
    "declaration inventory is still AST-carried inside MIRProgram"; do
    require_term "src/codegen/llvm_pipeline.c" "$term"
done

require_term "src/codegen/llvm_domain.c" "llvm_active_domain_inventory(ctx, &inventory)"

for term in \
    "declaration / top-level inventory is carried through MIRProgram" \
    "dedicated declaration IR layer"; do
    require_term "src/codegen/llvm_backend.h" "$term"
done

for term in \
    "transpiler_active_inventory" \
    "transpiler_active_externs" \
    "transpiler_active_executables" \
    "transpiler_active_synthetic_executable_func" \
    "transpiler_active_has_main_function" \
    "transpiler_active_has_top_level_exec"; do
    require_term "src/codegen/transpiler.h" "$term"
done

for term in \
    "transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count)" \
    "transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count)" \
    "transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count)" \
    "transpiler_active_inventory(ctx, AST_INTENT_DECL, &intents, &intent_count)" \
    "transpiler_active_synthetic_executable_func(ctx)" \
    "transpiler_active_has_main_function(ctx)" \
    "transpiler_active_has_top_level_exec(ctx)"; do
    require_term "src/codegen/transpiler.c" "$term"
done

routine_raw_hits="$(
    for rel in \
        "src/codegen/llvm_pipeline.c" \
        "src/codegen/llvm_domain.c" \
        "src/codegen/llvm_intent.c"; do
        grep -EIn '\bctx->mir->routine_count\b|\bctx->mir->routines\b|\bmir->routine_count\b|\bmir->routines\b' \
            "$ROOT_DIR/$rel" | sed "s#^#$rel:#" || true
    done
)"
if [[ -n "$routine_raw_hits" ]]; then
    fail "LLVM routine inventory must go through llvm_active_routine_inventory outside the helper owner:
$routine_raw_hits"
fi

if grep -Fq "decl_header->ast == decl" \
    "$ROOT_DIR/src/codegen/llvm_inventory_internal.h"; then
    fail "llvm_host_decl_methods must be MIRDeclHeader metadata-first; do not require decl_header->ast == decl"
fi

for term in \
    "llvm_mir_decl_method_name" \
    "llvm_mir_decl_method_param_count" \
    "llvm_mir_decl_method_param" \
    "llvm_mir_decl_method_return_type" \
    "llvm_mir_decl_method_is_action_like"; do
    require_term "src/codegen/llvm_inventory_internal.h" "$term"
done

for term in \
    "llvm_mir_decl_method_param_count(method_meta)" \
    "llvm_mir_decl_method_return_type(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "MIR-only LLVM path missing enum method declaration metadata" \
    "MIR-only LLVM path missing class method declaration metadata"; do
    require_term "src/codegen/llvm_register.c" "$term"
done

for forbidden in \
    "llvm_mir_decl_method_name(method_meta, method)" \
    "llvm_mir_decl_method_param_count(method_meta, method)" \
    "llvm_mir_decl_method_return_type(method_meta, method)" \
    "llvm_mir_decl_method_is_action_like(method_meta, method)"; do
    if grep -Fq "$forbidden" \
        "$ROOT_DIR/src/codegen/llvm_register.c" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.h" \
        "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
        fail "LLVM MIR method accessors must not fall back to AST method nodes: $forbidden"
    fi
done

for term in \
    "MIRDeclMethod" \
    "method_metadata" \
    "method_metadata_count" \
    "mir_decl_header_set_methods" \
    "mir_link_decl_method_routines" \
    "params" \
    "param_count" \
    "return_type" \
    "has_routine" \
    "routine_index"; do
    if ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir.h" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_lower_public_api.h" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_decl_headers.h"; then
        fail "MIR declaration method metadata missing term: $term"
    fi
done

if awk '/decl = decl_header->ast;/{exit} {print}' \
    "$ROOT_DIR/src/codegen/llvm_inventory_internal.h" |
    grep -Fq "method->data.func_decl.name != NULL"; then
    fail "LLVM host method lookup must compare MIRDeclMethod.name before AST func_decl name"
fi

for term in \
    "MIR Declaration Debt Removal" \
    "AST-carried declaration inventory" \
    "dedicated declaration metadata view" \
    "make mir-declaration-inventory-test-smoke"; do
    require_term "docs/100_beta_readiness_checklist.md" "$term"
done

require_term "TODO.md" "declaration-side MIR-only debt"

domain_arrays=(
    functions intents abilities roles parties rosters worlds relations effects
    zones events types
)
allowed_raw_files=(
    "src/codegen/llvm_internal.h"
    "src/codegen/llvm_inventory_internal.h"
    "src/codegen/transpiler.h"
)
raw_hits=""
for path in "$ROOT_DIR"/src/codegen/llvm*.[ch] \
    "$ROOT_DIR/src/codegen/transpiler.c" \
    "$ROOT_DIR/src/codegen/transpiler.h"; do
    [[ -e "$path" ]] || continue
    rel="${path#$ROOT_DIR/}"
    allowed=false
    for allowed_file in "${allowed_raw_files[@]}"; do
        if [[ "$rel" == "$allowed_file" ]]; then
            allowed=true
            break
        fi
    done
    [[ "$allowed" == true ]] && continue
    for name in "${domain_arrays[@]}"; do
        if grep -Eq "\bctx->mir->$name\b|\bmir->$name\b" "$path"; then
            raw_hits+="$rel: raw MIR declaration array access: $name"$'\n'
        fi
    done
done
if [[ -n "$raw_hits" ]]; then
    fail "raw MIR declaration inventory array access outside allowed owner files:
$raw_hits"
fi

echo "[mir-decl-inventory] OK: C/LLVM declaration inventory use is helper-gated"
