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
    "src/codegen/llvm_inventory_internal.c" \
    "src/codegen/llvm_inventory_internal.h" \
    "src/codegen/llvm_inventory_decl_lookup.c" \
    "src/codegen/llvm_inventory_decl_lookup.h" \
    "src/codegen/host_decl_compat.c" \
    "src/codegen/host_decl_compat.h" \
    "src/codegen/llvm_inventory_host_methods.h" \
    "src/codegen/llvm_pipeline.c" \
    "src/codegen/llvm_main_wrapper.c" \
    "src/codegen/llvm_domain.c" \
    "src/codegen/llvm_domain_method_helpers.c" \
    "src/codegen/llvm_domain_method_emit.c" \
    "src/codegen/llvm_domain_forward.c" \
    "src/codegen/llvm_domain_forward.h" \
    "src/codegen/llvm_decl_authority.c" \
    "src/codegen/llvm_decl_authority.h" \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_backend.h" \
    "src/codegen/llvm_register.c" \
    "src/codegen/transpiler.h" \
    "src/codegen/transpiler_inventory_view.c" \
    "src/codegen/transpiler_inventory_view.h" \
    "src/codegen/transpiler.c" \
    "src/codegen/transpiler_decl_host_lookup.c" \
    "src/codegen/transpiler_domain_role_ability_emit.c" \
    "src/codegen/transpiler_domain_role_ability_emit.h" \
    "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "src/codegen/transpiler_mir_role_lookup.c" \
    "src/codegen/transpiler_mir_ssa_names.h" \
    "src/compiler/mir.h" \
    "src/compiler/mir_lower_public_api.h" \
    "src/compiler/mir_public_surface.c" \
    "src/compiler/mir_decl_headers.c" \
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
    "llvm_find_host_decl_in_active_inventory"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
done

for term in \
    "kPgyHostDeclCompatTypes" \
    "AST_PARTY_DECL" \
    "AST_ROLE_DECL" \
    "AST_ROSTER_DECL" \
    "pgy_host_decl_compat_types" \
    "pgy_host_decl_compat_is_type" \
    "pgy_host_decl_compat_name" \
    "case AST_PARTY_DECL" \
    "case AST_ROLE_DECL" \
    "case AST_ROSTER_DECL"; do
    require_term "src/codegen/host_decl_compat.c" "$term"
done

for term in \
    "llvm_active_inventory" \
    "mir_find_decl_header(ctx->mir, name)" \
    "llvm_is_host_decl_type" \
    "pgy_host_decl_compat_is_type(decl_type)" \
    "pgy_host_decl_compat_types(&host_type_count)" \
    "host_types[i]" \
    "pgy_host_decl_compat_name(node)" \
    "return llvm_is_host_decl_type(decl->type)" \
    "llvm_decl_node_name(decl)"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.c" "$term"
done

for term in \
    "llvm_host_decl_method_metadata" \
    "llvm_find_host_method_metadata_in_context" \
    "llvm_hosted_method_view" \
    "llvm_hosted_method_view_metadata" \
    "llvm_hosted_method_view_source_ast" \
    "llvm_find_host_method_decl_in_context" \
    "llvm_mir_decl_method_name" \
    "llvm_mir_decl_method_source_ast" \
    "llvm_mir_decl_method_param_count" \
    "llvm_mir_decl_method_param" \
    "llvm_mir_decl_method_return_type" \
    "llvm_mir_decl_method_is_action_like" \
    "llvm_mir_decl_method_routine" \
    "llvm_hosted_method_view_routine"; do
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
if grep -RInE 'llvm_(hosted_method_view|mir_decl_method)_ast' \
    "$ROOT_DIR/src/codegen"; then
    fail "LLVM declaration method source compatibility accessors must use *_source_ast names"
fi
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_methods"
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_count"
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "view->count != view->ast_compat_count"
require_term "src/codegen/host_decl_compat.c" \
    "ast_role_impl_method_total_count"
require_term "src/codegen/host_decl_compat.c" \
    "view.count = (size_t)-1"
require_term "src/codegen/host_decl_compat.c" \
    "case AST_ROLE_DECL"
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "pgy_host_method_compat_view_from_decl(decl, llvm_active_has_mir(ctx))"
if grep -Fq "llvm_hosted_method_view(" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c" \
    && grep -Fq "NULL, 0)" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must preserve AST compatibility counts when a MIR header exists"
fi
if grep -Fq "fallback_methods" "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must name AST compatibility paths explicitly, not as fallback_methods"
fi
if grep -Fq "fallback_count" "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must name AST compatibility counts explicitly, not as fallback_count"
fi
for term in \
    "method->has_routine" \
    "llvm_active_routine_inventory(ctx, &inventory)" \
    "llvm_routine_inventory_get(&inventory, method->routine_index)" \
    "llvm_mir_decl_method_routine(" \
    "llvm_hosted_method_view_routine("; do
    require_term "src/codegen/llvm_inventory_host_methods.c" "$term"
done
if grep -RInE 'method(_meta)?->(has_routine|routine_index)' \
    "$ROOT_DIR/src/codegen"/llvm_*.c \
    "$ROOT_DIR/src/codegen"/llvm_*.h \
    | grep -v "src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method routine lookup must go through llvm_mir_decl_method_routine/llvm_hosted_method_view_routine"
fi

for term in \
    "llvm_active_routine_inventory" \
    "llvm_mir_routine_inventory_from_program" \
    "llvm_routine_inventory_get" \
    "llvm_mir_routine_source_ast" \
    "llvm_mir_routine_source_ast_of_type" \
    "llvm_active_nominal_inventory" \
    "llvm_active_domain_inventory"; do
    require_term "src/codegen/llvm_inventory_internal.h" "$term"
done
for term in \
    "llvm_mir_routine_source_ast(const MIRRoutine *routine)" \
    "llvm_mir_routine_source_ast_of_type(const MIRRoutine *routine" \
    "return routine != NULL ? routine->ast : NULL"; do
    require_term "src/codegen/llvm_inventory_internal.c" "$term"
done

for term in "mir_active_inventory" "mir_active_externs"; do
    require_term "src/compiler/mir.h" "$term"
    require_term "src/compiler/mir_public_surface.c" "$term"
done
for term in \
    "mir_find_function_decl" \
    "mir_active_inventory" \
    "mir_active_externs" \
    "mir_find_decl_header" \
    "mir_run_liveness_pass" \
    "mir_run_dce_pass"; do
    if grep -Eq "^[[:space:]]*(ASTNode[[:space:]*]+|const[[:space:]]+MIRDeclHeader[[:space:]*]+|void[[:space:]]*|bool[[:space:]]*)$term[[:space:]]*\\(" \
            "$ROOT_DIR/src/compiler/mir_lower_public_api.h"; then
        fail "MIR public query/pass wrapper '$term' must stay in mir_public_surface.c, not mir_lower_public_api.h"
    fi
