#ifndef PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler_context.h"

char *render_effective_ability_ref_vtable_tag(ASTNode *ability_decl,
                                              ASTNode *ability_ref,
                                              TranspilerCtx *ctx);
bool ability_ref_vtable_typedef_name(ASTNode *ability_ref,
                                     char *buf,
                                     size_t buf_size,
                                     TranspilerCtx *ctx);
void ensure_ability_ref_vtable_decl(ASTNode *ability_ref, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H */
