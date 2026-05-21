#ifndef PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H
#define PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H

#include "parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_constructor_emit.h"
#include "transpiler_enum.h"

static char *
emit_call_domain_constructor(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (callee->type != AST_IDENTIFIER)
        return NULL;

    const char *fn = ast_identifier_name(callee);
    ASTNode *class_decl = find_class_decl(ctx, fn);
    if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
        const char *ctor_type = fn;
        if (class_has_generic_params(class_decl)) {
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

#endif /* PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H */
