#include "type_checker_internal.h"
#include "diag_codes.h"

/*
 * Legacy resolver audit counters.
 *
 * The recursive type-node evaluator has been removed from the
 * beta path. These counters intentionally remain exported so
 * `PGY_TYPE_RES_STATS=1` can keep reporting the retired compatibility surface
 * as 0 calls / 0 body fallbacks.
 */
size_t g_resolve_type_node_calls = 0;
size_t g_resolve_type_node_unique_nodes = 0;
size_t g_resolve_type_node_ast_type_calls = 0;
size_t g_resolve_type_node_channel_type_calls = 0;
size_t g_resolve_type_node_future_type_calls = 0;
size_t g_resolve_type_node_event_handler_type_calls = 0;
size_t g_resolve_type_node_other_ast_calls = 0;
size_t g_resolve_type_node_cache_hits = 0;
size_t g_resolve_type_node_cache_misses = 0;

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
