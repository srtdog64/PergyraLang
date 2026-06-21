/*
 * LLVM MIR local type lookup from already-owned MIR/local facts.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_type_lookup.h"

#include <string.h>

#include "llvm_mir_type_helpers.h"
#include "parser/ast_api.h"

LLVMTypeRef
llvm_mir_local_type_from_vars(LLVMMirVar *vars, size_t var_count,
                              const char *name)
{
    LLVMMirVar *entry;
    char base_name[128];

    if (vars == NULL || name == NULL)
        return NULL;

    entry = llvm_mir_get_var_entry(vars, var_count, name);
    if (entry != NULL)
        return entry->type;

    for (size_t i = var_count; i > 0; i--) {
        const char *mir_name = vars[i - 1].mir_name;
        if (mir_name == NULL)
            continue;
        if (!llvm_mir_base_name_from_versioned(mir_name, base_name,
                sizeof(base_name)))
            continue;
        if (strcmp(base_name, name) == 0)
            return vars[i - 1].type;
    }

    return NULL;
}

ASTNode *
llvm_mir_local_initializer_expr(ASTNode *expr)
{
    if (expr != NULL && expr->type == AST_LET_DECL)
        return ast_let_initializer(expr);
    return expr;
}

static bool
llvm_mir_value_expr_is_method_call(ASTNode *expr)
{
    ASTNode *callee;

    if (expr == NULL)
        return false;
    if (expr->type == AST_ARRAY_ACCESS)
        return true;
    if (expr->type != AST_CALL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL)
        return false;
    return callee->type == AST_MEMBER_ACCESS
        || callee->type == AST_IDENTIFIER;
}

LLVMTypeRef
llvm_mir_local_type_from_value_fact(const MIRInstruction *inst,
                                    LLVMMirVar *vars,
                                    size_t var_count)
{
    ASTNode *value_expr;

    if (inst == NULL)
        return NULL;
    value_expr = llvm_mir_local_initializer_expr(inst->expr0);
    if (llvm_mir_value_expr_is_method_call(value_expr))
        return NULL;
    if (inst->use_count > 0 && inst->uses != NULL) {
        bool value_is_binary = value_expr != NULL
            && value_expr->type == AST_BINARY;
        bool walk_all_uses = value_expr != NULL
            && (value_expr->type == AST_IDENTIFIER
                || value_expr->type == AST_ASSIGNMENT
                || value_is_binary);
        size_t walk_limit = walk_all_uses ? inst->use_count : 1;
        for (size_t ui = 0; ui < walk_limit; ui++) {
            LLVMTypeRef use_type =
                llvm_mir_local_type_from_vars(vars, var_count,
                    inst->uses[ui]);
            if (use_type == NULL)
                continue;
            if (value_is_binary
                && LLVMGetTypeKind(use_type) == LLVMStructTypeKind)
                continue;
            return use_type;
        }
    }

    if (value_expr != NULL
        && value_expr->type == AST_IDENTIFIER) {
        const char *value_name = ast_identifier_name(value_expr);
        if (value_name != NULL)
            return llvm_mir_local_type_from_vars(vars, var_count, value_name);
    }

    return NULL;
}

#endif
