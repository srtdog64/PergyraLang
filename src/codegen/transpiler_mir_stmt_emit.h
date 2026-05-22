#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_STMT_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_STMT_EMIT_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

bool transpiler_mir_stmt_is_mirrored_resource(TranspilerCtx *ctx,
                                              const MIRBasicBlock *block,
                                              ASTNode *stmt);

bool transpiler_emit_mir_call_statement(CodeBuf *buf,
                                        const MIRBasicBlock *block,
                                        ASTNode *stmt,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map,
                                        char *reason,
                                        size_t reason_cap);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_STMT_EMIT_H */
