#include "mir_generic_method_specialization.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mir.h"
#include "mir_base_helpers.h"
#include "mir_decl_headers.h"
#include "mir_source_local_expr_types.h"
#include "mir_type_helpers.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

typedef struct
{
    MIRProgram *mir;
    const MIRRoutine *caller;
    size_t caller_index;
    char **error_message;
} MIRGenericMethodCaptureCtx;

static void
mir_generic_method_fact_clear(MIRGenericMethodSpecializationFact *fact)
{
    if (fact == NULL)
        return;
    free(fact->owner_name);
    free(fact->method_name);
    free(fact->specialized_name);
    for (size_t i = 0; i < fact->binding_count; i++) {
        free(fact->generic_param_names != NULL
            ? fact->generic_param_names[i] : NULL);
        free(fact->actual_type_names != NULL
            ? fact->actual_type_names[i] : NULL);
    }
    free(fact->generic_param_names);
    free(fact->actual_type_names);
    memset(fact, 0, sizeof(*fact));
}

void
mir_generic_method_specializations_clear(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->generic_method_specialization_count; i++)
        mir_generic_method_fact_clear(&mir->generic_method_specializations[i]);
    free(mir->generic_method_specializations);
    mir->generic_method_specializations = NULL;
    mir->generic_method_specialization_count = 0;
    mir->generic_method_specialization_capacity = 0;
}

static const MIRDeclHeader *
mir_generic_method_receiver_header(const MIRProgram *mir,
                                   const char *receiver_type_name)
{
    char base[128];
    const char *lt;
    size_t len;

    if (mir == NULL || receiver_type_name == NULL)
        return NULL;
    lt = strchr(receiver_type_name, '<');
    len = lt != NULL ? (size_t)(lt - receiver_type_name)
                     : strlen(receiver_type_name);
    if (len == 0 || len >= sizeof(base))
        return NULL;
    memcpy(base, receiver_type_name, len);
    base[len] = '\0';
    return mir_find_decl_header(mir, base);
}

static const MIRDeclMethod *
mir_generic_method_find(const MIRDeclHeader *header, const char *method_name)
{
    if (header == NULL || method_name == NULL)
        return NULL;
    for (size_t i = 0; i < mir_decl_header_method_count(header); i++) {
        const MIRDeclMethod *method = mir_decl_header_method(header, i);
        const char *name = mir_decl_method_name(method);
        if (name != NULL && strcmp(name, method_name) == 0)
            return method;
    }
    return NULL;
}

static bool
mir_generic_method_name_append(char **name, size_t *len, size_t *capacity,
                               char ch)
{
    char *grown;
    size_t next_capacity;

    if (name == NULL || len == NULL || capacity == NULL)
        return false;
    if (*len + 2 > *capacity) {
        next_capacity = *capacity > 0 ? *capacity * 2 : 64;
        if (next_capacity < *len + 2)
            next_capacity = *len + 2;
        grown = realloc(*name, next_capacity);
        if (grown == NULL)
            return false;
        *name = grown;
        *capacity = next_capacity;
    }
    (*name)[(*len)++] = ch;
    (*name)[*len] = '\0';
    return true;
}

static bool
mir_generic_method_name_append_text(char **name, size_t *len,
                                    size_t *capacity, const char *text)
{
    if (text == NULL)
        return false;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if (!mir_generic_method_name_append(name, len, capacity, (char)*p))
            return false;
    }
    return true;
}

