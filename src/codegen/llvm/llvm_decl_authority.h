#ifndef PGY_LLVM_DECL_AUTHORITY_H
#define PGY_LLVM_DECL_AUTHORITY_H

#ifdef PGY_LLVM_ENABLED
typedef struct LLVMGenCtx LLVMGenCtx;

void llvm_decl_emit_zone_authority_check(LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_DECL_AUTHORITY_H */
