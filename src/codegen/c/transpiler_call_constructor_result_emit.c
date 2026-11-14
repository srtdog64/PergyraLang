#include "transpiler_call_constructor_result_emit.h"

#include <string.h>

#include "../../parser/ast.h"
#include "../../parser/ast_api.h"
#include "../../semantic/diag_codes.h"
#include "../codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_constructor_emit.h"
#include "transpiler_enum.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_specialization_registry.h"

/* The semantic call target, not a same-name inventory entry, owns the
 * constructor kind. The identifier binding is checked when it survives a
 * source projection; MIR-only calls carry the selected call target instead. */
static bool
constructor_selected_decl_matches(ASTNode *call, ASTNode *callee,
                                  ASTNode *decl)
{
    uint32_t selected_id = ast_call_semantic_callee_decl_id(call);
    uint32_t binding_id = ast_identifier_binding_syntax_id(callee);
    return decl != NULL && selected_id != 0
        && selected_id == ast_node_stable_id(decl)
        && (binding_id == 0 || binding_id == selected_id)
        && !ast_call_semantic_callee_declared_callable(call)
        && !ast_call_semantic_callee_is_stdlib(call)
        && ast_call_semantic_callee_value_binding_id(call) == 0;
}

char *
emit_call_domain_constructor(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (callee->type != AST_IDENTIFIER)
        return NULL;

    const char *fn = ast_identifier_name(callee);
    if (ast_call_semantic_callee_is_stdlib(call)
        && ast_call_semantic_callee_decl_id(call) == 0)
        return NULL;
    if (pgy_codegen_match_variant_is_builtin(
            pgy_codegen_match_variant_lookup(fn)))
        return NULL;
    ASTNode *class_decl = find_class_decl(ctx, fn);
    if (class_decl != NULL && class_decl->type == AST_CLASS_DECL
        && constructor_selected_decl_matches(call, callee, class_decl)) {
        const char *ctor_type = fn;
        if (transpiler_class_has_generic_params(class_decl)) {
            const char *type_hint = ctx != NULL && ctx->expected_type != NULL
                ? ctx->expected_type
                : (ctx != NULL ? ctx->active_type_hint : NULL);
            const char *hint_spec_name =
                transpiler_ensure_generic_class_specialization_from_type_name(
                    ctx, ctx != NULL ? ctx->decls : NULL, type_hint);
            ASTNode *hint_base = type_hint != NULL
                ? transpiler_generic_class_spec_base_decl(ctx, type_hint)
                : NULL;
            if (hint_base == NULL && hint_spec_name != NULL) {
                hint_base = transpiler_generic_class_spec_base_decl(
                    ctx, hint_spec_name);
            }
            const char *hint_base_name = hint_base != NULL
                ? transpiler_decl_name_local(hint_base)
                : NULL;
            if (hint_base_name != NULL && fn != NULL
                && strcmp(hint_base_name, fn) == 0) {
                ctor_type = hint_spec_name != NULL ? hint_spec_name : type_hint;
            } else {
                ASTNode *synthetic_type = ast_create_type(fn);
                if (synthetic_type != NULL) {
                    const char *spec_name =
                        ensure_generic_class_specialization(
                            ctx, ctx->decls, class_decl, synthetic_type);
                    if (spec_name != NULL)
                        ctor_type = spec_name;
                    ast_destroy(synthetic_type);
                }
            }
        }
        return transpiler_emit_class_constructor_with_type(
            call, class_decl, ctor_type, ctx);
    }

    {
        ASTNode *decl = transpiler_find_domain_constructor_decl_local(ctx, fn);
        if (decl != NULL
            && constructor_selected_decl_matches(call, callee, decl)) {
            return transpiler_emit_domain_constructor_for_decl(
                call, decl, fn, ctx);
        }
        if (decl != NULL)
            class_decl = decl;
    }

    {
        char qualified[128];
        if (lookup_enum_variant_qualified_name_copy(
                ctx, fn, qualified, sizeof(qualified))) {
            size_t qualified_len = strlen(qualified);
            size_t variant_len = strlen(fn);
            if (qualified_len <= variant_len + 1
                || qualified[qualified_len - variant_len - 1] != '_') {
                transpiler_set_mir_inventory_missing(ctx,
                    "C enum constructor has malformed qualified variant identity");
                return NULL;
            }
            qualified[qualified_len - variant_len - 1] = '\0';
            ASTNode *enum_decl = find_enum_decl(ctx, qualified);
            if (constructor_selected_decl_matches(call, callee, enum_decl)) {
                qualified[qualified_len - variant_len - 1] = '_';
                return transpiler_emit_enum_variant_constructor(
                    call, qualified, ctx);
            }
            class_decl = enum_decl != NULL ? enum_decl : class_decl;
        }
    }

    if (class_decl != NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: constructor call has no matching selected declaration");
    }
    return NULL;
}
