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
    local text

    text="$(<"$ROOT_DIR/$rel")"
    [[ "$text" == *"$term"* ]] ||
        fail "$rel missing term: $term"
}

for rel in \
    "src/codegen/llvm_internal.h" \
    "src/codegen/llvm_inventory_internal.h" \
    "src/codegen/llvm_inventory_decl_lookup.h" \
    "src/codegen/llvm_inventory_host_methods.h" \
    "src/codegen/llvm_pipeline.c" \
    "src/codegen/llvm_domain.c" \
    "src/codegen/llvm_domain_method_helpers.c" \
    "src/codegen/llvm_domain_method_emit.c" \
    "src/codegen/llvm_domain_forward.c" \
    "src/codegen/llvm_domain_forward.h" \
    "src/codegen/llvm_backend.h" \
    "src/codegen/llvm_register.c" \
    "src/codegen/transpiler.h" \
    "src/codegen/transpiler.c" \
    "src/codegen/transpiler_decl_host_lookup.c" \
    "src/codegen/transpiler_domain_role_ability_emit.h" \
    "src/codegen/transpiler_generic_class_specialization_emit.h" \
    "src/codegen/transpiler_mir_ssa_names.h" \
    "src/compiler/mir.h" \
    "src/compiler/mir_lower_public_api.h" \
    "src/compiler/mir_decl_headers.h" \
    "docs/100_beta_readiness_checklist.md" \
    "TODO.md"; do
    require_file "$rel"
done

for term in \
    "llvm_active_inventory" \
    "llvm_find_decl_header_in_context" \
    "llvm_find_host_decl_header_in_context" \
    "llvm_find_decl_in_active_inventory" \
    "llvm_find_host_decl_in_active_inventory" \
    "mir_find_decl_header(ctx->mir, name)" \
    "llvm_is_host_decl_type"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
done

for term in \
    "llvm_host_decl_method_metadata" \
    "llvm_find_host_method_metadata_in_context" \
    "llvm_hosted_method_view" \
    "llvm_hosted_method_view_metadata" \
    "llvm_hosted_method_view_ast" \
    "llvm_find_host_method_decl_in_context" \
    "llvm_mir_decl_method_name" \
    "llvm_mir_decl_method_ast" \
    "llvm_mir_decl_method_param_count" \
    "llvm_mir_decl_method_param" \
    "llvm_mir_decl_method_return_type" \
    "llvm_mir_decl_method_is_action_like"; do
    require_term "src/codegen/llvm_inventory_host_methods.h" "$term"
done
if grep -Fq "llvm_find_host_decl_methods_in_context" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM host method inventory must not expose AST method-array lookup helpers"
fi
if grep -Fq "llvm_host_decl_methods(" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM host method inventory must be MIRDeclMethod metadata-only"
fi
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_methods"
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_count"
require_term "src/codegen/llvm_inventory_host_methods.h" \
    "view->count != view->ast_compat_count"
if grep -Fq "fallback_methods" "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM hosted method view must name AST compatibility paths explicitly, not as fallback_methods"
fi
if grep -Fq "fallback_count" "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM hosted method view must name AST compatibility counts explicitly, not as fallback_count"
fi

for term in \
    "llvm_active_routine_inventory" \
    "llvm_mir_routine_inventory_from_program" \
    "llvm_routine_inventory_get" \
    "llvm_active_nominal_inventory" \
    "llvm_active_domain_inventory"; do
    require_term "src/codegen/llvm_inventory_internal.h" "$term"
done

for term in "mir_active_inventory" "mir_active_externs"; do
    require_term "src/compiler/mir.h" "$term"
    require_term "src/compiler/mir_lower_public_api.h" "$term"
done

for rel in "src/codegen/llvm_inventory_decl_lookup.h" "src/codegen/transpiler.h"; do
    require_term "$rel" "mir_active_inventory(ctx->mir, decl_type, &nodes, &count)"
done
for rel in "src/codegen/llvm_inventory_internal.h" "src/codegen/transpiler.h"; do
    require_term "$rel" "mir_active_externs(ctx->mir, &nodes, &count)"
done

for term in \
    "llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count)" \
    "llvm_hosted_method_view_from_decl(ctx, cls_name, decl)" \
    "method_view.uses_mir_metadata" \
    "method_meta->has_routine" \
    "method_meta->routine_index" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing routine for class method" \
    "MIR-only LLVM path missing routine for function" \
    "declaration inventory is still AST-carried inside MIRProgram"; do
    require_term "src/codegen/llvm_pipeline.c" "$term"
