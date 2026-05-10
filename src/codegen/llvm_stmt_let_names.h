#ifndef PGY_LLVM_STMT_LET_NAMES_H
#define PGY_LLVM_STMT_LET_NAMES_H

#include "llvm_internal.h"

bool llvm_stmt_require_let_type_arg(LLVMGenCtx *ctx,
                                    ASTNode *node,
                                    const char *binding_name,
                                    const char *container_name);
bool llvm_let_with_token_name(LLVMGenCtx *ctx,
                              ASTNode *node,
                              char *out,
                              size_t out_size,
                              const char *binding_name);
bool llvm_let_with_slot_write_name(LLVMGenCtx *ctx,
                                   ASTNode *node,
                                   char *out,
                                   size_t out_size,
                                   const char *inner_type,
                                   bool is_secure);

#endif
