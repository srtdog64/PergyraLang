#ifndef PGY_LLVM_MIR_LOCAL_EXPECTED_TYPE_H
#define PGY_LLVM_MIR_LOCAL_EXPECTED_TYPE_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../compiler/mir.h"
#include "../parser/ast.h"

const char *llvm_mir_local_expected_type_name(const MIRRoutine *routine,
                                              const MIRInstruction *inst,
                                              const char *base_name);

LLVMTypeRef llvm_mir_local_infer_expr_type(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           const MIRInstruction *inst,
                                           const char *base_name,
                                           ASTNode *expr);

#endif

#endif
