#include <stdlib.h>

#include "type_checker_internal.h"

Type **
collect_effective_generic_arg_types(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count)
{
    size_t effective_count = 0;
    ASTNode **effective_nodes;
    Type **effective_types;

    if (out_count != NULL)
        *out_count = 0;

    effective_nodes = collect_effective_generic_arg_nodes(
        decl_params,
        provided_args,
        site,
        ctx,
        owner_kind,
        owner_name,
        &effective_count);
    if (effective_nodes == NULL)
        return NULL;

    effective_types = calloc(effective_count > 0 ? effective_count : 1,
                             sizeof(Type *));
    if (effective_types == NULL) {
        free(effective_nodes);
        return NULL;
    }

    for (size_t i = 0; i < effective_count; i++) {
        Type *resolved = domain_resolve_type_ref(effective_nodes[i], ctx);
        if (resolved == NULL || resolved == TYPE_UNKNOWN) {
            resolved = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, effective_nodes[i]);
        }
        effective_types[i] = resolved != NULL ? resolved : TYPE_UNKNOWN;
    }

    free(effective_nodes);
    if (out_count != NULL)
        *out_count = effective_count;
    return effective_types;
}
