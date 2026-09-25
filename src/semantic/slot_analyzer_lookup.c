/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer function declaration lookup.
 */

#include "slot_analyzer_internal.h"
#include "type_checker_internal.h"

ASTNode *
slot_analyzer_find_function_decl(const SlotFunctionLookup *lookup,
                                 const char *name)
{
    if (lookup == NULL || lookup->ctx == NULL || name == NULL)
        return NULL;
    return semantic_host_index_find_decl_by_name(
        lookup->ctx, AST_FUNC_DECL, name);
}
