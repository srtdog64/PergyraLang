#ifndef PGY_LLVM_EXPR_CALL_INLINE_POLICY_H
#define PGY_LLVM_EXPR_CALL_INLINE_POLICY_H

#include <stddef.h>

typedef enum LLVMCallInlineOp {
    LLVM_CALL_INLINE_OP_NONE = 0,
    LLVM_CALL_INLINE_OP_CLONE,
    LLVM_CALL_INLINE_OP_TO_OBJECT,
} LLVMCallInlineOp;

LLVMCallInlineOp llvm_call_inline_lookup(const char *callee_name, size_t argc);

#endif
