#ifndef PGY_LLVM_EXPR_SPAWN_WORKER_BOUNDARY_H
#define PGY_LLVM_EXPR_SPAWN_WORKER_BOUNDARY_H

#include "llvm_internal.h"

bool llvm_spawn_reject_worker_storage_param(LLVMGenCtx *ctx, ASTNode *site,
                                            FuncParam *param,
                                            const char *param_type_name,
                                            size_t index,
                                            const char *callee_name);
bool llvm_spawn_reject_worker_storage_arg(LLVMGenCtx *ctx, ASTNode *site,
                                          ASTNode *arg, size_t index,
                                          const char *callee_name);

#endif
