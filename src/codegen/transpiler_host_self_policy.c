/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend host self-cell classification policy.
 */

#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_projection.h"

bool
is_pointer_self_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl;

    if (type_name == NULL)
        return false;
    if (is_subject_type_name(ctx, type_name))
        return true;
    decl = find_class_decl(ctx, type_name);
    if (decl != NULL
        && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL)
        return true;
    if (find_party_decl(ctx, type_name) != NULL
        || find_role_decl(ctx, type_name) != NULL
        || find_roster_decl(ctx, type_name) != NULL)
        return true;
    return find_relation_decl(ctx, type_name) != NULL
        || find_effect_decl(ctx, type_name) != NULL
        || find_zone_decl(ctx, type_name) != NULL
        || find_world_decl(ctx, type_name) != NULL;
}
