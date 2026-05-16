#ifndef PGY_LLVM_EXPR_TASK_CHANNEL_POLICY_H
#define PGY_LLVM_EXPR_TASK_CHANNEL_POLICY_H

#include <stdbool.h>
#include <stddef.h>

typedef enum LLVMTaskChannelOp {
    LLVM_TASK_CHANNEL_OP_NONE = 0,
    LLVM_TASK_CHANNEL_OP_CLOSE,
    LLVM_TASK_CHANNEL_OP_RECV_TIMEOUT,
    LLVM_TASK_CHANNEL_OP_SEND_TIMEOUT,
    LLVM_TASK_CHANNEL_OP_SEND_TIMEOUT_STATUS,
    LLVM_TASK_CHANNEL_OP_TRY_RECV,
    LLVM_TASK_CHANNEL_OP_TRY_SEND,
    LLVM_TASK_CHANNEL_OP_TRY_SEND_STATUS,
} LLVMTaskChannelOp;

bool llvm_is_task_channel_builtin_name(const char *callee_name);
const char *llvm_channel_query_runtime_op(const char *callee_name);
LLVMTaskChannelOp llvm_task_channel_op_lookup(const char *callee_name,
                                              size_t argc);

#endif
