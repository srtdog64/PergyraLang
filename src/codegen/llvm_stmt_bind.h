#ifndef PGY_LLVM_STMT_BIND_H
#define PGY_LLVM_STMT_BIND_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>

#include "llvm_internal.h"

bool llvm_emit_bind_statement_parts(LLVMGenCtx *ctx,
                                    const char *party_var,
                                    const char *slot_name,
                                    const char *role_name,
                                    ASTNode *diagnostic_node);

#endif

#endif
