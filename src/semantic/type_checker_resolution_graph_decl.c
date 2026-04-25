#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
resolution_decl_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static void
semantic_type_resolution_precollect_body_type_refs(ASTNode *stmt,
                                                   SemanticContext *ctx,
                                                   const ASTNode *owner,
                                                   const char *owner_name);

static void
semantic_type_resolution_precollect_expr_type_refs(ASTNode *expr,
                                                   SemanticContext *ctx,
                                                   const ASTNode *owner,
                                                   const char *owner_name)
{
    if (expr == NULL || ctx == NULL)
        return;

    switch (expr->type) {
    case AST_LAMBDA_EXPR:
        for (size_t i = 0; i < expr->data.lambda_expr.param_count; i++) {
            ASTNode *param = expr->data.lambda_expr.params[i];
            char *consumer_name;

            if (param == NULL || param->type != AST_LET_DECL)
                continue;

            consumer_name = resolution_decl_strdup_fmt(
                "lambda %s.%s",
                owner_name != NULL ? owner_name : "<lambda>",
                param->data.let_decl.name != NULL
                    ? param->data.let_decl.name : "<param>");
            if (consumer_name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    param->data.let_decl.type,
                    ctx,
                    param,
                    consumer_name,
                    "lambda parameter type lookup");
                free(consumer_name);
            }
        }
        semantic_type_resolution_collect_type_refs(
            expr->data.lambda_expr.return_type,
            ctx,
            expr,
            owner_name != NULL ? owner_name : "<lambda>",
            "lambda return type lookup");
        if (expr->data.lambda_expr.body != NULL
            && expr->data.lambda_expr.body->type == AST_BLOCK) {
            semantic_type_resolution_precollect_body_type_refs(
                expr->data.lambda_expr.body, ctx, owner, owner_name);
        } else {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.lambda_expr.body, ctx, owner, owner_name);
        }
        return;

    case AST_CALL:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.call.callee, ctx, owner, owner_name);
        for (size_t i = 0; i < expr->data.call.arg_count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.call.arguments[i], ctx, owner, owner_name);
        }
        if (expr->data.call.generic_args != NULL) {
            for (size_t i = 0; i < expr->data.call.generic_args->count; i++) {
                GenericParam *arg = expr->data.call.generic_args->params[i];
                if (arg != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
                        ctx,
                        expr,
                        owner_name != NULL ? owner_name : "<call>",
                        "call type-argument lookup");
                }
            }
        }
        return;

    case AST_BINARY:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.binary.left, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.binary.right, ctx, owner, owner_name);
        return;

    case AST_UNARY:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.unary.operand, ctx, owner, owner_name);
        return;

    case AST_MEMBER_ACCESS:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.member.object, ctx, owner, owner_name);
        return;

    case AST_ARRAY_ACCESS:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.array_access.array, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.array_access.index, ctx, owner, owner_name);
        return;

    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < expr->data.array_literal.count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.array_literal.elements[i], ctx, owner, owner_name);
        }
        return;

    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < expr->data.tuple_literal.count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.tuple_literal.elements[i], ctx, owner, owner_name);
        }
        return;

    case AST_ASSIGNMENT:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.assignment.target, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.assignment.value, ctx, owner, owner_name);
        return;

    case AST_AWAIT_EXPR:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.await_expr.expression, ctx, owner, owner_name);
        return;

    case AST_CHANNEL_SEND:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.channel_send.channel, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.channel_send.value, ctx, owner, owner_name);
        return;

    case AST_CHANNEL_RECV:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.channel_recv.channel, ctx, owner, owner_name);
        return;

    case AST_SPAWN_EXPR:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.spawn_expr.function, ctx, owner, owner_name);
        for (size_t i = 0; i < expr->data.spawn_expr.arg_count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.spawn_expr.arguments[i], ctx, owner, owner_name);
        }
        return;

    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.event_op.event, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.event_op.handler, ctx, owner, owner_name);
        return;

    case AST_EVENT_INVOKE:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.event_invoke.event, ctx, owner, owner_name);
        for (size_t i = 0; i < expr->data.event_invoke.arg_count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.event_invoke.arguments[i], ctx, owner, owner_name);
        }
        return;

    case AST_BLOCK:
        semantic_type_resolution_precollect_body_type_refs(
            expr, ctx, owner, owner_name);
        return;

    default:
        return;
    }
}

