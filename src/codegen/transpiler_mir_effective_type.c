/*
 * Copyright (c) 2026 Pergyra Language Project
 * Effective MIR local type rendering for the C backend.
 */

#include "transpiler_mir_effective_type.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

#include "transpiler_decl_lookup.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_type_render.h"

char *
transpiler_canonical_effective_local_type_name(TranspilerCtx *ctx,
                                               const char *type_name)
{
    const char *current = type_name;

    if (type_name == NULL)
        return NULL;
    for (unsigned depth = 0; depth < 32 && current != NULL; depth++) {
        const char *target =
            transpiler_type_alias_target_type_name_from_headers(ctx, current);
        if (target == NULL || target[0] == '\0'
            || strcmp(target, current) == 0) {
            break;
        }
        current = target;
    }
    return pergyra_strdup(current != NULL ? current : type_name);
}

char *
transpiler_render_effective_local_type_name(TranspilerCtx *ctx,
                                            ASTNode *type_node)
{
    char *rendered;

    if (type_node != NULL
        && type_node->type == AST_TYPE
        && ast_type_name(type_node) != NULL) {
        char *canonical =
            transpiler_canonical_effective_local_type_name(
                ctx, ast_type_name(type_node));
        if (canonical != NULL
            && strcmp(canonical, ast_type_name(type_node)) != 0) {
            return canonical;
        }
        free(canonical);

        ASTNode *class_decl = find_class_decl(ctx, ast_type_name(type_node));
        if (class_decl != NULL && transpiler_class_has_generic_params(class_decl)) {
            const char *spec_name =
                ensure_generic_class_specialization(ctx, class_decl, type_node);
            if (spec_name != NULL
                && strcmp(spec_name, ast_type_name(type_node)) != 0) {
                return transpiler_canonical_effective_local_type_name(
                    ctx, spec_name);
            }
        }
    }

    rendered = render_type_name_in_ctx(ctx, type_node);
    if (rendered != NULL) {
        char *canonical =
            transpiler_canonical_effective_local_type_name(ctx, rendered);
        free(rendered);
        return canonical;
    }
    return NULL;
}