done
if grep -Fq "llvm_find_mir_method_routine" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline class-method emission must use linked MIRDeclMethod routine indexes, not local routine fallback search"
fi
if grep -Fq "llvm_find_host_decl_methods_in_context(ctx, cls_name" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline class-method emission must consume MIRDeclMethod metadata, not AST method arrays"
fi
if grep -Eq 'decl->data\.class_decl\.method_count|decl->data\.class_decl\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must use LLVMHostedMethodView for class-method inventory guards"
fi

require_term "src/codegen/llvm_internal_api.h" "llvm_set_mir_inventory_missing"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_inventory_missing"
require_term "src/codegen/llvm_error.c" "PGY_CODE_LLVM_MIR_ROUTINE_MISSING"
require_term "src/codegen/llvm_error.c" "PGY_FIX_INSPECT_MIR_INVENTORY"

if grep -A8 -F "MIR-only LLVM path missing intent routine" \
    "$ROOT_DIR/src/codegen/llvm_intent.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM intent MIR-missing diagnostics must use llvm_set_mir_inventory_missing"
fi
if grep -A8 -F "MIR-only LLVM path missing routine for function" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM pipeline MIR-missing diagnostics must use llvm_set_mir_inventory_missing"
fi

require_term "src/codegen/llvm_domain.c" "llvm_active_domain_inventory(ctx, &inventory)"

for term in \
    "declaration / top-level inventory is carried through MIRProgram" \
    "dedicated declaration IR layer"; do
    require_term "src/codegen/llvm_backend.h" "$term"
done

for term in \
    "transpiler_active_inventory" \
    "TranspilerMIRRoutineInventory" \
    "transpiler_active_routine_inventory" \
    "transpiler_mir_routine_inventory_from_program" \
    "transpiler_routine_inventory_get" \
    "transpiler_active_routine_count" \
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
for term in \
    "transpiler_find_method_in_mir_header" \
    "mir_find_decl_header(ctx->mir, host_type_name)" \
    "transpiler_hosted_method_view_from_decl(ctx, host_type_name, decl)" \
    "header->method_metadata_count" \
    "method->name" \
    "method->ast"; do
    require_term "src/codegen/transpiler_decl_host_lookup.c" "$term"
done
if grep -RIn 'transpiler_decl_methods_local' "$ROOT_DIR/src/codegen"; then
    fail "C backend must not expose public AST method-array lookup seam"
fi
for rel in \
    "src/codegen/transpiler_block_intent_helpers.h" \
    "src/codegen/transpiler_projection_sync_helpers.h"; do
    require_term "$rel" "find_nominal_host_method_decl(ctx"
    if grep -Eq 'data\.class_decl\.methods\[[^]]+\]|data\.class_decl\.method_count' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must use the C backend MIR-aware host-method lookup seam"
    fi
done
for term in \
    "mir_find_decl_header(ctx->mir, owner_name)" \
    "header->method_metadata_count" \
    "transpiler_mir_decl_method_routine(ctx, method)"; do
    require_term "src/codegen/transpiler_mir_ssa_names.h" "$term"
done
for term in \
    "method->has_routine" \
    "transpiler_active_routine_inventory(ctx, &inventory)" \
    "transpiler_routine_inventory_get(&inventory, method->routine_index)" \
    "transpiler_mir_decl_method_param_count" \
    "transpiler_mir_decl_method_param" \
    "transpiler_mir_decl_method_return_type" \
    "transpiler_mir_decl_method_is_action_like"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
for term in \
    "emit_hosted_method_forward_decl_from_metadata" \
    "transpiler_mir_decl_method_param_count(method_meta)" \
    "transpiler_mir_decl_method_return_type(method_meta)" \
    "transpiler_mir_decl_method_param(method_meta, j)"; do
    require_term "src/codegen/transpiler_func_forward_helpers.h" "$term"
done
for rel in \
    "src/codegen/transpiler_class_decl_emit.h" \
    "src/codegen/transpiler_enum_decl_emit.h"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_hosted_method_view_ast(&method_view, i)"
    require_term "$rel" "transpiler_hosted_method_view_routine(ctx, &method_view, i)"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
require_term "src/codegen/transpiler_generic_class_specialization_emit.h" \
    "transpiler_hosted_method_view_from_decl(ctx, base_class_name"
require_term "src/codegen/transpiler_generic_class_specialization_emit.h" \
    "transpiler_hosted_method_view_ast(&method_view, i)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.h" \
    "transpiler_hosted_method_view_routine(ctx,"
