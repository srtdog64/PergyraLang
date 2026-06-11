#include "mir_type_helpers.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

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

static char *
mir_render_tuple_type_name(ASTNode *type_node)
{
    size_t element_count;
    char *result;

    if (type_node == NULL)
        return NULL;
    element_count = ast_type_tuple_element_count(type_node);
    if (element_count == 0)
        return NULL;

    result = pergyra_strdup("(");
    if (result == NULL)
        return NULL;
    for (size_t i = 0; i < element_count; i++) {
        char *inner = mir_render_type_name(ast_type_tuple_element(type_node, i));
        if (inner == NULL) {
            free(result);
            return NULL;
        }
        if ((i > 0 && !mir_type_append_owned(&result, ","))
            || !mir_type_append_owned(&result, inner)) {
            free(inner);
            free(result);
            return NULL;
        }
        free(inner);
    }
    if (!mir_type_append_owned(&result, ")")) {
        free(result);
        return NULL;
    }
    return result;
}

static bool
mir_type_node_is_slot_like(const ASTNode *type_node)
{
    const char *name;

    if (type_node == NULL)
        return false;
    name = ast_type_name(type_node);
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
        || ast_call_callee(expr) == NULL
        || ast_call_callee(expr)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_call_callee(expr)) == NULL)
        return false;

    callee = ast_identifier_name(ast_call_callee(expr));
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
        for (size_t i = 0; i < ast_func_param_count(func_decl); i++) {
            FuncParam *param = ast_func_param(func_decl, i);
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
        const char *prior_name;
        if (prior == NULL || prior->type != AST_LET_DECL
            || ast_let_name(prior) == NULL)
            continue;
        prior_name = ast_let_name(prior);
        if (strcmp(prior_name, binding_name) != 0)
            continue;
        if (mir_type_node_is_slot_like(ast_let_type(prior)))
            return true;
        if (mir_expr_is_claim_like(ast_let_initializer(prior)))
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
        || ast_assignment_target(stmt) == NULL
        || ast_assignment_target(stmt)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_assignment_target(stmt)) == NULL)
        return false;

    target_name = ast_identifier_name(ast_assignment_target(stmt));
    return mir_binding_name_is_slot_like(func_decl,
                                         statements,
                                         statement_count,
                                         stmt_index,
                                         target_name);
}

char *
mir_render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return NULL;
    if (type_node->type == AST_EVENT_HANDLER_TYPE)
        return NULL;
    if (type_node->type == AST_TYPE
        && ast_type_tuple_element_count(type_node) > 0)
        return mir_render_tuple_type_name(type_node);
    if (type_node->type == AST_TYPE) {
        GenericParams *generic_args = ast_type_generic_args(type_node);
        const char *type_name = ast_type_name(type_node);
        char *result = NULL;
        size_t generic_count = ast_generic_param_count(generic_args);
        if (type_name == NULL)
            return NULL;
        result = pergyra_strdup(type_name);
        if (result == NULL)
            return NULL;
        if (generic_count > 0) {
            if (!mir_type_append_owned(&result, "<")) {
                free(result);
                return NULL;
            }
            for (size_t i = 0; i < generic_count; i++) {
                GenericParam *param = ast_generic_param_at(generic_args, i);
                char *inner = mir_render_type_name(
                    ast_generic_param_constraint(param));
                if (inner == NULL) {
                    free(result);
                    return NULL;
                }
                if ((i > 0 && !mir_type_append_owned(&result, ","))
                    || !mir_type_append_owned(&result, inner)) {
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
        char *inner = mir_render_type_name(ast_channel_type_element_type(type_node));
        char *result = NULL;
        if (inner != NULL)
            result = mir_type_strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }
    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = mir_render_type_name(ast_future_type_value_type(type_node));
        char *result = NULL;
        if (inner != NULL)
            result = mir_type_strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }
    return NULL;
}

char *
mir_claim_abi_type_name_from_ast(const ASTNode *ast)
{
    if (ast == NULL)
        return NULL;
    if (ast->type == AST_WITH_STMT) {
        char *inner = mir_render_type_name(ast_with_slot_type(ast));
        char *result = NULL;
        if (inner != NULL) {
            result = mir_type_strdup_fmt("%s<%s>",
                              ast_with_is_secure(ast) ? "SecureSlot" : "Slot",
                              inner);
        }
        free(inner);
        return result;
    }
    if (ast->type == AST_CALL
        && ast_call_callee(ast) != NULL
        && ast_call_callee(ast)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(ast)) != NULL) {
        const char *callee = ast_identifier_name(ast_call_callee(ast));
        const MIRClaimKindSpec *claim = mir_claim_kind_from_callee(callee);
        char *inner = NULL;
        char *result = NULL;

        if (claim == NULL)
            return NULL;
        if (ast_call_generic_arg_count(ast) >= 1) {
            GenericParam *type_arg = ast_call_generic_arg(ast, 0);
            inner = mir_render_type_name(ast_generic_param_constraint(type_arg));
        } else if (ast_call_arg_count(ast) >= 1
                   && ast_call_argument(ast, 0) != NULL) {
            inner = mir_render_type_name(ast_call_argument(ast, 0));
        }
        if (inner != NULL)
            result = mir_type_strdup_fmt("%s<%s>", claim->abi_prefix, inner);
        free(inner);
        return result;
    }
    return NULL;
}
