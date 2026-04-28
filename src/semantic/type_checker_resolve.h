#ifndef PERGYRA_TYPE_CHECKER_RESOLVE_H
#define PERGYRA_TYPE_CHECKER_RESOLVE_H

#include "type_checker_internal.h"

/* Declaration-only compatibility seam. The metadata-first compatibility
 * resolver implementation lives in type_checker_resolve.c.
 */
Type *resolve_type_node(ASTNode *type_node, SemanticContext *ctx);
bool require_assignable(Type *from, Type *to, const ASTNode *site,
                        SemanticContext *ctx);
Type *wrap_constructed(Type *constructor, Type *inner);

#endif
