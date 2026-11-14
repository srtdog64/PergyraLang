/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR local variable lookup owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_vars.h"

LLVMMirVar *
llvm_mir_get_var_entry(LLVMMirVar *vars, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (vars[i].mir_name && strcmp(vars[i].mir_name, name) == 0)
            return &vars[i];
    }
    return NULL;
}

LLVMValueRef
llvm_mir_get_var(LLVMMirVar *vars, size_t count, const char *name)
{
    LLVMMirVar *entry = llvm_mir_get_var_entry(vars, count, name);
    return entry != NULL ? entry->alloca : NULL;
}

#endif /* PGY_LLVM_ENABLED */
