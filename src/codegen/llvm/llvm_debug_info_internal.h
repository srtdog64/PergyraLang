/* LLVM source debug location declarations. */
#ifndef PGY_LLVM_DEBUG_INFO_INTERNAL_H
#define PGY_LLVM_DEBUG_INFO_INTERNAL_H

void llvm_debug_begin_function(LLVMGenCtx *ctx, const char *name,
                               LLVMValueRef fn, unsigned line);
void llvm_debug_set_line(LLVMGenCtx *ctx, unsigned line);

#endif