done
if grep -Fq "ASTNode    **methods;" "$ROOT_DIR/src/compiler/mir.h"; then
    fail "MIRDeclHeader must not carry AST method-array pointers as inventory state"
fi
if grep -Fq "header->methods" \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
    fail "MIR declaration-header validation must consume method metadata, not AST method arrays"
fi

for rel in "src/codegen/llvm_inventory_decl_lookup.c" "src/codegen/transpiler_inventory_view.c"; do
    require_term "$rel" "mir_active_inventory(ctx->mir, decl_type, &nodes, &count)"
done
for rel in "src/codegen/llvm_inventory_internal.c" "src/codegen/transpiler_inventory_view.c"; do
    require_term "$rel" "mir_active_externs(ctx->mir, &nodes, &count)"
done

raw_ctx_mir_hits="$(
    grep -RIn 'ctx->mir' "$ROOT_DIR/src/codegen" |
        grep -Ev 'src/codegen/(llvm_api\.c|llvm_inventory_decl_lookup\.c|llvm_inventory_internal\.c|transpiler_entry\.c|transpiler_inventory_view\.c|transpiler_mir_emission_contract\.c):' || true
)"
if [[ -n "$raw_ctx_mir_hits" ]]; then
    fail "raw ctx->mir access must stay in backend entrypoints, inventory view/lookup owners, or MIR emission contract probes:
$raw_ctx_mir_hits"
fi

for term in \
    "llvm_register_active_nominal_types(ctx)" \
    "llvm_emit_class_method_bodies_from_inventory(ctx)" \
    "llvm_forward_declare_function_routines_from_inventory(" \
    "llvm_emit_function_routines_from_inventory(" \
    "llvm_validate_function_routine_bodies_from_inventory(" \
    "llvm_forward_declare_intent_routines_from_inventory(" \
    "llvm_emit_intent_routines_from_inventory(" \
    "llvm_emit_main_wrapper(ctx)" \
    "declaration inventory is still AST-carried inside MIRProgram"; do
    require_term "src/codegen/llvm_pipeline.c" "$term"
done
for term in \
    "llvm_active_synthetic_executable_func(ctx)" \
    "llvm_active_has_mir(ctx)" \
    "llvm_active_has_top_level_exec(ctx)" \
    "llvm_active_has_main_function(ctx)" \
    "llvm_active_uses_thread_pool(ctx)" \
    "LLVM thread-pool entry requires registered runtime function" \
    "LLVM event initialization requires generated event function"; do
    require_term "src/codegen/llvm_main_wrapper.c" "$term"
done
require_term "src/codegen/llvm_api.c" \
    "llvm_active_uses_intent_observability(ctx)"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_has_mir"
require_term "src/codegen/llvm_inventory_internal.c" \
    "llvm_active_has_mir(const LLVMGenCtx *ctx)"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_uses_intent_observability"
require_term "src/codegen/llvm_inventory_internal.c" \
    "pgy_mir_program_uses_intent_observability(ctx->mir)"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_uses_thread_pool"
require_term "src/codegen/llvm_inventory_internal.c" \
    "pgy_mir_program_uses_thread_pool(ctx->mir)"
for term in \
    "llvm_register_generic_template_decl(LLVMGenCtx *ctx, ASTNode *func_decl)" \
    "ctx->generic_templates[ctx->generic_template_count].name = name" \
    "llvm_lookup_generic_template(ctx, name)"; do
    require_term "src/codegen/llvm_backend_generic.c" "$term"
done
if grep -Fq "ctx->generic_templates" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not mutate the generic-template registry directly"
fi
if grep -Fq "routine->ast" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not reopen routine source AST for emit policy"
fi
if grep -Fq "MIR_SCOPE_" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not classify routine kinds locally for emit policy"
fi
if grep -Fq "mir_find_function_decl(ctx->mir, \"__pgy_top_level_exec\")" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM main wrapper must use the active executable inventory seam"
fi
if grep -Fq "mir_find_function_decl(ctx->mir, \"__pgy_top_level_exec\")" \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"; then
    fail "LLVM main wrapper must use the active executable inventory seam"
fi
if grep -Eq 'ctx->mir->has_(top_level_exec|main_function)' \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM main wrapper must use active top-level/main metadata helpers"
fi
if grep -Eq 'ctx->mir->has_(top_level_exec|main_function)' \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"; then
    fail "LLVM main wrapper must use active top-level/main metadata helpers"
fi
for term in \
    "llvm_forward_declare_function_routines_from_inventory(" \
    "llvm_emit_function_routines_from_inventory(" \
    "llvm_validate_function_routine_bodies_from_inventory(" \
    "llvm_mir_routine_source_ast_of_type(" \
    "llvm_register_generic_template_decl(ctx, func_decl)" \
    "llvm_emit_func_from_mir(routine, ctx)" \
    "MIR-only LLVM path missing routine for function"; do
    require_term "src/codegen/llvm_decl_routines.c" "$term"
done
require_term "src/codegen/llvm_decl.c" '#include "llvm_decl_authority.h"'
for term in \
    "llvm_decl_emit_zone_authority_check(LLVMGenCtx *ctx)" \
    "pgy_zone_authority_check_export" \
    "ast_zone_authorities(zone_decl" \
    "ast_zone_authority_subject_slot_name(authority)" \
    "llvm_set_mir_inventory_missing(ctx"; do
    require_term "src/codegen/llvm_decl_authority.c" "$term"
done
if grep -R "data\.zone_decl\.\(authorities\|authority_count\)" \
    "$ROOT_DIR/src/codegen/llvm_decl.c" \
    "$ROOT_DIR/src/codegen/llvm_decl_authority.c" >/dev/null; then
    fail "LLVM zone authority checks must use AST zone child accessors"
fi
for rel in \
    "src/codegen/llvm_decl.c" \
    "src/codegen/llvm_decl_authority.c" \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_forward.c" \
    "src/codegen/llvm_mir_emit.c"; do
    if grep -Fq "routine->ast" "$ROOT_DIR/$rel"; then
        fail "$rel must consume routine source AST through llvm_mir_routine_source_ast* accessors"
    fi
