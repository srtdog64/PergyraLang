#ifndef PGY_TRANSPILER_DOMAIN_NOMINAL_EMIT_H
#define PGY_TRANSPILER_DOMAIN_NOMINAL_EMIT_H

#include "transpiler_context.h"

bool transpiler_domain_nominal_surface_desc(char *out,
                                            size_t out_size,
                                            const char *prefix,
                                            const char *owner_name,
                                            const char *member_name,
                                            const char *param_name);
void transpiler_domain_nominal_surface_desc_too_long(
    TranspilerCtx *ctx,
    const char *surface_kind);
void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_DOMAIN_NOMINAL_EMIT_H */
