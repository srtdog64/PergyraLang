#ifndef PGY_LLVM_EXPR_STDLIB_SCALAR_IO_CALLS_H
#define PGY_LLVM_EXPR_STDLIB_SCALAR_IO_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_stdlib_string_file_call(ASTNode *node,
                                       LLVMGenCtx *ctx,
                                       const char *callee_name,
                                       LLVMValueRef *out_result);
bool llvm_emit_stdlib_runtime_io_call(ASTNode *node,
                                      LLVMGenCtx *ctx,
                                      const char *callee_name,
                                      LLVMValueRef *out_result);

#endif
