#ifndef PGY_LLVM_STMT_PARALLEL_NAMES_H
#define PGY_LLVM_STMT_PARALLEL_NAMES_H

#include "llvm_internal.h"

bool llvm_parallel_counter_name(LLVMGenCtx *ctx,
                                char *out,
                                size_t out_size,
                                const char *prefix,
                                int counter);
bool llvm_parallel_task_name(LLVMGenCtx *ctx,
                             char *out,
                             size_t out_size,
                             int counter,
                             size_t index);
bool llvm_async_wrapper_name(LLVMGenCtx *ctx,
                             char *out,
                             size_t out_size,
                             int counter);
bool llvm_select_channel_runtime_name(LLVMGenCtx *ctx,
                                      char *out,
                                      size_t out_size,
                                      const char *prefix,
                                      const char *inner);

#endif
