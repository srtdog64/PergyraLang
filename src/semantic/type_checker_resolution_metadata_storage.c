#include <stdlib.h>

#include "type_checker_internal.h"

void
semantic_type_resolution_free_owned_type(Type *type)
{
    if (type == NULL)
        return;
    free(type->name);
    if (type->kind == TYPE_KIND_CONSTRUCTED)
        free(type->data.constructed.args);
    if (type->kind == TYPE_KIND_FUNCTION) {
        free(type->data.function.param_types);
        free(type->data.function.param_modes);
        free(type->data.function.param_escape_summary_masks);
    }
    if (type->kind == TYPE_KIND_TUPLE)
        free(type->data.tuple.elements);
    free(type);
}

void
semantic_type_resolution_free_metadata(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (ctx->type_resolution_metadata.owned[i]) {
            semantic_type_resolution_free_owned_type(
                (Type *)ctx->type_resolution_metadata.values[i]);
        }
    }
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
