#ifndef PERGYRA_TYPE_CHECKER_RESOLUTION_METADATA_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_RESOLUTION_METADATA_INTERNAL_H

#include "type_checker_internal.h"

Type *semantic_type_resolution_lookup_resolved_type(SemanticContext *ctx,
                                                    ASTNode *type_node);
bool metadata_index_insert(SemanticContext *ctx, void *key, size_t entry_index);
bool metadata_ensure_index_capacity(SemanticContext *ctx, size_t next_count);
bool metadata_lookup_entry_index(SemanticContext *ctx, ASTNode *type_node,
                                 size_t *out_index);

#endif /* PERGYRA_TYPE_CHECKER_RESOLUTION_METADATA_INTERNAL_H */
