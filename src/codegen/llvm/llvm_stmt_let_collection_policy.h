#ifndef PGY_LLVM_STMT_LET_COLLECTION_POLICY_H
#define PGY_LLVM_STMT_LET_COLLECTION_POLICY_H

typedef enum LLVMStmtCollectionCtorOp {
    LLVM_STMT_COLLECTION_CTOR_NONE = 0,
    LLVM_STMT_COLLECTION_CTOR_HASH_MAP,
    LLVM_STMT_COLLECTION_CTOR_LIST,
    LLVM_STMT_COLLECTION_CTOR_QUEUE,
    LLVM_STMT_COLLECTION_CTOR_SET,
} LLVMStmtCollectionCtorOp;

typedef struct LLVMStmtCollectionCtorSpec {
    const char *callee_name;
    const char *annotation_name;
    const char *runtime_fn;
    LLVMStmtCollectionCtorOp op;
} LLVMStmtCollectionCtorSpec;

typedef enum LLVMStmtLetCallOp {
    LLVM_STMT_LET_CALL_NONE = 0,
    LLVM_STMT_LET_CALL_CHANNEL,
    LLVM_STMT_LET_CALL_TO_OBJECT,
} LLVMStmtLetCallOp;

const LLVMStmtCollectionCtorSpec *
llvm_stmt_collection_ctor_lookup(const char *callee_name);
LLVMStmtLetCallOp llvm_stmt_let_call_lookup(const char *callee_name);

#endif
