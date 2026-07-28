#ifndef PGY_LLVM_INTENT_TYPED_EXECUTION_H
#define PGY_LLVM_INTENT_TYPED_EXECUTION_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

/* LLVM last consumer for an admitted MIR typed-intent execution plan. */
bool llvm_emit_typed_intent_execution(ASTNode *node,
                                      LLVMGenCtx *ctx,
                                      const MIRRoutine *routine,
                                      ASTNode *priority_expr,
                                      bool is_concurrent);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_INTENT_TYPED_EXECUTION_H */
