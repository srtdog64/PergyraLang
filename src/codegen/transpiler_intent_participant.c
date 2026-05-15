/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent participant classification helpers.
 */

#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_intent_participant.h"
#include "transpiler_projection.h"

const char *
intent_involves_type_name_local(ASTNode *involves)
{
    ASTNode *subject_type = ast_intent_involves_subject_type(involves);

    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || subject_type == NULL
        || subject_type->type != AST_TYPE) {
        return NULL;
    }
    return ast_type_name(subject_type);
}

bool
intent_involves_is_subject_participant(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_subject_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

bool
intent_involves_uses_pointer_self(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_pointer_self_host_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}
