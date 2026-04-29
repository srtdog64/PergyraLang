#include "type_checker_internal.h"
#include "diag_codes.h"

bool
require_assignable(Type *from, Type *to, const ASTNode *site,
                   SemanticContext *ctx)
{
    if (type_is_assignable(from, to))
        return true;

    if (to->kind == TYPE_KIND_SLOT && to->data.slot.inner_type != NULL
        && type_is_assignable(from, to->data.slot.inner_type)) {
        return true;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_ASSIGNABILITY_CHECK,
        PGY_FIX_ANNOTATE_OR_CONVERT,
        site,
        "Type mismatch: cannot assign '%s' to '%s'",
        from->name, to->name);
    return false;
}

Type *
wrap_constructed(Type *constructor, Type *inner)
{
    Type *args[1] = { inner };
    return type_create_constructed(constructor, args, 1);
}
