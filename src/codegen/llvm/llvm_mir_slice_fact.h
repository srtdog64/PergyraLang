#ifndef PERGYRA_LLVM_MIR_SLICE_FACT_H
#define PERGYRA_LLVM_MIR_SLICE_FACT_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

LLVMTypeRef llvm_mir_slice_fact_type_from_call(LLVMGenCtx *ctx,
                                               ASTNode *expr,
                                               LLVMMirVar *vars,
                                               size_t var_count);
LLVMTypeRef llvm_mir_slice_fact_elem_type_from_receiver(LLVMGenCtx *ctx,
                                                        ASTNode *receiver,
                                                        LLVMMirVar *vars,
                                                        size_t var_count);
LLVMTypeRef llvm_mir_slice_fact_array_type_from_slice_type(
    LLVMGenCtx *ctx,
    LLVMTypeRef slice_type);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_MIR_SLICE_FACT_H */
