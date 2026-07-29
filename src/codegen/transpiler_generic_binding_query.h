#ifndef PGY_TRANSPILER_GENERIC_BINDING_QUERY_H
#define PGY_TRANSPILER_GENERIC_BINDING_QUERY_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

typedef struct TranspilerGenericBindingSnapshot {
    int binding_count;
} TranspilerGenericBindingSnapshot;

TranspilerGenericBindingSnapshot transpiler_generic_binding_snapshot(
    TranspilerCtx *ctx);
void transpiler_generic_binding_restore(
    TranspilerCtx *ctx,
    TranspilerGenericBindingSnapshot snapshot);
bool transpiler_generic_binding_push_entries(
    TranspilerCtx *ctx,
    const GenericBindingEntry *bindings,
    size_t binding_count);
bool transpiler_infer_generic_call_bindings(TranspilerCtx *ctx,
                                            ASTNode *decl,
                                            ASTNode *call,
                                            GenericBindingEntry *bindings,
                                            size_t *binding_count);
char *transpiler_render_type_name_with_bindings(
    TranspilerCtx *ctx,
    ASTNode *type_node,
    GenericBindingEntry *bindings,
    size_t binding_count);

#endif /* PGY_TRANSPILER_GENERIC_BINDING_QUERY_H */
