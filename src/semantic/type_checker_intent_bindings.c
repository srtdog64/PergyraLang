#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

ASTNode *
find_intent_involves_local(ASTNode *intent, const char *alias)
{
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && ast_intent_involves_alias(involves) != NULL
            && strcmp(ast_intent_involves_alias(involves), alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

ASTNode *
find_intent_value_local(ASTNode *intent, const char *alias)
{
    ASTNode **values;
    size_t value_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    values = ast_intent_decl_values(intent, &value_count);
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        if (value != NULL && value->type == AST_INTENT_VALUE
            && ast_intent_value_alias(value) != NULL
            && strcmp(ast_intent_value_alias(value), alias) == 0) {
            return value;
        }
    }
    return NULL;
}

static ASTNode *
find_unique_intent_involves_by_type_name(ASTNode *intent,
                                         const char *type_name,
                                         const char **alias_out)
{
    ASTNode *matched = NULL;
    ASTNode **involves_nodes;
    size_t involve_count;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent == NULL || intent->type != AST_INTENT_DECL || type_name == NULL)
        return NULL;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        const char *participant_type_name = intent_involves_type_name(involves);

        if (participant_type_name == NULL
            || strcmp(participant_type_name, type_name) != 0) {
            continue;
        }

        if (matched != NULL)
            return NULL;
        matched = involves;
    }

    if (matched != NULL && alias_out != NULL)
        *alias_out = ast_intent_involves_alias(matched);
    return matched;
}

ASTNode *
intent_step_resolve_transfer_target_involves(ASTNode *intent_decl,
                                             ASTNode *step,
                                             const char **resolved_alias_out)
{
    const char *to_name;
    ASTNode *to_involves;
    const char *resolved_alias = NULL;

    if (resolved_alias_out != NULL)
        *resolved_alias_out = NULL;
    if (intent_decl == NULL || step == NULL || step->type != AST_INTENT_STEP)
        return NULL;

    to_name = ast_intent_step_transfer_to_alias(step);
    if (to_name == NULL)
        return NULL;

    to_involves = find_intent_involves_local(intent_decl, to_name);
    if (to_involves == NULL) {
        to_involves = find_unique_intent_involves_by_type_name(
            intent_decl, to_name, &resolved_alias);
        if (to_involves != NULL && resolved_alias != NULL) {
            (void)ast_intent_step_replace_transfer_to_alias_copy(
                step, resolved_alias);
        }
    } else {
        resolved_alias = to_name;
    }

    if (resolved_alias_out != NULL)
        *resolved_alias_out = resolved_alias;
    return to_involves;
}

bool
type_check_intent_update_existing_signature(ASTNode *intent,
                                            Symbol *existing,
                                            SemanticContext *ctx)
{
    const char *name = ast_intent_decl_name(intent);
    ASTNode **bindings = NULL;
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    size_t binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    size_t param_count;
    Type **param_types;
    Type *function_type;

    if (intent == NULL || existing == NULL || existing->kind != SYMBOL_INTENT)
        return true;

    bindings = ast_intent_decl_bindings(intent, &binding_count);
    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    values = ast_intent_decl_values(intent, &value_count);
    param_count = binding_count > 0 ? binding_count
        : (involve_count + value_count);
    param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
    if (param_types == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_UNKNOWN_TYPE,
            PGY_CAUSE_RESOLUTION_OOM,
            PGY_FIX_REDUCE_SCOPE_OR_RETRY,
            intent,
            "Could not update duplicate intent signature for '%s'.\n"
            "Reason:\n"
            "- semantic type-resolution metadata allocation failed while rebuilding the intent parameter list\n"
            "Fix:\n"
            "- reduce this compilation unit size and retry\n"
            "- or report the input if this happens on a small program",
            name != NULL ? name : "<intent>");
        return false;
    }

    for (size_t i = 0; i < param_count; i++) {
        ASTNode *binding = binding_count > 0
            ? bindings[i]
            : (i < involve_count ? involves_nodes[i]
                : values[i - involve_count]);
        if (binding != NULL && binding->type == AST_INTENT_INVOLVES
            && ast_intent_involves_subject_type(binding) != NULL) {
            param_types[i] = intent_resolve_involves_type(binding, ctx);
        } else if (binding != NULL && binding->type == AST_INTENT_VALUE
            && ast_intent_value_type(binding) != NULL) {
            param_types[i] = intent_resolve_value_type(binding, ctx);
        } else {
            param_types[i] = TYPE_UNKNOWN;
        }
    }

    Type *intent_result = ast_intent_decl_has_typed_result(intent)
        ? intent_resolve_type_ref(ast_intent_decl_return_type(intent), ctx)
        : TYPE_BOOL;
    function_type = type_create_function(param_types, param_count,
                                         intent_result);
    free(param_types);
    if (function_type != NULL)
        existing->type = function_type;
    return true;
}

void
type_check_intent_resolve_binding_types(ASTNode *intent, SemanticContext *ctx)
{
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    size_t involve_count = 0;
    size_t value_count = 0;

    if (intent == NULL || intent->type != AST_INTENT_DECL)
        return;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    values = ast_intent_decl_values(intent, &value_count);

    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        (void)intent_resolve_involves_type(involves, ctx);
    }
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        (void)intent_resolve_value_type(value, ctx);
    }
}

void
type_check_intent_declare_binding_symbols(ASTNode *intent,
                                          SemanticContext *ctx)
{
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    size_t involve_count = 0;
    size_t value_count = 0;

    if (intent == NULL || intent->type != AST_INTENT_DECL || ctx == NULL)
        return;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    values = ast_intent_decl_values(intent, &value_count);

    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        Type *subject_type;
        Symbol *participant_sym;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;

        subject_type = intent_resolve_involves_type(involves, ctx);
        participant_sym = symbol_create_variable(ast_intent_involves_alias(involves),
            subject_type, involves->line, involves->column);
        scope_declare(ctx->scope, participant_sym);
    }

    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        Type *value_type;
        Symbol *value_sym;

        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;

        value_type = intent_resolve_value_type(value, ctx);
        value_sym = symbol_create_variable(ast_intent_value_alias(value),
            value_type, value->line, value->column);
        scope_declare(ctx->scope, value_sym);
    }
}