static void
semantic_type_resolution_precollect_body_type_refs(ASTNode *stmt,
                                                   SemanticContext *ctx,
                                                   const ASTNode *owner,
                                                   const char *owner_name)
{
    char *consumer_name;

    if (stmt == NULL || ctx == NULL)
        return;

    switch (stmt->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < stmt->data.block.count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.block.statements[i], ctx, owner, owner_name);
        }
        return;

    case AST_LET_DECL:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.let_decl.initializer, ctx, owner, owner_name);
        consumer_name = resolution_decl_strdup_fmt(
            "body %s.%s",
            owner_name != NULL ? owner_name : "<body>",
            stmt->data.let_decl.name != NULL
                ? stmt->data.let_decl.name : "<local>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                stmt->data.let_decl.type,
                ctx,
                stmt,
                consumer_name,
                "local let annotation lookup");
            free(consumer_name);
        }
        return;

    case AST_WITH_STMT:
        consumer_name = resolution_decl_strdup_fmt(
            "body %s.%s",
            owner_name != NULL ? owner_name : "<body>",
            stmt->data.with_stmt.alias != NULL
                ? stmt->data.with_stmt.alias : "<with>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                stmt->data.with_stmt.slot_type,
                ctx,
                stmt,
                consumer_name,
                "with slot type lookup");
            free(consumer_name);
        }
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.with_stmt.body, ctx, owner, owner_name);
        return;

    case AST_IF_STMT:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.if_stmt.condition, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.if_stmt.then_branch, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.if_stmt.else_branch, ctx, owner, owner_name);
        return;

    case AST_WHILE_LOOP:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.while_loop.condition, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.while_loop.body, ctx, owner, owner_name);
        return;

    case AST_FOR_LOOP:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.for_loop.range_start, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.for_loop.range_end, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.for_loop.iterable, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.for_loop.body, ctx, owner, owner_name);
        return;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < stmt->data.parallel.task_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.parallel.tasks[i], ctx, owner, owner_name);
        }
        return;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < stmt->data.async_block.statement_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.async_block.statements[i], ctx, owner, owner_name);
        }
        return;

    case AST_TASK_GROUP:
        for (size_t i = 0; i < stmt->data.task_group.task_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.task_group.tasks[i], ctx, owner, owner_name);
        }
        return;

    case AST_MATCH_STMT:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.match_stmt.subject, ctx, owner, owner_name);
        for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.match_stmt.cases[i], ctx, owner, owner_name);
        }
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.match_stmt.default_body, ctx, owner, owner_name);
        return;

    case AST_MATCH_CASE:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.match_case.guard, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.match_case.body, ctx, owner, owner_name);
        return;

    case AST_UNSAFE_BLOCK:
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.unsafe_block.body, ctx, owner, owner_name);
        return;

    case AST_DEFER_STMT:
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.defer_stmt.body, ctx, owner, owner_name);
        return;

    case AST_RETURN:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.return_stmt.value, ctx, owner, owner_name);
        return;

    case AST_ASSIGNMENT:
    case AST_CALL:
    case AST_LAMBDA_EXPR:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
    case AST_SPAWN_EXPR:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt, ctx, owner, owner_name);
        return;

    default:
        return;
    }
}

void
semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                    SemanticContext *ctx)
{
    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        party_decl->data.party_decl.generic_params,
        NULL,
        ctx,
        party_decl,
        "party",
        party_decl->data.party_decl.name);

    semantic_type_resolution_collect_type_refs(
        party_decl->data.party_decl.extends,
        ctx,
        party_decl,
        party_decl->data.party_decl.name != NULL
            ? party_decl->data.party_decl.name : "<party>",
        "party extends lookup");

    for (size_t i = 0; i < party_decl->data.party_decl.shared_count; i++) {
        ASTNode *field = party_decl->data.party_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<party-shared>",
            "party shared field type lookup");
    }

    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
        ASTNode *role_slot = party_decl->data.party_decl.role_slots[i];
        char *consumer_name;

        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "party %s.%s",
            party_decl->data.party_decl.name != NULL
                ? party_decl->data.party_decl.name : "<party>",
            role_slot->data.role_slot.slot_name != NULL
                ? role_slot->data.role_slot.slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            role_slot->data.role_slot.required_abilities,
            role_slot->data.role_slot.ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < party_decl->data.party_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            party_decl->data.party_decl.methods[i],
            ctx,
            party_decl->data.party_decl.name);
    }
}