require_term "src/codegen/transpiler_generic_class_specialization_emit.h" \
    "emit_hosted_method_forward_decl_from_metadata"
if grep -Eq 'class_decl->data\.class_decl\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.h"; then
    fail "generic class specialization must consume TranspilerHostedMethodView, not index AST method arrays"
fi
for term in \
    "TranspilerHostedMethodView" \
    "ast_compat_methods" \
    "ast_compat_count" \
    "view->count != view->ast_compat_count" \
    "transpiler_hosted_method_view(" \
    "transpiler_hosted_method_view_metadata(" \
    "transpiler_mir_decl_method_name(" \
    "transpiler_mir_decl_method_ast(" \
    "transpiler_mir_decl_method_routine(" \
    "transpiler_hosted_method_view_routine(" \
    "transpiler_hosted_method_view_from_decl(" \
    "transpiler_hosted_method_view_ast(" \
    "if (view->requires_mir_metadata)" \
    "transpiler_hosted_method_view_missing_mir_metadata("; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
if grep -Fq "fallback_methods" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h"; then
    fail "C hosted method view must name AST compatibility paths explicitly, not as fallback_methods"
fi
if grep -Fq "fallback_count" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h"; then
    fail "C hosted method view must name AST compatibility counts explicitly, not as fallback_count"
fi
for rel in \
    "src/codegen/transpiler_domain_nominal_emit.h" \
    "src/codegen/transpiler_zone_decl_emit.h" \
    "src/codegen/transpiler_world_select_event_emit.h"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_hosted_method_view_ast(&method_view, i)"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
if grep -RIn "emit_hosted_method_forward_decl_named" "$ROOT_DIR/src/codegen"; then
    fail "C hosted method forward declarations must use MIRDeclMethod metadata-first helper"
fi
require_term "src/codegen/transpiler_domain_role_ability_emit.h" \
    "const TranspilerHostedMethodView *method_view"
require_term "src/codegen/transpiler_domain_role_ability_emit.h" \
    "transpiler_hosted_method_view_metadata(method_view, i)"
require_term "src/codegen/transpiler_domain_role_ability_emit.h" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
if grep -Eq 'emit_hosted_methods_from_mir_or_error_local\([^)]*ASTNode \*\*methods|emit_hosted_methods_from_mir_or_error_local\([^)]*size_t method_count' \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h"; then
    fail "hosted method body emission must accept TranspilerHostedMethodView, not AST method arrays"
fi
require_term "src/codegen/transpiler_class_decl_emit.h" \
    "transpiler_hosted_method_view_from_decl(ctx, name"
