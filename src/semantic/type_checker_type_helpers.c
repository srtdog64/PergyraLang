#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
assignable_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

bool
require_assignable(Type *from, Type *to, const ASTNode *site,
                   SemanticContext *ctx)
{
    from = assignable_normalize_type(from);
    to = assignable_normalize_type(to);

    if (type_is_assignable(from, to))
        return true;

    if (type_is_slot_handle(to) && type_slot_inner_type(to) != NULL
        && type_is_assignable(from, type_slot_inner_type(to))) {
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