void
semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                     SemanticContext *ctx)
{
    if (roster_decl == NULL || roster_decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        roster_decl->data.roster_decl.generic_params,
        NULL,
        ctx,
        roster_decl,
        "roster",
        roster_decl->data.roster_decl.name);

    for (size_t i = 0; i < roster_decl->data.roster_decl.shared_count; i++) {
        ASTNode *field = roster_decl->data.roster_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<roster-shared>",
            "roster shared field type lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.party_count; i++) {
        ASTNode *slot = roster_decl->data.roster_decl.party_slots[i];
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.roster_slot.slot_name != NULL
                ? slot->data.roster_slot.slot_name : "<roster-slot>",
            slot->data.roster_slot.party_type,
            "roster party lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            roster_decl->data.roster_decl.methods[i],
            ctx,
            roster_decl->data.roster_decl.name);
    }
}

void
semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                     SemanticContext *ctx)
{
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        semantic_type_resolution_collect_type_refs(
            involves->data.intent_involves.subject_type,
            ctx,
            involves,
            involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "<intent-binding>",
            "intent involves type lookup");
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.value_count; i++) {
        ASTNode *value = intent_decl->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        semantic_type_resolution_collect_type_refs(
            value->data.intent_value.value_type,
            ctx,
            value,
            value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "<intent-value>",
            "intent value type lookup");
    }

    semantic_type_resolution_collect_type_refs(
        intent_decl->data.intent_decl.default_where_type,
        ctx,
        intent_decl,
        intent_decl->data.intent_decl.name != NULL
            ? intent_decl->data.intent_decl.name : "<intent>",
        "intent default where-type lookup");

    for (size_t i = 0; i < intent_decl->data.intent_decl.step_count; i++) {
        ASTNode *step = intent_decl->data.intent_decl.steps[i];
        char *step_consumer_name;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        step_consumer_name = resolution_decl_strdup_fmt(
            "intent %s.%s",
            intent_decl->data.intent_decl.name != NULL
                ? intent_decl->data.intent_decl.name : "<intent>",
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>");
        if (step_consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            step->data.intent_step.where_type,
            ctx,
            step,
            step_consumer_name,
            "intent step where-type lookup");
        semantic_type_resolution_precollect_required_abilities(
            step->data.intent_step.required_abilities,
            step->data.intent_step.required_ability_count,
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        semantic_type_resolution_record_string_dependency(
            ctx,
            step,
            step_consumer_name,
            step->data.intent_step.causes_effect,
            "intent step causes-effect lookup");
        free(step_consumer_name);
    }
}

void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx)
{
    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        role_decl->data.role_decl.generic_params,
        role_decl->data.role_decl.where_clause,
        ctx,
        role_decl,
        "role",
        role_decl->data.role_decl.name);

    semantic_type_resolution_collect_type_refs(
        role_decl->data.role_decl.for_type,
        ctx,
        role_decl,
        role_decl->data.role_decl.name != NULL
            ? role_decl->data.role_decl.name : "<role>",
        "role host-type lookup");

    for (size_t i = 0; i < role_decl->data.role_decl.include_count; i++) {
        ASTNode *inc = role_decl->data.role_decl.includes[i];
        char *consumer_name;

        if (inc == NULL || inc->type != AST_INCLUDE_STMT)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "role %s.include",
            role_decl->data.role_decl.name != NULL
                ? role_decl->data.role_decl.name : "<role>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_record_string_dependency(
            ctx,
            inc,
            consumer_name,
            inc->data.include_stmt.role_name,
            "role include lookup");

        if (inc->data.include_stmt.type_args != NULL) {
            for (size_t j = 0; j < inc->data.include_stmt.type_args->count; j++) {
                GenericParam *arg = inc->data.include_stmt.type_args->params[j];
                if (arg != NULL && arg->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
                        ctx,
                        inc,
                        consumer_name,
                        "role include type-argument lookup");
                }
            }
        }
        free(consumer_name);
    }

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        semantic_type_resolution_collect_type_refs(
            impl->data.impl_ability.ability_ref,
            ctx,
            impl,
            role_decl->data.role_decl.name != NULL
                ? role_decl->data.role_decl.name : "<role>",
            "role impl ability lookup");
    }
}

