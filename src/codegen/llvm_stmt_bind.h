#ifndef PGY_LLVM_STMT_BIND_H
#define PGY_LLVM_STMT_BIND_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>

#include "llvm_internal.h"

/* The slot stores the vtable of `role_name` and the address of `subject`,
 * which the role body reads as self (docs/206 section 1). */
bool llvm_emit_bind_statement_parts(LLVMGenCtx *ctx,
                                    const char *party_var,
                                    const char *slot_name,
                                    const ASTNode *subject,
                                    const char *role_name,
                                    ASTNode *diagnostic_node);

#endif

#endif