done
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_mir_routine_source_ast(routine)"
if grep -Fq "MIR-only LLVM path missing routine for function" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline function residual diagnostics must stay in the decl owner"
fi
for term in \
    "llvm_forward_declare_intent_routines_from_inventory(" \
    "llvm_mir_routine_source_ast_of_type(" \
    "llvm_forward_declare_intent(intent_decl, ctx)"; do
    require_term "src/codegen/llvm_intent_forward.c" "$term"
done
for term in \
    "llvm_emit_intent_routines_from_inventory(" \
    "llvm_mir_routine_source_ast_of_type(" \
    "llvm_emit_intent_decl(intent_decl, ctx)"; do
    require_term "src/codegen/llvm_intent.c" "$term"
done
if grep -Fq "llvm_forward_declare_intent(stmt, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline intent forward declaration must stay in the intent owner"
fi
if grep -Fq "llvm_emit_intent_decl(stmt, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline intent routine emission must stay in the intent owner"
fi
for term in \
    "llvm_emit_class_method_bodies_from_inventory(LLVMGenCtx *ctx)" \
    "llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count)" \
    "llvm_hosted_method_view_from_decl(ctx, cls_name, decl)" \
    "method_view.uses_mir_metadata" \
    "llvm_hosted_method_view_routine(" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing routine for class method"; do
    require_term "src/codegen/llvm_domain_method_emit.c" "$term"
done
require_term "src/codegen/llvm_domain_method_emit.h" \
    "llvm_emit_class_method_bodies_from_inventory"
require_term "src/codegen/llvm_internal_api.h" \
    "bool llvm_emit_class_method_bodies_from_inventory(LLVMGenCtx *ctx);"
if grep -Fq '#include "llvm_domain_method_emit.h"' \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must consume class-method emission through llvm_internal_api.h"
fi
register_decl_body="$(
    awk '
        /emit_program_from_mir:register_decl_items/ { in_body = 1 }
        /emit_program_from_mir:emit_domain_passes/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_pipeline.c"
)"
if ! grep -Fq "llvm_register_active_nominal_types(ctx)" \
        <<<"$register_decl_body"; then
    fail "LLVM pipeline nominal registration must call the register owner helper"
fi
if grep -Fq "llvm_active_nominal_inventory" <<<"$register_decl_body"; then
    fail "LLVM pipeline nominal registration must not reopen the active nominal inventory loop"
fi
if grep -Fq "llvm_active_nominal_inventory" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not directly iterate the active nominal inventory"
fi
if grep -Fq "llvm_hosted_method_view_from_decl(ctx, cls_name, decl)" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline class-method emission must stay in the method emit owner"
fi
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
require_term "src/codegen/llvm_internal_api.h" "llvm_set_mir_topology_invalid"
require_term "src/codegen/llvm_internal_api.h" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_inventory_missing"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_topology_invalid"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_error.c" "llvm_result_error_with_hints"
require_term "src/codegen/llvm_error.c" "llvm_result_error_fmt_with_hints"
require_term "src/codegen/llvm_error.c" "PGY_CODE_LLVM_MIR_ROUTINE_MISSING"
require_term "src/codegen/llvm_error.c" "PGY_CODE_MIR_TOPOLOGY_INVALID"
require_term "src/codegen/llvm_error.c" "PGY_CODE_MIR_INTENT_CARRIER_MISSING"
require_term "src/codegen/llvm_error.c" "PGY_FIX_INSPECT_MIR_INVENTORY"
require_term "src/codegen/llvm_error.c" "PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING"
require_term "src/codegen/llvm_error.c" "PGY_FIX_CHECK_INTENT_STEP_LOWERING"
if grep -RIn "PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING" "$ROOT_DIR/src/codegen" \
    | grep -v "src/codegen/llvm_error.c"; then
    fail "LLVM MIR-missing diagnostics must route through llvm_set_mir_inventory_missing"
fi
if grep -A3 -F "LLVM MIR pin block cannot resolve" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM MIR pin topology diagnostics must use llvm_set_mir_topology_invalid"
fi
if grep -A3 -F "MIR-only LLVM path missing owner metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM MIR owner topology diagnostics must use llvm_set_mir_topology_invalid"
fi
require_term "src/codegen/llvm_mir_contract.c" "llvm_set_mir_topology_invalid"
if grep -RIn "llvm_set_error_with_hints(ctx" \
    "$ROOT_DIR/src/codegen/llvm_mir_contract.c"; then
    fail "LLVM MIR contract diagnostics must use llvm_set_mir_topology_invalid"
fi
require_term "src/codegen/llvm_mir_contract.c" "llvm_validate_mir_for_codegen"
require_term "src/codegen/llvm_mir_contract.c" "llvm_result_error_with_hints(\"MIR program is NULL\""
require_term "src/codegen/llvm_mir_contract.c" "llvm_result_error_with_hints(\"MIR routine is missing name\""
require_term "src/codegen/llvm_mir_contract.c" "llvm_result_error_fmt_with_hints("
if grep -A10 -F "MIR routine '%s' emission topology invalid" \
    "$ROOT_DIR/src/codegen/llvm_mir_contract.c" | grep -Fq "llvm_result_error_fmt("; then
    fail "LLVM MIR topology preflight must attach structured diagnostic hints"
fi

if grep -A8 -F "MIR-only LLVM path missing intent routine" \
    "$ROOT_DIR/src/codegen/llvm_intent.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM intent MIR-missing diagnostics must use llvm_set_mir_inventory_missing"
fi
if grep -A8 -F "MIR-only LLVM path missing intent participant metadata" \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM intent participant inventory diagnostics must use llvm_set_mir_inventory_missing"
fi
require_term "src/codegen/llvm_intent_cleanup.c" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_intent_step_context.c" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_intent_step_context.c" \
    "out->dispatch_aliases = (const char **)ast_intent_step_who_names(step, NULL)"
if grep -Fq "step->data.intent_step.who_names[j]" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"; then
    fail "LLVM intent dispatch emission must consume LLVMIntentStepContext aliases"
fi
if grep -RIn "llvm_set_error_with_hints(ctx" \
    "$ROOT_DIR/src/codegen/llvm_intent_cleanup.c" \
    "$ROOT_DIR/src/codegen/llvm_intent_step_context.c"; then
    fail "LLVM intent carrier diagnostics must use llvm_set_mir_intent_carrier_missing"
fi
if grep -A8 -F "MIR-only LLVM path missing routine for function" \
    "$ROOT_DIR/src/codegen/llvm_decl_routines.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM pipeline MIR-missing diagnostics must use llvm_set_mir_inventory_missing"
