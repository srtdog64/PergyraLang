#ifndef PGY_LLVM_TYPE_KIND_H
#define PGY_LLVM_TYPE_KIND_H

#ifdef PGY_LLVM_ENABLED

#include <llvm-c/Core.h>

typedef struct LLVMGenCtx LLVMGenCtx;

typedef enum
{
    PGY_TK_INT,
    PGY_TK_LONG,
    PGY_TK_FLOAT,
    PGY_TK_DOUBLE,
    PGY_TK_BOOL,
    PGY_TK_STRING,
    PGY_TK_VOID,
    PGY_TK_QUBIT_SLOT,
    PGY_TK_REMOTE_FUTURE,
    PGY_TK_DEVICE_SLOT,
    PGY_TK_SLOT,
    PGY_TK_SECURE_SLOT,
    PGY_TK_RESULT,
    PGY_TK_OPTION,
    PGY_TK_CHANNEL,
    PGY_TK_FUTURE,
    PGY_TK_ALLOCATOR,
    PGY_TK_BOX,
    PGY_TK_RC,
    PGY_TK_WEAK,
    PGY_TK_ARRAY,
    PGY_TK_SLICE,
    PGY_TK_HASHMAP,
    PGY_TK_LIST,
    PGY_TK_QUEUE,
    PGY_TK_SET,
    PGY_TK_CLASS,
    PGY_TK_UNKNOWN
} PgyTypeKind;

PgyTypeKind pgy_classify_type(const char *type_name);
LLVMTypeRef pgy_kind_to_llvm(LLVMGenCtx *ctx, PgyTypeKind kind);
const char *pgy_kind_to_suffix(PgyTypeKind kind);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_TYPE_KIND_H */
