/*
 * Copyright (c) 2026 Pergyra Language Project
 * Effective MIR local type rendering for the C backend.
 */

#include "transpiler_mir_effective_type.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

#include "transpiler_decl_lookup.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_type_render.h"

char *
transpiler_render_effective_local_type_name(TranspilerCtx *ctx,
                                            ASTNode *type_node)
{
    if (type_node != NULL
        && type_node->type == AST_TYPE
        && ast_type_name(type_node) != NULL) {
        ASTNode *class_decl = find_class_decl(ctx, ast_type_name(type_node));
        if (class_decl != NULL && transpiler_class_has_generic_params(class_decl)) {
            const char *spec_name =
                ensure_generic_class_specialization(ctx, class_decl, type_node);
            if (spec_name != NULL
                && strcmp(spec_name, ast_type_name(type_node)) != 0) {
                return pergyra_strdup(spec_name);
            }
        }
    }
    return render_type_name_in_ctx(ctx, type_node);
}