fi
if grep -RIn "llvm_set_error(ctx" "$ROOT_DIR/src/codegen"/llvm_mir*.c \
    "$ROOT_DIR/src/codegen"/llvm_mir*.h \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR emission must route failures through MIR diagnostic helpers"
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
    "transpiler_mir_routine_source_ast" \
    "transpiler_mir_routine_source_ast_of_type" \
    "transpiler_active_routine_count" \
    "transpiler_active_decl_header" \
    "transpiler_active_externs" \
    "transpiler_active_executables" \
    "transpiler_active_synthetic_executable_func" \
    "transpiler_active_has_mir" \
    "transpiler_active_mir_identity" \
    "transpiler_active_has_main_function" \
    "transpiler_active_has_top_level_exec" \
    "transpiler_active_uses_intent_observability" \
    "transpiler_active_uses_thread_pool" \
    "transpiler_active_can_emit_intent_cleanup_from_mir"; do
    require_term "src/codegen/transpiler_inventory_view.h" "$term"
done
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_has_mir(const TranspilerCtx *ctx)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_mir_identity(const TranspilerCtx *ctx)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_decl_header(const TranspilerCtx *ctx, const char *name)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "pgy_mir_program_uses_intent_observability(ctx->mir)"
require_term "src/codegen/transpiler_entry.c" \
    "transpiler_active_uses_intent_observability(ctx)"
require_term "src/codegen/transpiler.c" \
    "transpiler_active_has_mir(ctx)"
if grep -Fq "ctx->mir" "$ROOT_DIR/src/codegen/transpiler.c"; then
    fail "C program emitter must use active MIR view helpers, not direct ctx->mir probes"
fi
require_term "src/codegen/transpiler_inventory_view.c" \
    "pgy_mir_program_uses_thread_pool(ctx->mir)"
for term in \
    "transpiler_mir_routine_source_ast(const MIRRoutine *routine)" \
    "transpiler_mir_routine_source_ast_of_type(" \
    "return routine != NULL ? routine->ast : NULL"; do
    require_term "src/codegen/transpiler_inventory_view.c" "$term"
done
for rel in \
    "src/codegen/transpiler_mir_emission_contract.c"; do
    if grep -Fq "routine->ast" "$ROOT_DIR/$rel"; then
        fail "$rel must consume routine source AST through transpiler_mir_routine_source_ast* accessors"
    fi
done
require_term "src/codegen/transpiler_mir_emission_contract.c" \
    "transpiler_mir_routine_source_ast(routine)"
require_term "src/codegen/transpiler_mir_emission_contract.c" \
    "transpiler_mir_routine_source_ast_of_type("

for term in \
    "transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count)" \
    "transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count)" \
    "transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count)" \
    "transpiler_active_inventory(ctx, AST_INTENT_DECL, &intents, &intent_count)" \
    "transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count)" \
    "transpiler_active_inventory(ctx, AST_PARTY_DECL, &parties, &party_count)" \
    "transpiler_active_inventory(ctx, AST_ROSTER_DECL, &rosters, &roster_count)" \
    "transpiler_active_synthetic_executable_func(ctx)" \
    "transpiler_active_has_main_function(ctx)" \
    "transpiler_active_has_top_level_exec(ctx)"; do
    require_term "src/codegen/transpiler.c" "$term"
done
for term in \
    "return pgy_host_decl_compat_name(decl)" \
    "transpiler_is_host_decl_type" \
    "return pgy_host_decl_compat_is_type(decl_type)" \
    "pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count)" \
    "host_lookup_types[i]" \
    "transpiler_find_domain_constructor_decl_local" \
    "pgy_host_decl_compat_constructor_domain_types(" \
    "&constructor_type_count" \
    "constructor_types[i]"; do
    require_term "src/codegen/transpiler_decl_lookup.c" "$term"
done
require_term "src/codegen/transpiler_call_constructor_result_emit.c" \
    "transpiler_find_domain_constructor_decl_local(ctx, fn)"
require_term "src/codegen/transpiler_mir_local_type_lookup.c" \
    "transpiler_find_domain_constructor_decl_local("
require_term "src/codegen/transpiler_let_emit.c" \
    "transpiler_find_domain_constructor_decl_local("
if grep -Fq "find_party_decl(ctx, fn)" \
    "$ROOT_DIR/src/codegen/transpiler_call_constructor_result_emit.c"; then
    fail "C constructor dispatch must consume the domain-constructor lookup seam instead of repeating host chains"
fi
if grep -Fq "find_zone_decl(ctx, callee_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local type lookup must consume the domain-constructor lookup seam instead of repeating host chains"
fi
if grep -Fq "find_zone_decl(ctx, ann_type_name)" \
    "$ROOT_DIR/src/codegen/transpiler_let_emit.c"; then
    fail "C annotated let constructor fallback must consume the domain-constructor lookup seam instead of repeating host chains"
fi
require_term "src/codegen/transpiler_intent_emit.c" \
    "find_zone_decl_in_program_view(ctx, step_zone_name)"
if grep -Fq "find_zone_decl(ctx, step_zone_name)" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit.c"; then
    fail "C intent step zone binding must consume the active inventory view instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "find_zone_decl_in_program_view(ctx, zone_type)"
if grep -Fq "find_zone_decl(ctx, zone_type)" \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_helpers.c"; then
    fail "C intent block zone-effect helpers must consume the active inventory view instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,"