static char *
mir_generic_method_specialized_name(const char *owner_name,
                                    const char *method_name,
                                    char *const *actual_type_names,
                                    size_t actual_count)
{
    char *name = NULL;
    size_t len = 0;
    size_t capacity = 0;

    if (!mir_generic_method_name_append_text(&name, &len, &capacity, owner_name)
        || !mir_generic_method_name_append(&name, &len, &capacity, '_')
        || !mir_generic_method_name_append_text(&name, &len, &capacity,
            method_name)) {
        free(name);
        return NULL;
    }
    for (size_t i = 0; i < actual_count; i++) {
        bool wrote = false;
        if (!mir_generic_method_name_append(&name, &len, &capacity, '_')) {
            free(name);
            return NULL;
        }
        for (const unsigned char *p =
                 (const unsigned char *)actual_type_names[i];
             p != NULL && *p != '\0'; p++) {
            if (isalnum(*p)) {
                if (!mir_generic_method_name_append(
                        &name, &len, &capacity, (char)*p)) {
                    free(name);
                    return NULL;
                }
                wrote = true;
            } else if (wrote && len > 0 && name[len - 1] != '_') {
                if (!mir_generic_method_name_append(
                        &name, &len, &capacity, '_')) {
                    free(name);
                    return NULL;
                }
            }
        }
        if (!wrote
            && !mir_generic_method_name_append_text(
                &name, &len, &capacity, "Type")) {
            free(name);
            return NULL;
        }
    }
    return name;
}

static char *
mir_generic_method_captured_return_type(
    const MIRGenericMethodCaptureCtx *ctx,
    ASTNode *expr)
{
    const MIRGenericMethodSpecializationFact *fact;
    const MIRRoutine *method_routine;

    if (ctx == NULL || ctx->mir == NULL || expr == NULL
        || expr->type != AST_CALL) {
        return NULL;
    }
    fact = mir_generic_method_specialization_for_call(
        ctx->mir, ast_node_stable_id(expr));
    if (fact == NULL || fact->method_routine_index >= ctx->mir->routine_count)
        return NULL;
    method_routine = &ctx->mir->routines[fact->method_routine_index];
    if (method_routine->return_type == NULL)
        return NULL;
    return mir_render_substituted_type_name(method_routine->return_type,
        fact->generic_param_names, fact->actual_type_names,
        fact->binding_count);
}

static bool
mir_generic_method_capture_actuals(
    MIRGenericMethodCaptureCtx *ctx,
    ASTNode *call,
    const MIRRoutine *method_routine,
    MIRGenericMethodSpecializationFact *fact)
{
    size_t explicit_count = ast_call_generic_arg_count(call);
    MIRSourceLocalTypeScratch scratch = {0};

    fact->binding_count = method_routine->generic_param_count;
    if (fact->binding_count == 0)
        return true;
    if (fact->binding_count > SIZE_MAX / sizeof(char *))
        return false;
    fact->generic_param_names = calloc(fact->binding_count, sizeof(char *));
    fact->actual_type_names = calloc(fact->binding_count, sizeof(char *));
    if (fact->generic_param_names == NULL || fact->actual_type_names == NULL)
        return false;

    if (explicit_count > 0 && explicit_count != fact->binding_count) {
        if (ctx->error_message != NULL)
            *ctx->error_message = mir_strdup_fmt(
                "MIR generic method call %u carries %zu actual types for %zu parameters",
                ast_node_stable_id(call), explicit_count, fact->binding_count);
        return false;
    }

    for (size_t i = 0; i < fact->binding_count; i++) {
        const char *formal = method_routine->generic_param_names[i];
        const char *inferred = NULL;
        char *captured_return = NULL;

        fact->generic_param_names[i] = pergyra_strdup(formal);
        if (explicit_count > 0) {
            GenericParam *actual = ast_call_generic_arg(call, i);
            fact->actual_type_names[i] = mir_capture_type_name(
                ast_generic_param_constraint(actual),
                ast_generic_param_name(actual));
        } else {
            size_t call_arg_index = 0;
            for (size_t p = 0; p < method_routine->param_count; p++) {
                FuncParam *param = method_routine->params[p];
                const char *param_type = method_routine->param_type_names[p];
                if (param != NULL && param->name != NULL
                    && strcmp(param->name, "self") == 0) {
                    continue;
                }
                if (param_type != NULL && formal != NULL
                    && strcmp(param_type, formal) == 0
                    && call_arg_index < ast_call_arg_count(call)) {
                    ASTNode *argument =
                        ast_call_argument(call, call_arg_index);
                    captured_return = mir_generic_method_captured_return_type(
                        ctx, argument);
                    inferred = captured_return;
                    if (inferred == NULL) {
                        inferred = mir_source_local_expr_type_name(
                            ctx->mir, ctx->caller, &scratch, argument);
                    }
                    break;
                }
                call_arg_index++;
            }
            fact->actual_type_names[i] = pergyra_strdup(inferred);
        }
        free(captured_return);
        if (fact->generic_param_names[i] == NULL
            || fact->actual_type_names[i] == NULL
            || fact->actual_type_names[i][0] == '\0') {
            if (ctx->error_message != NULL && *ctx->error_message == NULL)
                *ctx->error_message = mir_strdup_fmt(
                    "MIR generic method call %u cannot resolve parameter '%s'",
                    ast_node_stable_id(call),
                    formal != NULL ? formal : "(anonymous)");
            return false;
        }
    }
    return true;
}

