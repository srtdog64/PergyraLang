#ifndef PERGYRA_TYPE_CHECKER_RESOLUTION_HELPERS_H
#define PERGYRA_TYPE_CHECKER_RESOLUTION_HELPERS_H

#include "type_checker_internal.h"

/* Declaration-only compatibility seam for metadata-first resolution helpers.
 * Implementation lives in type_checker_resolution_helpers.c.
 */
Type *resolve_named_type(const char *name, SemanticContext *ctx,
                         const ASTNode *site);
ASTNode *find_type_alias_decl(ASTNode *program, const char *name);
bool name_looks_qualified(const char *name);
const char *semantic_symbol_kind_label(SymbolKind kind);
const char *type_name_or_unknown(const Type *type);
void semantic_format_function_signature(const Type *type, char *out,
                                        size_t out_cap);
void reject_if_embedded_world_zone_mutation(SemanticContext *ctx,
                                            ASTNode *site,
                                            ASTNode *target,
                                            const char *op_name);

#endif
