#ifndef PERGYRA_LLVM_RUNTIME_INTERNAL_H
#define PERGYRA_LLVM_RUNTIME_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

void llvm_declare_runtime_raw_collections(LLVMGenCtx *ctx);
void llvm_declare_runtime_core_builtins(LLVMGenCtx *ctx);
void llvm_declare_runtime_channels(LLVMGenCtx *ctx);
void llvm_declare_runtime_secure_slots(LLVMGenCtx *ctx);
void llvm_declare_runtime_task_memory(LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_RUNTIME_INTERNAL_H */
