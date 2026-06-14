#ifndef PGY_LLVM_BACKEND_GENERIC_H
#define PGY_LLVM_BACKEND_GENERIC_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

const LLVMGenericTemplate *llvm_lookup_generic_template_entry(
    LLVMGenCtx *ctx,
    const char *name);
ASTNode *llvm_lookup_generic_template(LLVMGenCtx *ctx, const char *name);
bool llvm_register_generic_template_decl(LLVMGenCtx *ctx,
                                         ASTNode *func_decl);
bool llvm_register_generic_template_routine(LLVMGenCtx *ctx,
                                            const MIRRoutine *routine);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_BACKEND_GENERIC_H */
