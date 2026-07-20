#ifndef PGY_TRANSPILER_CLASS_DECL_EMIT_H
#define PGY_TRANSPILER_CLASS_DECL_EMIT_H

#include "../parser/ast.h"
#include "transpiler.h"

void emit_class_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_class_decl_from_mir_header(const MIRDeclHeader *header,
                                     TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_CLASS_DECL_EMIT_H */
