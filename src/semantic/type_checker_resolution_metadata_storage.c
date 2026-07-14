#include <stdlib.h>

#include "type_checker_internal.h"

/*
 * The metadata table used to free the Types it had flagged as `owned`. That
 * was a second, partial owner: it ran at semantic_context_destroy -- before
 * HIR/MIR/codegen had finished reading types -- and covered only the subset
 * that happened to be routed through this table. Type ownership is now whole
 * and lives in the per-analysis registry (type_alloc / type_registry_*), so
 * this table borrows and frees nothing. Two owners for one allocation is a
 * double free, which is exactly what the sanitizer gate reported the moment
 * the registry landed.
 *
 * The `owned` flags stay: they still record which entries the table
 * synthesized rather than borrowed, and callers read them.
 */
void
semantic_type_resolution_free_metadata(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    free(ctx->type_resolution_metadata.keys);
    free(ctx->type_resolution_metadata.values);
    free(ctx->type_resolution_metadata.owned);
    free(ctx->type_resolution_metadata.index_keys);
    free(ctx->type_resolution_metadata.index_entries);
    ctx->type_resolution_metadata.keys = NULL;
    ctx->type_resolution_metadata.values = NULL;
    ctx->type_resolution_metadata.owned = NULL;
    ctx->type_resolution_metadata.index_keys = NULL;
    ctx->type_resolution_metadata.index_entries = NULL;
    ctx->type_resolution_metadata.count = 0;
    ctx->type_resolution_metadata.capacity = 0;
    ctx->type_resolution_metadata.index_capacity = 0;
}
