#ifndef PGY_TRANSPILER_ZONE_DECL_EMIT_INTERNAL_H
#define PGY_TRANSPILER_ZONE_DECL_EMIT_INTERNAL_H

#include "transpiler_zone_decl_emit.h"

void transpiler_emit_zone_decl_impl(ASTNode *node,
                                    const MIRDeclHeader *header,
                                    const char *name,
                                    TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_ZONE_DECL_EMIT_INTERNAL_H */