if grep -Fq "return find_zone_decl(ctx, zone_type)" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C world-zone projection resolution must consume active inventory instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_find_decl_in_inventory_local((TranspilerCtx *)ctx,"
if grep -Fq "return find_zone_decl((TranspilerCtx *)ctx, zone_name)" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    fail "C world frontier lookup must consume active inventory instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,"
if grep -Fq "find_zone_decl(ctx, host_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA host recovery must consume active inventory for zone lookup"
fi
require_term "src/codegen/transpiler_func_forward_policy.c" \
    "transpiler_find_decl_in_inventory_local(ctx, AST_WORLD_DECL,"
if grep -Fq "find_world_decl(ctx, name)" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C function forward policy must consume active inventory for world lookup"
fi
for rel in \
    "src/codegen/transpiler_projection_sync.c" \
    "src/codegen/transpiler_overlay_projection.c" \
    "src/codegen/transpiler_expr_call_member_emit.c"; do
    require_term "$rel" "transpiler_find_decl_in_inventory_local("
    if grep -Fq "find_zone_decl(ctx, zone_type_name)" "$ROOT_DIR/$rel"; then
        fail "$rel must consume active inventory for world-zone projection/action context lookup"
    fi
done
require_term "src/codegen/transpiler_projection_sync.c" \
    "AST_EFFECT_DECL"
if grep -Fq "find_effect_decl(ctx, effect_name)" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "C world action effect sync must consume active inventory for effect lookup"
fi
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_find_decl_in_inventory_local("
if grep -Fq "find_effect_decl(ctx, ast_zone_layer_slot_layer_type(layer_slot))" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.c"; then
    fail "C zone effect bind must consume active inventory for effect lookup"
fi
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "transpiler_find_decl_in_inventory_local("
if grep -Fq "find_relation_decl(ctx," \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_relation_bind.c"; then
    fail "C zone relation bind must consume active inventory for relation lookup"
fi
c_domain_lookup_hits="$(
    grep -RInE 'find_(zone|world|relation|effect)_decl\(ctx,' \
        "$ROOT_DIR/src/codegen" \
        --include='transpiler*.c' --include='transpiler*.h' \
        | grep -v 'src/codegen/transpiler_decl_lookup.c:' \
        | grep -v 'src/codegen/transpiler_decl_lookup.h:' || true
)"
if [[ -n "$c_domain_lookup_hits" ]]; then
    fail "C backend domain declaration recovery must consume active inventory seams outside decl_lookup owner:
$c_domain_lookup_hits"
fi
require_term "src/codegen/transpiler_expr_type_infer.c" \
    "transpiler_has_known_nominal_type(ctx, name)"
if grep -Fq "find_subject_host_decl(ctx, name)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"; then
    fail "C expression type inference must consume known-nominal policy instead of repeating host chains"
fi
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_find_nominal_host_decl_local(ctx, type_name)"
if grep -Fq "find_relation_decl(ctx, type_name) != NULL" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C nominal host type predicate must consume nominal host lookup instead of repeating domain host chains"
fi
for term in \
    "transpiler_find_nominal_host_decl_local(ctx, type_name)" \
    "pgy_host_decl_compat_uses_pointer_self(decl)"; do
    require_term "src/codegen/transpiler_host_self_policy.c" "$term"
done
require_term "src/codegen/transpiler_expr_dispatch_emit.c" \
    "pgy_host_decl_compat_uses_pointer_self(host_decl)"
if grep -Fq "host_decl->type == AST_PARTY_DECL" \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"; then
    fail "C self-member dispatch must consume host pointer-self policy instead of repeating domain host chains"
fi
for term in \
    "transpiler_find_method_source_ast_in_mir_header" \
    "transpiler_decl_header_is_nominal_host(header)" \
    "transpiler_active_decl_header(ctx, host_type_name)" \
    "transpiler_active_mir_identity(ctx)" \
    "static const TranspilerHostOwnerLookup kTranspilerHostOwnerLookups[]" \
    "lookup->lookup_type" \
    "pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count)" \
    "host_lookup_types[i]" \
    "AST_PARTY_DECL" \
    "AST_ROLE_DECL" \
    "AST_ROSTER_DECL" \
    "transpiler_hosted_method_view_from_decl(ctx, host_type_name, decl)" \
    "header->method_metadata_count" \
    "method->name" \
    "method->source_ast"; do
    require_term "src/codegen/transpiler_decl_host_lookup.c" "$term"
done
if grep -Fq "ctx->mir" "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C host-decl lookup cache must use active MIR identity helpers, not direct ctx->mir probes"
fi
if grep -RIn 'transpiler_decl_methods_local' "$ROOT_DIR/src/codegen"; then
    fail "C backend must not expose public AST method-array lookup seam"
fi
if grep -RInE 'transpiler_(hosted_method_view|mir_decl_method)_ast' \
    "$ROOT_DIR/src/codegen"; then
    fail "C declaration method source compatibility accessors must use *_source_ast names"
fi
for rel in \
    "src/codegen/transpiler_intent_context.c" \
    "src/codegen/transpiler_domain_receiver_query.c"; do
    require_term "$rel" "find_nominal_host_method_decl(ctx"
    if grep -Eq 'data\.class_decl\.methods\[[^]]+\]|data\.class_decl\.method_count' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must use the C backend MIR-aware host-method lookup seam"
    fi
done
for term in \
    "transpiler_active_decl_header(ctx, owner_name)" \
    "header->ast_type != AST_ROLE_DECL" \
    "header->method_metadata_count" \
    "transpiler_mir_decl_method_routine(ctx, method)"; do
    require_term "src/codegen/transpiler_mir_role_lookup.c" "$term"
done
for term in \
    "method->has_routine" \
    "transpiler_is_host_decl_type(header->ast_type)" \
    "pgy_host_method_compat_view_from_decl(" \
    "transpiler_active_routine_inventory(ctx, &inventory)" \
    "transpiler_routine_inventory_get(&inventory, method->routine_index)"; do
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
for rel in \
    "src/codegen/llvm_inventory_host_methods.c" \
    "src/codegen/transpiler_decl_method_view.c"; do
    if grep -Fq "case AST_ROLE_DECL" "$ROOT_DIR/$rel"; then
        fail "$rel must delegate hosted-method AST compatibility classification to host_decl_compat.c"
    fi
done
for term in \
    "kPgyHostDeclCompatTypes[]" \
    "pgy_host_decl_compat_types" \
    "pgy_host_decl_compat_is_type" \
    "pgy_host_decl_compat_name" \
    "pgy_host_decl_compat_uses_pointer_self" \
    "pgy_host_decl_compat_has_projection_ready_flag" \
    "kPgyHostDeclCompatConstructorDomainTypes[]" \
    "pgy_host_decl_compat_constructor_domain_types" \
    "PgyHostMethodCompatView" \
    "pgy_host_method_compat_view_from_decl" \
    "PgyHostSharedFieldsCompatView" \
    "pgy_host_shared_fields_compat_view_from_decl" \
    "case AST_CLASS_DECL" \
    "case AST_ENUM_DECL" \
    "case AST_PARTY_DECL" \
    "case AST_ROSTER_DECL" \
    "case AST_ROLE_DECL" \
    "case AST_WORLD_DECL" \
    "case AST_RELATION_DECL" \
    "case AST_EFFECT_DECL" \
    "case AST_ZONE_DECL"; do
    require_term "src/codegen/host_decl_compat.c" "$term"
done
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "pgy_host_shared_fields_compat_view_from_decl(host_decl)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_find_domain_constructor_decl"
require_term "src/codegen/llvm_domain_lookup.c" \
    "pgy_host_decl_compat_constructor_domain_types("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_find_domain_constructor_decl(ctx, callee_name)"
if grep -Fq "llvm_find_named_domain_decl(ctx, AST_PARTY_DECL, callee_name)" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM constructor dispatch must consume the domain-constructor lookup seam instead of repeating host chains"
fi
for term in \
    "transpiler_party_slot_first_ability_tag" \
    "transpiler_party_slot_method_ability_tag"; do
    require_term "src/codegen/transpiler_role_ability_helpers.h" "$term"
    require_term "src/codegen/transpiler_role_ability.c" "$term"
done
require_term "src/codegen/transpiler_statement_dispatch.c" \
    "transpiler_party_slot_first_ability_tag("
require_term "src/codegen/transpiler_expr_call_member_emit.c" \
    "transpiler_party_slot_method_ability_tag("
for rel in \
    "src/codegen/transpiler_statement_dispatch.c" \
    "src/codegen/transpiler_expr_call_member_emit.c"; do
    if grep -Fq "ast_party_role_count(" "$ROOT_DIR/$rel"; then
        fail "$rel must consume party-slot ability helpers instead of repeating role-slot scans"
    fi
done
for rel in \
    "src/codegen/llvm_expr_domain_query_calls.c" \
    "src/codegen/transpiler_expr_domain_query_builtin.c"; do
    require_term "$rel" "pgy_host_decl_compat_has_projection_ready_flag(host_decl)"
    if grep -Fq "host_decl->type == AST_RELATION_DECL" "$ROOT_DIR/$rel"; then
        fail "$rel must consume host projection-ready policy instead of repeating relation/effect/zone chains"
    fi
done
for term in \
    "kPgyHostDeclCompatNominalLookupTypes[]" \
    "pgy_host_decl_compat_nominal_lookup_types"; do
    require_term "src/codegen/host_decl_compat.c" "$term"
done
if grep -Fq "kTranspilerNominalHostLookupTypes" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C nominal host lookup must consume host_decl_compat.c lookup order"
fi
for term in \
    "return ast_role_name(decl)" \
    "return ast_party_name(decl)" \
    "return ast_roster_name(decl)"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c"; then
        fail "C host declaration names must delegate to host_decl_compat.c"
    fi
done
for term in \
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
    require_term "src/codegen/transpiler_func_forward_metadata.c" "$term"
done
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_hosted_method_view_source_ast(&method_view, i)"
    require_term "$rel" "transpiler_hosted_method_view_routine(ctx, &method_view, i)"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, base_class_name"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_source_ast(&method_view, i)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_routine(ctx,"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "emit_hosted_method_forward_decl_from_metadata"
if grep -Eq 'class_decl->data\.class_decl\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    fail "generic class specialization must consume TranspilerHostedMethodView, not index AST method arrays"
fi
for term in \
    "TranspilerHostedMethodView" \
    "ast_compat_methods" \
    "ast_compat_count" \
    "transpiler_hosted_method_view(" \
    "transpiler_hosted_method_view_metadata(" \
    "transpiler_mir_decl_method_name(" \
    "transpiler_mir_decl_method_source_ast(" \
    "transpiler_mir_decl_method_routine(" \
    "transpiler_hosted_method_view_routine(" \
    "transpiler_hosted_method_view_from_decl(" \
    "transpiler_hosted_method_view_source_ast(" \
    "transpiler_hosted_method_view_missing_mir_metadata("; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
require_term "src/codegen/transpiler_context.h" \
    "transpiler_set_mir_inventory_missing"
require_term "src/codegen/transpiler_context.h" \
    "transpiler_set_mir_topology_invalid"
require_term "src/codegen/transpiler_context.h" \
    "transpiler_set_mir_intent_carrier_missing"
for term in \
    "transpiler_set_mir_inventory_missing" \
    "transpiler_set_mir_topology_invalid" \
    "transpiler_set_mir_intent_carrier_missing" \
    "PGY_CODE_MIR_TOPOLOGY_INVALID" \
    "PGY_CODE_MIR_INTENT_CARRIER_MISSING" \
    "PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING" \
    "PGY_CAUSE_MIR_TOPOLOGY_INVALID" \
    "PGY_CAUSE_MIR_INTENT_CARRIER_MISSING" \
    "PGY_FIX_CHECK_INTENT_STEP_LOWERING" \
    "PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING"; do
    require_term "src/codegen/transpiler_context.c" "$term"
done
if grep -RIn "PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING" "$ROOT_DIR/src/codegen" \
    | grep -v "src/codegen/transpiler_context.c"; then
    fail "C backend MIR-missing diagnostics must route through transpiler_set_mir_inventory_missing"
fi
for term in \
    "view->count != view->ast_compat_count" \
    "if (view->requires_mir_metadata)"; do
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
if grep -Fq "transpiler_hosted_method_view(" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c" \
    && grep -Fq "NULL, 0)" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must preserve AST compatibility counts when a MIR header exists"