require_term "src/codegen/transpiler_class_decl_emit.h" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_enum_decl_emit.h" \
    "transpiler_hosted_method_view_from_decl(ctx, ename"
require_term "src/codegen/transpiler_enum_decl_emit.h" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_domain_role_ability_emit.h" \
    "transpiler_hosted_method_view_missing_mir_metadata(method_view)"
for rel in \
    "src/codegen/transpiler_class_decl_emit.h" \
    "src/codegen/transpiler_enum_decl_emit.h" \
    "src/codegen/transpiler_generic_class_specialization_emit.h"; do
    if grep -Fq "transpiler_find_mir_method(ctx" "$ROOT_DIR/$rel"; then
        fail "$rel must use TranspilerHostedMethodView routine metadata, not a secondary method lookup"
    fi
done
if sed -n '1,70p' "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h" \
    | grep -Fq "transpiler_find_mir_method(ctx"; then
    fail "hosted role/domain method emission must use MIRDeclMethod routine metadata, not a secondary method lookup"
fi
if grep -RIn "transpiler_find_mir_method" "$ROOT_DIR/src/codegen"; then
    fail "generic C method lookup helper name must not reappear; remaining role include seam is explicit"
fi
require_term "src/codegen/transpiler_mir_ssa_names.h" \
    "transpiler_find_role_impl_mir_method"
c_method_raw_hits="$(
    c_method_files=()
    for path in "$ROOT_DIR"/src/codegen/transpiler*.c \
        "$ROOT_DIR"/src/codegen/transpiler*.h; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        case "$rel" in
            src/codegen/transpiler_decl_lookup.h)
                continue
                ;;
        esac
        c_method_files+=("$path")
    done
    if ((${#c_method_files[@]})); then
        grep -EHIn 'data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.methods\[[^]]+\]|data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.method_count' \
            "${c_method_files[@]}" | sed "s#^$ROOT_DIR/##" || true
    fi
)"
if [[ -n "$c_method_raw_hits" ]]; then
    fail "C backend hosted-method emission must use TranspilerHostedMethodView outside method-view owners:
$c_method_raw_hits"
fi

c_routine_raw_hits="$(
    c_routine_files=()
    for path in "$ROOT_DIR"/src/codegen/*.c \
        "$ROOT_DIR"/src/codegen/*.h; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        case "$rel" in
            src/codegen/llvm*|\
            src/codegen/transpiler.h|\
            src/codegen/transpiler_decl_lookup.h)
                continue
                ;;
        esac
        c_routine_files+=("$path")
    done
    if ((${#c_routine_files[@]})); then
        grep -EHIn '\bctx->mir->routine_count\b|\bctx->mir->routines\b|\bmir->routine_count\b|\bmir->routines\b' \
            "${c_routine_files[@]}" | sed "s#^$ROOT_DIR/##" || true
    fi
)"
if [[ -n "$c_routine_raw_hits" ]]; then
    fail "C backend routine inventory must use TranspilerMIRRoutineInventory outside helper owners:
$c_routine_raw_hits"
fi

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
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "llvm_host_decl_methods must be MIRDeclHeader metadata-first; do not require decl_header->ast == decl"
fi

for term in \
    "llvm_find_host_method_metadata_in_context" \
    "method_meta->has_routine" \
    "llvm_routine_inventory_get" \
    "return NULL;" \
    "routine->kind == MIR_SCOPE_METHOD"; do
    require_term "src/codegen/llvm_domain_method_helpers.c" "$term"
done
if grep -Fq "routine->ast == method" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_helpers.c"; then
    fail "LLVM domain method helper must not use AST identity as routine fallback"
fi
if grep -Fq "strcmp(routine->owner_name, owner_name)" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_helpers.c"; then
    fail "LLVM domain method helper must consume linked MIRDeclMethod routine indexes, not owner/name routine search"
fi
if grep -Fq "routine->ast == method->ast" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.h"; then
    fail "MIRDeclMethod routine linking must not use AST identity matching"
fi
for term in \
    "mir_decl_header_set_role_impl_methods" \
    "mir_role_impl_method_count" \
    "case AST_ROLE_DECL"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
done
for term in \
    "hir->role_count" \
    "mir_record_decl_header(mir, hir->roles[i])"; do
    require_term "src/compiler/mir_lower_public_api.h" "$term"
done
if grep -Fq "routine->ast == method_decl" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.h"; then
    fail "C MIR method lookup must use MIRDeclMethod routine metadata, not AST identity fallback"
fi

for term in \
    "llvm_mir_decl_method_param_count(method_meta)" \
    "llvm_mir_decl_method_return_type(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "llvm_hosted_method_view_from_decl(ctx, enum_name, stmt)" \
    "llvm_hosted_method_view_from_decl(ctx, cls_name, stmt)" \
    "llvm_hosted_method_view_missing_mir_metadata(&enum_method_view)" \
    "llvm_hosted_method_view_missing_mir_metadata(&class_method_view)" \
    "llvm_mir_decl_method_ast(method_meta)" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing enum method declaration metadata" \
    "MIR-only LLVM path missing class method declaration metadata"; do
    require_term "src/codegen/llvm_register.c" "$term"
done
if grep -Eq 'for[[:space:]]*\([^)]*stmt->data\.(enum_decl|class_decl)\.method_count' \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registration must iterate MIRDeclMethod metadata, not AST method_count"
fi
if grep -Eq 'stmt->data\.(enum_decl|class_decl)\.method_count' \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registration must use LLVMHostedMethodView for method-count guards"
fi
if grep -Eq 'stmt->data\.(enum_decl|class_decl)\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registration must not index AST method arrays"
fi

domain_method_forward_body="$(
    awk '
        /llvm_emit_domain_method_forward_decls\(LLVMGenCtx \*ctx,/ { in_body = 1 }
        /llvm_emit_domain_ability_vtables\(LLVMGenCtx \*ctx,/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
)"
for term in \
    "llvm_hosted_method_view_metadata(methods, j)" \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_return_type_metadata_first"; do
    grep -Fq "$term" <<<"$domain_method_forward_body" ||
        fail "LLVM domain method forward declarations must be MIRDeclMethod metadata-first: missing $term"
done
if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' \
    <<<"$domain_method_forward_body"; then
    fail "LLVM domain method forward declarations must not read AST method param_count/return_type directly"
fi
require_term "src/codegen/llvm_domain_forward.h" \
    "const LLVMHostedMethodView *methods"
require_term "src/codegen/llvm_domain_forward.c" \
    "llvm_hosted_method_view_metadata(methods, j)"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "LLVMHostedMethodView method_view"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "llvm_hosted_method_view_metadata(&method_view, j)"
if grep -Fq "llvm_find_mir_method_routine_local(ctx," \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"; then
    fail "LLVM hosted domain method body emission must use linked MIRDeclMethod routine indexes, not AST/name routine search"
fi
if grep -Eq 'llvm_emit_domain_method_forward_decls\([^)]*ASTNode \*\*methods|llvm_emit_domain_method_forward_decls\([^)]*size_t method_count' \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.h" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"; then
    fail "LLVM domain method forward declarations must accept LLVMHostedMethodView, not AST method arrays"
fi
llvm_method_raw_hits="$(
    llvm_method_files=()
    for path in "$ROOT_DIR"/src/codegen/llvm*.[ch]; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        case "$rel" in
            src/codegen/llvm_inventory_host_methods.h|\
            src/codegen/llvm_domain_decl_parts_helpers.h)
                continue
                ;;
        esac
        llvm_method_files+=("$path")
    done
    if ((${#llvm_method_files[@]})); then
        grep -EHIn 'data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.methods\[[^]]+\]|data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.method_count' \
            "${llvm_method_files[@]}" | sed "s#^$ROOT_DIR/##" || true
    fi
)"
if [[ -n "$llvm_method_raw_hits" ]]; then
    fail "LLVM hosted-method emission must use LLVMHostedMethodView outside method-view owners:
$llvm_method_raw_hits"
fi

for forbidden in \
    "llvm_mir_decl_method_name(method_meta, method)" \
    "llvm_mir_decl_method_param_count(method_meta, method)" \
    "llvm_mir_decl_method_return_type(method_meta, method)" \
    "llvm_mir_decl_method_is_action_like(method_meta, method)"; do
    if grep -Fq "$forbidden" \
        "$ROOT_DIR/src/codegen/llvm_register.c" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.h" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
        "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
        fail "LLVM MIR method accessors must not fall back to AST method nodes: $forbidden"
    fi
done

require_term "src/codegen/llvm_inventory_host_methods.h" \
    "if (view->requires_mir_metadata)"

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
for term in \
    "mir_validate_decl_header_ast_compat" \
    "mir_validate_decl_header_metadata" \
    "AST method compatibility drift" \
    "AST payload drift" \
    "name metadata drift" \
    "duplicates declaration header" \
    "pointer-self ABI metadata drift" \
    "method metadata count" \
    "signature metadata drift" \
    "routine index"; do
    require_term "src/compiler/mir_fact_validate.h" "$term"
done
require_term "src/compiler/mir_public_surface.h" \
    "mir_validate_decl_header_metadata(mir, error_message)"
require_term "src/tests/mir/test_mir_lowering_part_b.cases.h" \
    "MIR validator rejects hosted method signature metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_b.cases.h" \
    "MIR validator rejects declaration header name metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_b.cases.h" \
    "MIR declaration headers preserve pointer-self ABI shape"
require_term "src/tests/mir/test_mir_lowering_part_b.cases.h" \
    "MIR validator rejects pointer-self ABI metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_b.cases.h" \
    "MIR validator rejects duplicate declaration header names"

if awk '/decl = decl_header->ast;/{exit} {print}' \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" |
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
    "src/codegen/llvm_inventory_decl_lookup.h"
    "src/codegen/llvm_inventory_host_methods.h"
    "src/codegen/transpiler.h"
)
raw_hits=""
domain_array_pattern="$(
    IFS='|'
    printf '%s' "${domain_arrays[*]}"
)"
raw_scan_files=()
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
    raw_scan_files+=("$path")
done
if ((${#raw_scan_files[@]})); then
    raw_hits="$(
        grep -EHIn "\\b(ctx->mir|mir)->($domain_array_pattern)\\b" \
            "${raw_scan_files[@]}" |
            sed "s#^$ROOT_DIR/##; s#:#: raw MIR declaration array access: #" ||
            true
    )"
fi
if [[ -n "$raw_hits" ]]; then
    fail "raw MIR declaration inventory array access outside allowed owner files:
$raw_hits"
fi

echo "[mir-decl-inventory] OK: C/LLVM declaration inventory use is helper-gated"
