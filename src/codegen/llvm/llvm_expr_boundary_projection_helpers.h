#ifndef PGY_LLVM_EXPR_BOUNDARY_PROJECTION_HELPERS_H
#define PGY_LLVM_EXPR_BOUNDARY_PROJECTION_HELPERS_H

#include "llvm_internal.h"

/* Non-NULL entries in evaluated_values are already evaluated value operands.
 * They are consumed exactly once and cannot replace indirect storage. Ordinary
 * calls pass NULL; operator dispatch may supply a resolved primitive operand. */
LLVMValueRef *llvm_build_boundary_call_args(LLVMGenCtx *ctx, ASTNode *decl,
                                            const MIRRoutine *bound_routine,
                                            ASTNode **arg_nodes, size_t argc,
                              const LLVMValueRef *evaluated_values,
                                            unsigned *out_count);

#endif