fi
if grep -Fq "fallback_methods" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must name AST compatibility paths explicitly, not as fallback_methods"
fi
if grep -Fq "fallback_count" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must name AST compatibility counts explicitly, not as fallback_count"
fi
for rel in \
    "src/codegen/transpiler_domain_nominal_emit.c" \
    "src/codegen/transpiler_world_select_event_emit.c"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_hosted_method_view_source_ast(&method_view, i)"
    require_term "$rel" "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_zone_methods_emit.c" \
    "transpiler_hosted_method_view_source_ast(method_view, i)"
require_term "src/codegen/transpiler_zone_methods_emit.c" \
    "emit_hosted_method_forward_decl_from_metadata"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "MIR-only C path missing method declaration metadata for party"
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "MIR-only C path missing method declaration metadata for roster"
for term in \
    "MIR-only C path missing method declaration metadata for relation" \
    "MIR-only C path missing method declaration metadata for effect"; do
    require_term "src/codegen/transpiler_relation_effect_emit.c" "$term"
done
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "MIR-only C path missing method declaration metadata for zone"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "MIR-only C path missing method declaration metadata for world"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "MIR-only C path missing method declaration metadata for generic class"
if grep -RIn "emit_hosted_method_forward_decl_named" "$ROOT_DIR/src/codegen"; then
    fail "C hosted method forward declarations must use MIRDeclMethod metadata-first helper"
fi
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "const TranspilerHostedMethodView *method_view"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_hosted_method_view_metadata(method_view, i)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
if grep -Eq 'emit_hosted_methods_from_mir_or_error_local\([^)]*ASTNode \*\*methods|emit_hosted_methods_from_mir_or_error_local\([^)]*size_t method_count' \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h"; then
    fail "hosted method body emission must accept TranspilerHostedMethodView, not AST method arrays"
