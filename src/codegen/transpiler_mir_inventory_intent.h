#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_INVENTORY_INTENT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_INVENTORY_INTENT_H

#include "transpiler.h"

void emit_func_forward_decl_named(ASTNode *node,
                                  const char *emitted_name,
                                  CodeBuf *buf,
                                  TranspilerCtx *ctx);
void emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx);

#include "transpiler_mir_inventory_intent_collect.h"

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_INVENTORY_INTENT_H */
