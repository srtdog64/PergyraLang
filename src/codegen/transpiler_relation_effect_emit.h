#ifndef PGY_TRANSPILER_RELATION_EFFECT_EMIT_H
#define PGY_TRANSPILER_RELATION_EFFECT_EMIT_H

#include "transpiler_context.h"

void emit_relation_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_effect_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_relation_decl_from_mir_header(const MIRDeclHeader *header,
                                        TranspilerCtx *ctx);
void emit_effect_decl_from_mir_header(const MIRDeclHeader *header,
                                      TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_RELATION_EFFECT_EMIT_H */
