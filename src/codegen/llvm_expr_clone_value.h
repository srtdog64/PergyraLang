#ifndef PGY_LLVM_EXPR_CLONE_VALUE_H
#define PGY_LLVM_EXPR_CLONE_VALUE_H

#include "llvm_internal.h"

/* `Clone` of `value`, a value of Pergyra type `type_name`. Every String,
 * Array, Option payload and non-generic struct field it owns is copied into
 * new storage (2026-10-19_struct_clone_is_deep); any other part is copied by
 * value. The checker's result relation treats only fully copied types as
 * independent of the argument. NULL after a backend error. */
LLVMValueRef llvm_emit_clone_value(ASTNode *node, LLVMGenCtx *ctx,
                                   const char *type_name, LLVMValueRef value);

#endif
