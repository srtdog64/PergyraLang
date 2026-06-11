#include "transpiler_call_constructor_result_emit.h"

#include <string.h>

#include "../parser/ast.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_constructor_emit.h"
#include "transpiler_enum.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_generic_param_query.h"

char *
emit_call_domain_constructor(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (callee->type != AST_IDENTIFIER)
        return NULL;

    const char *fn = ast_identifier_name(callee);
    ASTNode *class_decl = find_class_decl(ctx, fn);
    if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
        const char *ctor_type = fn;
        if (transpiler_class_has_generic_params(class_decl)) {
            const char *type_hint = ctx != NULL && ctx->expected_type != NULL
                ? ctx->expected_type
                : (ctx != NULL ? ctx->active_type_hint : NULL);
            ASTNode *hint_base = type_hint != NULL
                ? transpiler_generic_class_spec_base_decl(ctx, type_hint)
                : NULL;
            const char *hint_base_name = hint_base != NULL
                ? transpiler_decl_name_local(hint_base)
                : NULL;
            if (hint_base_name != NULL && fn != NULL
                && strcmp(hint_base_name, fn) == 0) {
                ctor_type = type_hint;
            } else {
                ASTNode *synthetic_type = ast_create_type(fn);
                if (synthetic_type != NULL) {
                    const char *spec_name =
                        ensure_generic_class_specialization(
                            ctx, class_decl, synthetic_type);
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
        if (decl != NULL)
            return transpiler_emit_domain_constructor_for_decl(
                call, decl, fn, ctx);
    }

    {
        char qualified[128];
        if (lookup_enum_variant_qualified_name_copy(
                ctx, fn, qualified, sizeof(qualified))) {
            return transpiler_emit_enum_variant_constructor(call, qualified, ctx);
        }
    }

    return NULL;
}