static bool
mir_generic_method_append(MIRGenericMethodCaptureCtx *ctx,
                          ASTNode *call,
                          const MIRDeclHeader *header,
                          const MIRDeclMethod *method,
                          size_t method_routine_index)
{
    MIRGenericMethodSpecializationFact fact;
    MIRGenericMethodSpecializationFact *grown;
    const MIRRoutine *method_routine =
        &ctx->mir->routines[method_routine_index];
    size_t next_capacity;

    memset(&fact, 0, sizeof(fact));
    fact.source_call_syntax_id = ast_node_stable_id(call);
    fact.caller_routine_index = ctx->caller_index;
    fact.method_routine_index = method_routine_index;
    fact.owner_name = pergyra_strdup(mir_decl_header_name(header));
    fact.method_name = pergyra_strdup(mir_decl_method_name(method));
    if (fact.source_call_syntax_id == 0) {
        if (ctx->error_message != NULL && *ctx->error_message == NULL)
            *ctx->error_message = pergyra_strdup(
                "MIR generic method call is missing stable syntax identity");
        mir_generic_method_fact_clear(&fact);
        return false;
    }
    if (fact.owner_name == NULL || fact.method_name == NULL
        || !mir_generic_method_capture_actuals(ctx, call, method_routine,
            &fact)) {
        mir_generic_method_fact_clear(&fact);
        return false;
    }
    fact.specialized_name = mir_generic_method_specialized_name(
        fact.owner_name, fact.method_name, fact.actual_type_names,
        fact.binding_count);
    if (fact.specialized_name == NULL) {
        mir_generic_method_fact_clear(&fact);
        return false;
    }

    if (ctx->mir->generic_method_specialization_count
        == ctx->mir->generic_method_specialization_capacity) {
        next_capacity = ctx->mir->generic_method_specialization_capacity > 0
            ? ctx->mir->generic_method_specialization_capacity * 2 : 8;
        grown = realloc(ctx->mir->generic_method_specializations,
            next_capacity * sizeof(*grown));
        if (grown == NULL) {
            mir_generic_method_fact_clear(&fact);
            return false;
        }
        ctx->mir->generic_method_specializations = grown;
        ctx->mir->generic_method_specialization_capacity = next_capacity;
    }
    ctx->mir->generic_method_specializations[
        ctx->mir->generic_method_specialization_count++] = fact;
    return true;
}

static bool mir_generic_method_capture_node(
    MIRGenericMethodCaptureCtx *ctx, ASTNode *node);

static bool
mir_generic_method_capture_children(MIRGenericMethodCaptureCtx *ctx,
                                    ASTNode **nodes,
                                    size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (!mir_generic_method_capture_node(ctx, nodes[i]))
            return false;
    }
    return true;
}

