#ifndef PGY_LLVM_EXPR_ASSIGNMENT_MEMBER_PROJECTION_H
#define PGY_LLVM_EXPR_ASSIGNMENT_MEMBER_PROJECTION_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_assignment_parts(ASTNode *diagnostic_anchor,
                                        ASTNode *target,
                                        ASTNode *value,
                                        LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_mir_assignment_parts(const MIRRoutine *routine,
                                            const MIRInstruction *inst,
                                            ASTNode *diagnostic_anchor,
                                            ASTNode *target,
                                            ASTNode *value,
                                            LLVMGenCtx *ctx);

#endif
