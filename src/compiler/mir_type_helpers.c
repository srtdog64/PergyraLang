#include "mir_type_helpers.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static char *
mir_type_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    int written;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    written = vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != length) {
        free(result);
        return NULL;
    }
    return result;
}

static bool
mir_type_append_owned(char **dst, const char *suffix)
{
    size_t dst_len;
    size_t suffix_len;
    char *grown;

    if (dst == NULL || *dst == NULL)
        return false;
    if (suffix == NULL)
        suffix = "";

    dst_len = strlen(*dst);
    suffix_len = strlen(suffix);
    if (suffix_len > ((size_t)-1) - dst_len - 1)
        return false;

    grown = realloc(*dst, dst_len + suffix_len + 1);
    if (grown == NULL)
        return false;
    memcpy(grown + dst_len, suffix, suffix_len + 1);
    *dst = grown;
    return true;
}

static bool
mir_type_node_is_slot_like(const ASTNode *type_node)
{
    const char *name;

    if (type_node == NULL || type_node->type != AST_TYPE)
        return false;
    name = type_node->data.type.name;
    if (name == NULL)
        return false;
    return strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strncmp(name, "Slot<", 5) == 0
        || strncmp(name, "SecureSlot<", 11) == 0
        || strncmp(name, "DeviceSlot<", 11) == 0;
}

typedef struct MIRClaimKindSpec {
    const char *callee;
    const char *abi_prefix;
} MIRClaimKindSpec;

static const MIRClaimKindSpec *
mir_claim_kind_from_callee(const char *callee)
{
    static const MIRClaimKindSpec specs[] = {
        {"ClaimSlot", "Slot"},
        {"ClaimSecureSlot", "SecureSlot"},
        {"ClaimDeviceSlot", "DeviceSlot"},
    };

    if (callee == NULL)
        return NULL;
    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        if (strcmp(callee, specs[i].callee) == 0)
            return &specs[i];
    }
    return NULL;
}

static bool
mir_expr_is_claim_like(const ASTNode *expr)
{
    const char *callee;

    if (expr == NULL || expr->type != AST_CALL
        || expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER
        || expr->data.call.callee->data.identifier.name == NULL)
        return false;

    callee = expr->data.call.callee->data.identifier.name;
    return mir_claim_kind_from_callee(callee) != NULL;
}

static bool
mir_binding_name_is_slot_like(const ASTNode *func_decl,
                              ASTNode **statements,
                              size_t statement_count,
                              size_t stmt_index,
                              const char *binding_name)
{
    if (binding_name == NULL || binding_name[0] == '\0')
        return false;

    if (func_decl != NULL && func_decl->type == AST_FUNC_DECL) {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *param = func_decl->data.func_decl.params[i];
            if (param == NULL || param->name == NULL)
                continue;
            if (strcmp(param->name, binding_name) != 0)
                continue;
            return mir_type_node_is_slot_like(param->type);
        }
    }

    if (statements == NULL || statement_count == 0)
        return false;
    if (stmt_index > statement_count)
        stmt_index = statement_count;

    for (size_t i = 0; i < stmt_index; i++) {
        ASTNode *prior = statements[i];
        if (prior == NULL || prior->type != AST_LET_DECL
            || prior->data.let_decl.name == NULL)
            continue;
        if (strcmp(prior->data.let_decl.name, binding_name) != 0)
            continue;
        if (mir_type_node_is_slot_like(prior->data.let_decl.type))
            return true;
        if (mir_expr_is_claim_like(prior->data.let_decl.initializer))
            return true;
    }

    return false;
}

bool
mir_assignment_requires_stmt_preservation(const ASTNode *func_decl,
                                          ASTNode **statements,
                                          size_t statement_count,
                                          size_t stmt_index,
                                          const ASTNode *stmt)
{
    const char *target_name;

    if (stmt == NULL || stmt->type != AST_ASSIGNMENT
        || stmt->data.assignment.target == NULL
        || stmt->data.assignment.target->type != AST_IDENTIFIER
        || stmt->data.assignment.target->data.identifier.name == NULL)
        return false;

    target_name = stmt->data.assignment.target->data.identifier.name;
    return mir_binding_name_is_slot_like(func_decl,
                                         statements,
                                         statement_count,
                                         stmt_index,
                                         target_name);
}

static char *
mir_render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");
    if (type_node->type == AST_TYPE) {
        char *result = pergyra_strdup(type_node->data.type.name != NULL
            ? type_node->data.type.name : "Int");
        if (result == NULL)
            return NULL;
        if (type_node->data.type.generic_args != NULL
            && type_node->data.type.generic_args->count > 0) {
            if (!mir_type_append_owned(&result, "<")) {
                free(result);
                return NULL;
            }
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *param = type_node->data.type.generic_args->params[i];
                char *inner = mir_render_type_name(
                    param != NULL ? param->constraint : NULL);
                if ((i > 0 && !mir_type_append_owned(&result, ","))
                    || !mir_type_append_owned(&result, inner != NULL ? inner : "Int")) {
                    free(inner);
                    free(result);
                    return NULL;
                }
                free(inner);
            }
            if (!mir_type_append_owned(&result, ">")) {
                free(result);
                return NULL;
            }
        }
        return result;
    }
    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = mir_render_type_name(type_node->data.channel_type.element_type);
        char *result = NULL;
        if (inner != NULL)
            result = mir_type_strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result != NULL ? result : pergyra_strdup("Channel<Int>");
    }
    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = mir_render_type_name(type_node->data.future_type.value_type);
        char *result = NULL;
        if (inner != NULL)
            result = mir_type_strdup_fmt("Future<%s>", inner);
        free(inner);
        return result != NULL ? result : pergyra_strdup("Future<Int>");
    }
    return pergyra_strdup("Int");
}

char *
mir_claim_abi_type_name_from_ast(const ASTNode *ast)
{
    if (ast == NULL)
        return NULL;
    if (ast->type == AST_WITH_STMT) {
        char *inner = mir_render_type_name(ast->data.with_stmt.slot_type);
        char *result = mir_type_strdup_fmt("%s<%s>",
                                  ast->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                                  inner != NULL ? inner : "Int");
        free(inner);
        return result;
    }
    if (ast->type == AST_CALL
        && ast->data.call.callee != NULL
        && ast->data.call.callee->type == AST_IDENTIFIER
        && ast->data.call.callee->data.identifier.name != NULL) {
        const char *callee = ast->data.call.callee->data.identifier.name;
        if (ast->data.call.arg_count >= 1 && ast->data.call.arguments[0] != NULL) {
            const MIRClaimKindSpec *claim = mir_claim_kind_from_callee(callee);
            char *inner = mir_render_type_name(ast->data.call.arguments[0]);
            char *result = NULL;
            if (claim != NULL) {
                result = mir_type_strdup_fmt("%s<%s>",
                                             claim->abi_prefix,
                                             inner != NULL ? inner : "Int");
            }
            free(inner);
            return result;
        }
    }
    return NULL;
}