fi
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, name"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, ename"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(method_view)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_intent_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_intent_emit_metadata_helpers.h" \
    "transpiler_set_mir_intent_carrier_missing("
require_term "src/codegen/transpiler_intent_cleanup_emit.c" \
    "transpiler_set_mir_intent_carrier_missing("
if grep -RIn "PGY_CODE_MIR_INTENT_CARRIER_MISSING" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit_metadata_helpers.h" \
    "$ROOT_DIR/src/codegen/transpiler_intent_cleanup_emit.c"; then
    fail "C intent carrier diagnostics must use transpiler_set_mir_intent_carrier_missing"
fi
require_term "src/codegen/transpiler_mir_emit_state.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_mir_func_emit.c" \
    "transpiler_set_mir_topology_invalid("
require_term "src/codegen/transpiler_mir_terminator_emit.c" \
    "transpiler_set_mir_topology_invalid("
require_term "src/codegen/transpiler_mir_reason_classifier.h" \
    "transpiler_classify_mir_function_reason"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "PGY_CODE_MIR_UNRESOLVED_LOCAL"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "PGY_CODE_MIR_SIGNATURE_UNSUPPORTED"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "PGY_CODE_MIR_SSA_LIMIT"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "kMirFunctionReasonPatterns"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "\"MIR contract invalid\""
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "\"no matching MIR routine\""
require_term "src/codegen/transpiler_func_class_flow_emit.c" \
    "transpiler_classify_mir_function_reason(reason)"
if grep -n "strstr(reason" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"; then
    fail "C function emitter must not classify MIR reason strings inline"
fi
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c" \
    "src/codegen/transpiler_generic_class_specialization_emit.c"; do
    if grep -Fq "transpiler_find_mir_method(ctx" "$ROOT_DIR/$rel"; then
        fail "$rel must use TranspilerHostedMethodView routine metadata, not a secondary method lookup"
    fi
done
if sed -n '1,110p' "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c" \
    | grep -Fq "transpiler_find_mir_method(ctx"; then
    fail "hosted role/domain method emission must use MIRDeclMethod routine metadata, not a secondary method lookup"
fi
if grep -RIn "transpiler_find_mir_method" "$ROOT_DIR/src/codegen"; then
    fail "generic C method lookup helper name must not reappear; remaining role include seam is explicit"
fi
require_term "src/codegen/transpiler_mir_role_lookup.c" \
    "transpiler_find_role_impl_mir_method"
c_method_raw_hits="$(
    c_method_files=()
    for path in "$ROOT_DIR"/src/codegen/transpiler*.c \
        "$ROOT_DIR"/src/codegen/transpiler*.h; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        if [[ "$rel" == "src/codegen/transpiler_decl_lookup.h" ||
              "$rel" == "src/codegen/transpiler_decl_method_view.c" ]]; then
            continue
        fi
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
        if [[ "$rel" == src/codegen/llvm* ||
              "$rel" == "src/codegen/transpiler.h" ||
              "$rel" == "src/codegen/transpiler_inventory_view.h" ||
              "$rel" == "src/codegen/transpiler_inventory_view.c" ||
              "$rel" == "src/codegen/transpiler_decl_lookup.h" ||
              "$rel" == "src/codegen/transpiler_decl_method_view.c" ]]; then
            continue
        fi
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

if grep -Fq "decl_header->source_ast == decl" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM host method inventory must be metadata-first; do not require source_ast identity"
fi
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "llvm_active_has_mir(ctx)"
if grep -Fq "ctx->mir" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM host method inventory must use active MIR view helpers, not direct ctx->mir probes"
fi

if grep -RIn "llvm_find_mir_method_routine_local" "$ROOT_DIR/src/codegen"; then
    fail "LLVM AST/name-based MIR method routine compatibility helper must not reappear"
fi

for term in \
    "return llvm_is_host_decl_type(decl->type)" \
    "llvm_decl_node_name(decl)" \
    "llvm_find_host_decl_header_in_context(ctx, type_name)" \
    "llvm_find_host_decl_in_active_inventory(ctx, type_name)" \
    "llvm_host_decl_uses_pointer_self"; do
    require_term "src/codegen/llvm_domain_lookup.c" "$term"
done
for term in \
    "pgy_host_decl_compat_types(&host_type_count)" \
    "if (ctx->mir != NULL)" \
    "host_types[i]" \
    "llvm_find_decl_in_active_inventory("; do
    require_term "src/codegen/llvm_inventory_decl_lookup.c" "$term"
done
if grep -Eq 'llvm_find_decl_in_active_inventory\([^,]+,[[:space:]]*AST_(CLASS|ENUM|RELATION|EFFECT|ZONE|WORLD)_DECL' \
    "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"; then
    fail "LLVM host-decl fallback must iterate host_decl_compat.c, not a partial hard-coded host chain"
fi
if grep -Fq "llvm_decl_current_nominal_name" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"; then
    fail "LLVM implicit self lowering must use the shared current host-name helper"
fi
if grep -Fq "routine->ast == method->source_ast" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
    fail "MIRDeclMethod routine linking must not use AST identity matching"
fi
for term in \
    "mir_decl_next_capacity" \
    "mir_decl_header_set_role_impl_methods" \
    "ast_role_impl_method_total_count" \
    "SIZE_MAX / sizeof(MIRDeclMethod)" \
    "case AST_ROLE_DECL"; do
    require_term "src/compiler/mir_decl_headers.c" "$term"
done
for term in \
    "hir->role_count" \
    "mir_record_decl_header(mir, hir->roles[i])"; do
    require_term "src/compiler/mir.c" "$term"
done
if grep -Fq "routine->ast == method_decl" \
    "$ROOT_DIR/src/codegen/transpiler_mir_role_lookup.c"; then
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
    "llvm_mir_decl_method_source_ast(method_meta)" \
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
    "llvm_hosted_method_view_missing_mir_metadata(methods)" \
    "MIR-only LLVM path missing method forward metadata for domain" \
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
role_method_forward_body="$(
    awk '
        /llvm_emit_role_method_forward_decls_metadata_first/ { in_body = 1 }
        /llvm_emit_domain_role_forward_decls\(LLVMGenCtx \*ctx,/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
)"
for term in \
    "llvm_hosted_method_view_missing_mir_metadata(methods)" \
    "MIR-only LLVM path missing method forward metadata for role" \
    "llvm_hosted_method_view_metadata(methods, j)" \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_return_type_metadata_first"; do
    grep -Fq "$term" <<<"$role_method_forward_body" ||
        fail "LLVM role method forward declarations must be MIRDeclMethod metadata-first: missing $term"
done
if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' \
    <<<"$role_method_forward_body"; then
    fail "LLVM role method forward declarations must not read AST method param_count/return_type directly"
fi
ability_vtable_body="$(
    awk '
        /llvm_emit_domain_ability_vtables\(LLVMGenCtx \*ctx,/ { in_body = 1 }
        /#endif/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward_ability.c"
)"
role_operator_body="$(
    awk '
        /llvm_emit_role_operator_forward_decl/ { in_body = 1 }
        /llvm_emit_domain_role_forward_decls\(LLVMGenCtx \*ctx,/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
)"
for body_name in ability_vtable_body role_operator_body; do
    body="${!body_name}"
    for term in \
        "llvm_domain_method_param_count_metadata_first" \
        "llvm_domain_method_param_metadata_first" \
        "llvm_domain_method_return_type_metadata_first"; do
        grep -Fq "$term" <<<"$body" ||
            fail "LLVM ${body_name} must route method signature reads through the shared method accessors: missing $term"
    done
    if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' <<<"$body"; then
        fail "LLVM ${body_name} must not read AST method param_count/return_type directly"
    fi
done
require_term "src/codegen/llvm_domain_forward.h" \
    "const LLVMHostedMethodView *methods"
require_term "src/codegen/llvm_domain_forward.c" \
    "llvm_hosted_method_view_metadata(methods, j)"
require_term "src/codegen/llvm_domain_forward_role.c" \
    "llvm_emit_role_method_forward_decls_metadata_first"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "LLVMHostedMethodView method_view"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "llvm_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "MIR-only LLVM path missing method declaration metadata for domain"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "llvm_hosted_method_view_metadata(&method_view, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "LLVMHostedMethodView method_view"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing method declaration metadata for role"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_hosted_method_view_metadata(&method_view, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_mir_decl_method_routine(ctx, method_meta)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_role_method_name_from_ast"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_role_for_type_name"
require_term "src/codegen/llvm_domain_forward_role.c" \
    "llvm_role_for_type_node(stmt)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_role_for_type_name(stmt)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing registered function for role method"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing vtable function for role method"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "ctx != NULL && ctx->backend_error != NULL"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "ast_include_role_name(include_stmt)"
require_term "src/codegen/transpiler_operator.c" \
    "ast_include_role_name(include_stmt)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "ast_include_role_name(inc)"
require_term "src/codegen/transpiler_operator.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ast_impl_ability_ref(impl)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "ast_impl_ability_name(impl)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/compiler/mir_decl_headers.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/parser/ast_domain_api.h" \
    "ast_role_impl_method_total_count"
require_term "src/parser/ast_role_type_accessors.c" \
    "ast_role_impl_method_total_count"
require_term "src/compiler/mir_decl_header_validate.c" \
    "ast_role_impl_method_total_count"
if grep -R "data\.include_stmt" "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "C/LLVM codegen include payload consumers must use AST include accessors"
fi
if grep -R "data\.impl_ability" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c" >/dev/null; then
    fail "MIR role impl method metadata must use AST impl-ability accessors"
fi
if grep -R "data\.role_decl\.\(for_type\|includes\|include_count\|impl_abilities\|impl_count\)" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c" \
    "$ROOT_DIR/src/codegen/transpiler_operator.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" >/dev/null; then
    fail "MIR/C/LLVM role inventory paths must use AST role accessors"
fi
if grep -R "data\.ability_decl\.\(methods\|method_count\)" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c" >/dev/null; then
    fail "C/LLVM ability method inventory paths must use AST ability accessors"
fi
if grep -R "data\.impl_ability" \
    "$ROOT_DIR/src/codegen/transpiler_operator.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c" >/dev/null; then
    fail "C/LLVM role emission compatibility paths must use AST impl-ability accessors"
fi
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ctx != NULL && ctx->backend_error != NULL"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ensure_ability_ref_vtable_decl(ability_ref, ctx)"
require_term "src/codegen/transpiler_decl_host_lookup.c" \
    "transpiler_role_subject_type_name_local"
require_term "src/codegen/transpiler_operator.c" \
    "transpiler_role_subject_type_name_local(role)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "transpiler_role_subject_type_node_local(role)"
if grep -Fq "llvm_find_mir_method_routine_local(ctx," \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role method body emission must use linked MIRDeclMethod routine indexes, not AST/name routine search"
fi
if grep -Fq "llvm_find_mir_method_routine_local(ctx," \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"; then
    fail "LLVM hosted domain method body emission must use linked MIRDeclMethod routine indexes, not AST/name routine search"
fi
if grep -Eq 'role_decl\.for_type' \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role operator emission must read role target type through llvm_domain_role_lookup helpers"
fi
if grep -Eq 'role_decl\.for_type' \
    "$ROOT_DIR/src/codegen/transpiler_operator.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role operator emission must read role target type through transpiler decl-host helpers"
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
        if [[ "$rel" == "src/codegen/llvm_inventory_host_methods.c" ||
              "$rel" == "src/codegen/llvm_inventory_host_methods.h" ||
              "$rel" == "src/codegen/llvm_domain_decl_parts_helpers.h" ]]; then
            continue
        fi
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
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c" \
        "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
        fail "LLVM MIR method accessors must not fall back to AST method nodes: $forbidden"
    fi
done

require_term "src/codegen/llvm_inventory_host_methods.c" \
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
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_public_surface.c" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_decl_headers.h" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
        fail "MIR declaration method metadata missing term: $term"
    fi
done
for term in \
    "mir_validate_decl_header_ast_compat" \
    "mir_validate_decl_header_metadata" \
    "ast_role_impl_method_total_count" \
    "AST method-count compatibility drift" \
    "name metadata drift" \
    "duplicates declaration header" \
    "pointer-self ABI metadata drift" \
    "method metadata count" \
    "signature metadata drift" \
    "routine index"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
if grep -Fq "header->ast_type != AST_ROLE_DECL" \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c"; then
    fail "MIR declaration header validation must not keep role method-count exceptions"
fi
require_term "src/compiler/mir_program_validate.c" \
    "mir_validate_decl_header_metadata(mir, error_message)"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects hosted method signature metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects declaration header name metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR declaration headers preserve pointer-self ABI shape"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects pointer-self ABI metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects duplicate declaration header names"

if awk '/decl = decl_header->source_ast;/{exit} {print}' \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c" |
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
    "src/codegen/llvm_inventory_internal.c"
    "src/codegen/llvm_inventory_internal.h"
    "src/codegen/llvm_inventory_decl_lookup.c"
    "src/codegen/llvm_inventory_decl_lookup.h"
    "src/codegen/llvm_inventory_host_methods.c"
    "src/codegen/llvm_inventory_host_methods.h"
    "src/codegen/transpiler_inventory_view.c"
    "src/codegen/transpiler_inventory_view.h"
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