static bool
mir_generic_method_capture_call(MIRGenericMethodCaptureCtx *ctx,
                                ASTNode *call)
{
    ASTNode *callee = ast_call_callee(call);

    if (!mir_generic_method_capture_node(ctx, callee)
        || !mir_generic_method_capture_children(ctx,
            ast_call_arguments(call, NULL), ast_call_arg_count(call))) {
        return false;
    }

    if (callee != NULL && callee->type == AST_MEMBER_ACCESS) {
        ASTNode *receiver = ast_member_object(callee);
        const char *method_name = ast_member_name(callee);
        MIRSourceLocalTypeScratch scratch = {0};
        const char *receiver_type = mir_source_local_expr_type_name(
            ctx->mir, ctx->caller, &scratch, receiver);
        const MIRDeclHeader *header = mir_generic_method_receiver_header(
            ctx->mir, receiver_type);
        const MIRDeclMethod *method = mir_generic_method_find(
            header, method_name);
        size_t method_routine_index = 0;

        if (method != NULL
            && mir_decl_method_routine_index(method, &method_routine_index)
            && method_routine_index < ctx->mir->routine_count
            && ctx->mir->routines[method_routine_index].generic_param_count > 0
            && !mir_generic_method_append(ctx, call, header, method,
                method_routine_index)) {
            return false;
        }
    }
    return true;
}