void
semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                    SemanticContext *ctx)
{
    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        class_decl->data.class_decl.generic_params,
        class_decl->data.class_decl.where_clause,
        ctx,
        class_decl,
        "class",
        class_decl->data.class_decl.name);

    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *field = class_decl->data.class_decl.fields[i];
        char *consumer_name;

        if (field == NULL)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "class %s.%s",
            class_decl->data.class_decl.name != NULL
                ? class_decl->data.class_decl.name : "<class>",
            field->name != NULL ? field->name : "<field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                field->type,
                ctx,
                class_decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        semantic_type_resolution_precollect_action_contract(
            method,
            ctx,
            class_decl->data.class_decl.name);
    }
}

void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name)
{
    const char *consumer_name;

    if (method == NULL || method->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = method->data.func_decl.name != NULL
        ? method->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<action>");

    semantic_type_resolution_collect_generic_contract_inventory(
        method->data.func_decl.generic_params,
        method->data.func_decl.where_clause,
        ctx,
        method,
        "func",
        consumer_name);

    for (size_t i = 0; i < method->data.func_decl.param_count; i++) {
        FuncParam *param = method->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = resolution_decl_strdup_fmt(
            "func %s.%s",
            consumer_name,
            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                param->type,
                ctx,
                method,
                param_consumer_name,
                "function parameter type lookup");
            free(param_consumer_name);
        }
    }

    semantic_type_resolution_collect_type_refs(
        method->data.func_decl.return_type,
        ctx,
        method,
        consumer_name,
        "function return type lookup");

    semantic_type_resolution_precollect_required_abilities(
        method->data.func_decl.required_abilities,
        method->data.func_decl.required_ability_count,
        ctx,
        method,
        consumer_name,
        "action ability consumer lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.within_zone,
        "action within-zone lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.causes_effect,
        "action causes-effect lookup");

    semantic_type_resolution_precollect_body_type_refs(
        method->data.func_decl.body,
        ctx,
        method,
        consumer_name);
}

void
semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                      SemanticContext *ctx)
{
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ability_decl->data.ability_decl.generic_params,
        ability_decl->data.ability_decl.where_clause,
        ctx,
        ability_decl,
        "ability",
        ability_decl->data.ability_decl.name);

    for (size_t i = 0; i < ability_decl->data.ability_decl.require_count; i++) {
        ASTNode *req = ability_decl->data.ability_decl.require_fields[i];
        char *consumer_name;

        if (req == NULL || req->type != AST_REQUIRE_FIELD)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "ability %s.%s",
            ability_decl->data.ability_decl.name != NULL
                ? ability_decl->data.ability_decl.name : "<ability>",
            req->data.require_field.name != NULL
                ? req->data.require_field.name : "<require-field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < ability_decl->data.ability_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            ability_decl->data.ability_decl.methods[i],
            ctx,
            ability_decl->data.ability_decl.name);
    }
}

void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx)
{
    if (enum_decl == NULL || enum_decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < enum_decl->data.enum_decl.variant_count; i++) {
        ASTNode **params = enum_decl->data.enum_decl.variant_params != NULL
            ? enum_decl->data.enum_decl.variant_params[i] : NULL;
        size_t param_count = enum_decl->data.enum_decl.variant_param_counts != NULL
            ? enum_decl->data.enum_decl.variant_param_counts[i] : 0;
        const char *variant_name = enum_decl->data.enum_decl.variants != NULL
            ? enum_decl->data.enum_decl.variants[i] : NULL;
        char *consumer_name;

        if (params == NULL || param_count == 0)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "enum %s.%s",
            enum_decl->data.enum_decl.name != NULL
                ? enum_decl->data.enum_decl.name : "<enum>",
            variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            semantic_type_resolution_collect_type_refs(
                params[j],
                ctx,
                enum_decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }

    for (size_t i = 0; i < enum_decl->data.enum_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            enum_decl->data.enum_decl.methods[i],
            ctx,
            enum_decl->data.enum_decl.name);
    }
}

void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                    SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = resolution_decl_strdup_fmt(
            "event %s.%s",
            event_decl->data.event_decl.name != NULL
                ? event_decl->data.event_decl.name : "<event>",
            param->data.let_decl.name != NULL
                ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    semantic_type_resolution_collect_type_refs(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name != NULL
            ? event_decl->data.event_decl.name : "<event>",
        "event return type lookup");
}
