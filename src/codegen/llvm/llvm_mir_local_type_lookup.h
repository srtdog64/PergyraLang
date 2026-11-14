#ifndef PGY_LLVM_MIR_LOCAL_TYPE_LOOKUP_H
#define PGY_LLVM_MIR_LOCAL_TYPE_LOOKUP_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

LLVMTypeRef llvm_mir_local_type_from_vars(LLVMMirVar *vars,
                                          size_t var_count,
                                          const char *name);
size_t llvm_mir_source_local_def_count(const MIRRoutine *routine,
                                       const char *base_name);
ASTNode *llvm_mir_local_initializer_expr(ASTNode *expr);
LLVMTypeRef llvm_mir_local_type_from_value_fact(const MIRInstruction *inst,
                                                LLVMMirVar *vars,
                                                size_t var_count);
LLVMTypeRef llvm_mir_local_array_access_type(const MIRRoutine *routine,
                                             LLVMGenCtx *ctx,
                                             ASTNode *expr);

#endif
