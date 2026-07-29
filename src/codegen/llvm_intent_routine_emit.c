/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM intent routine-inventory body emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_internal.h"

#include <string.h>

static bool
llvm_mir_intent_routine_has_instructions(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (routine->blocks[i].instruction_count > 0)
            return true;
    }
    return false;
}

void
llvm_emit_intent_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory)
{
    ASTNode **intent_decls = NULL;
    size_t intent_decl_count = 0;

    if (ctx == NULL || inventory == NULL)
        return;

    llvm_active_inventory(ctx, AST_INTENT_DECL,
        &intent_decls, &intent_decl_count);
    for (size_t i = 0; i < inventory->count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(inventory, i);
        const char *routine_name;
        ASTNode *intent_decl = NULL;
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid intent routine inventory row");
            return;
        }
        if (llvm_mir_routine_kind(routine) != MIR_SCOPE_INTENT)
            continue;
        routine_name = llvm_mir_routine_name(routine);
        for (size_t j = 0; j < intent_decl_count; j++) {
            ASTNode *candidate = intent_decls != NULL ? intent_decls[j] : NULL;
            const char *candidate_name =
                candidate != NULL && candidate->type == AST_INTENT_DECL
                    ? ast_intent_decl_name(candidate)
                    : NULL;
            if (routine_name != NULL && candidate_name != NULL
                && strcmp(candidate_name, routine_name) == 0) {
                intent_decl = candidate;
                break;
            }
        }
        if (intent_decl == NULL) {
            if (llvm_mir_intent_routine_has_instructions(routine)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing intent declaration inventory row for routine '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return;
            }
            continue;
        }
        llvm_emit_intent_decl(intent_decl, ctx);
        if (ctx->has_error)
            return;
    }
}

#endif /* PGY_LLVM_ENABLED */
