#ifndef PGY_LLVM_STMT_EMIT_SUPPORT_H
#define PGY_LLVM_STMT_EMIT_SUPPORT_H

#include "llvm_internal.h"

bool llvm_stmt_format_bind_name(LLVMGenCtx *ctx,
                                ASTNode *node,
                                char *out,
                                size_t out_size,
                                const char *prefix,
                                const char *suffix,
                                const char *label);
bool llvm_stmt_require_non_void_value(LLVMGenCtx *ctx,
                                      ASTNode *expr,
                                      const char *message);

#endif