static bool
mir_generic_method_capture_node(MIRGenericMethodCaptureCtx *ctx,
                                ASTNode *node)
{
    if (ctx == NULL || node == NULL)
        return true;
    switch (node->type) {
    case AST_BLOCK:
        return mir_generic_method_capture_children(ctx,
            ast_block_statements(node, NULL), ast_block_statement_count(node));
    case AST_LET_DECL:
        return mir_generic_method_capture_node(ctx, ast_let_initializer(node));
    case AST_LET_DESTRUCTURE:
        return mir_generic_method_capture_node(
            ctx, ast_let_destructure_initializer(node));
    case AST_WITH_STMT:
        return mir_generic_method_capture_node(ctx, ast_with_body(node));
    case AST_FOR_LOOP:
        return mir_generic_method_capture_node(ctx, ast_for_range_start(node))
            && mir_generic_method_capture_node(ctx, ast_for_range_end(node))
            && mir_generic_method_capture_node(ctx, ast_for_iterable(node))
            && mir_generic_method_capture_node(ctx, ast_for_body(node));
    case AST_WHILE_LOOP:
        return mir_generic_method_capture_node(ctx, ast_while_condition(node))
            && mir_generic_method_capture_node(ctx, ast_while_body(node));
    case AST_IF_STMT:
        return mir_generic_method_capture_node(ctx, ast_if_condition(node))
            && mir_generic_method_capture_node(ctx, ast_if_then_branch(node))
            && mir_generic_method_capture_node(ctx, ast_if_else_branch(node));
    case AST_RETURN:
        return mir_generic_method_capture_node(ctx, ast_return_value(node));
    case AST_GIVE_STMT:
        return mir_generic_method_capture_node(ctx, ast_give_value(node));
    case AST_CALL:
        return mir_generic_method_capture_call(ctx, node);
    case AST_MEMBER_ACCESS:
        return mir_generic_method_capture_node(ctx, ast_member_object(node));
    case AST_ASSIGNMENT:
        return mir_generic_method_capture_node(ctx, ast_assignment_target(node))
            && mir_generic_method_capture_node(ctx, ast_assignment_value(node));
    case AST_BINARY:
        return mir_generic_method_capture_node(ctx, ast_binary_left(node))
            && mir_generic_method_capture_node(ctx, ast_binary_right(node));
    case AST_UNARY:
        return mir_generic_method_capture_node(ctx, ast_unary_operand(node));
    case AST_ARRAY_ACCESS:
        return mir_generic_method_capture_node(ctx, ast_array_access_array(node))
            && mir_generic_method_capture_node(ctx, ast_array_access_index(node));
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++)
            if (!mir_generic_method_capture_node(
                    ctx, ast_array_literal_element(node, i)))
                return false;
        return true;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++)
            if (!mir_generic_method_capture_node(
                    ctx, ast_tuple_literal_element(node, i)))
                return false;
        return true;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++)
            if (!mir_generic_method_capture_node(ctx,
                    ast_map_literal_key(node, i))
                || !mir_generic_method_capture_node(ctx,
                    ast_map_literal_value(node, i)))
                return false;
        return true;
    case AST_SET_LITERAL:
        for (size_t i = 0; i < ast_set_literal_count(node); i++)
            if (!mir_generic_method_capture_node(
                    ctx, ast_set_literal_element(node, i)))
                return false;
        return true;
    case AST_CAST:
        return mir_generic_method_capture_node(ctx, ast_cast_operand(node));
    case AST_TYPE_TEST:
        return mir_generic_method_capture_node(ctx,
            ast_type_test_operand(node));
    case AST_AWAIT_EXPR:
        return mir_generic_method_capture_node(ctx, ast_await_expression(node));
    case AST_CHANNEL_SEND:
        return mir_generic_method_capture_node(ctx,
                   ast_channel_send_channel(node))
            && mir_generic_method_capture_node(ctx,
                   ast_channel_send_value(node));
    case AST_CHANNEL_RECV:
        return mir_generic_method_capture_node(ctx,
            ast_channel_recv_channel(node));
    case AST_ASYNC_BLOCK:
        return mir_generic_method_capture_children(ctx,
            ast_async_block_statements(node, NULL),
            ast_async_block_statement_count(node));
    case AST_PARALLEL_BLOCK:
        return mir_generic_method_capture_children(ctx,
            ast_parallel_tasks(node, NULL), ast_parallel_task_count(node));
    case AST_SPAWN_EXPR:
        return mir_generic_method_capture_node(ctx, ast_spawn_function(node))
            && mir_generic_method_capture_children(ctx,
                ast_spawn_arguments(node, NULL), ast_spawn_arg_count(node));
    case AST_MATCH_STMT:
        if (!mir_generic_method_capture_node(ctx, ast_match_subject(node)))
            return false;
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            if (!mir_generic_method_capture_node(
                    ctx, ast_match_case_at(node, i)))
                return false;
        return mir_generic_method_capture_node(ctx,
            ast_match_default_body(node));
    case AST_MATCH_CASE:
        return mir_generic_method_capture_node(ctx,
                   ast_match_case_pattern(node))
            && mir_generic_method_capture_node(ctx,
                   ast_match_case_guard(node))
            && mir_generic_method_capture_node(ctx,
                   ast_match_case_body(node));
    case AST_SELECT_STMT:
        return mir_generic_method_capture_children(ctx,
                   ast_select_cases(node, NULL), ast_select_case_count(node))
            && mir_generic_method_capture_node(ctx,
                   ast_select_default_case(node));
    case AST_LAMBDA_EXPR:
        return mir_generic_method_capture_node(ctx, ast_lambda_body(node));
    case AST_UNSAFE_BLOCK:
        return mir_generic_method_capture_node(ctx,
            ast_unsafe_block_body(node));
    case AST_TRANSACTION_BLOCK:
        return mir_generic_method_capture_node(ctx,
            ast_transaction_block_body(node));
    case AST_DEFER_STMT:
        return mir_generic_method_capture_node(ctx, ast_defer_body(node));
    case AST_FAIL_STMT:
        return mir_generic_method_capture_node(ctx, ast_fail_stmt_reason(node));
    default:
        return true;
    }
}

bool
mir_generic_method_specializations_capture(MIRProgram *mir,
                                           char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL)
        return false;
    mir_generic_method_specializations_clear(mir);
    for (size_t i = 0; i < mir->routine_count; i++) {
        MIRGenericMethodCaptureCtx ctx;
        ASTNode *decl = mir->routines[i].ast;
        ASTNode *body = decl != NULL && decl->type == AST_FUNC_DECL
            ? ast_func_body(decl) : NULL;
        memset(&ctx, 0, sizeof(ctx));
        ctx.mir = mir;
        ctx.caller = &mir->routines[i];
        ctx.caller_index = i;
        ctx.error_message = error_message;
        if (!mir_generic_method_capture_node(&ctx, body)) {
            if (error_message != NULL && *error_message == NULL)
                *error_message = pergyra_strdup(
                    "out of memory while capturing MIR generic method specializations");
            mir_generic_method_specializations_clear(mir);
            return false;
        }
    }
    return mir_generic_method_specializations_validate(mir, error_message);
}
