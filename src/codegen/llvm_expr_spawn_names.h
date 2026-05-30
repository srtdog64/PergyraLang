#ifndef PGY_LLVM_EXPR_SPAWN_NAMES_H
#define PGY_LLVM_EXPR_SPAWN_NAMES_H

#include "llvm_internal.h"

bool llvm_spawn_copy_name(LLVMGenCtx *ctx,
                          ASTNode *node,
                          char *out,
                          size_t out_size,
                          const char *name,
                          const char *label);
bool llvm_spawn_format_name(LLVMGenCtx *ctx,
                            ASTNode *node,
                            char *out,
                            size_t out_size,
                            const char *prefix,
                            const char *suffix,
                            const char *label);
bool llvm_spawn_wrapper_name(LLVMGenCtx *ctx,
                             ASTNode *node,
                             char *out,
                             size_t out_size,
                             int wrapper_id);
void llvm_spawn_append_mangled_suffix(char *buf,
                                      size_t buf_size,
                                      const char *suffix);

#endif
