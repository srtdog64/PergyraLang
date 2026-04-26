#ifndef PGY_TRANSPILER_PARALLEL_CAPTURE_H
#define PGY_TRANSPILER_PARALLEL_CAPTURE_H

/* Helpers for discovering locals captured by generated C parallel blocks. */

static ASTNode *
transpiler_find_local_let_type_node(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_local_let_type_node(
                body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        return body->data.let_decl.type;
    }
    return NULL;
}

static const char *
transpiler_current_local_type_name(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || ctx->current_func_decl == NULL)
        return NULL;
    return transpiler_find_local_type_name(ctx, ctx->current_func_decl, name);
}

static bool
transpiler_parallel_capture_has_name(char names[MAX_SLOT_VARS][64],
                                     int count,
                                     const char *name)
{
    if (name == NULL)
        return false;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void
transpiler_parallel_add_capture_name(TranspilerCtx *ctx,
                                     const char *name,
                                     char slot_names[MAX_SLOT_VARS][64],
                                     int *slot_count,
                                     char typed_names[MAX_SLOT_VARS][64],
                                     int *typed_count)
{
    if (ctx == NULL || name == NULL || name[0] == '\0'
        || strcmp(name, "self") == 0) {
        return;
    }

    if (is_slot_var(ctx, name)) {

        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && slot_count != NULL && *slot_count < MAX_SLOT_VARS) {
            strncpy(slot_names[*slot_count], name, sizeof(slot_names[*slot_count]) - 1);
            slot_names[*slot_count][sizeof(slot_names[*slot_count]) - 1] = '\0';
            (*slot_count)++;
        }
        return;
    }

    if (lookup_typed_entry(ctx, name) != NULL) {
        const char *type_name = lookup_typed_var(ctx, name);
        if ((type_name == NULL || strcmp(type_name, "Unknown") == 0)
            ) {
            type_name = transpiler_current_local_type_name(ctx, name);
            if (type_name != NULL && type_name[0] != '\0'
                && strcmp(type_name, "Unknown") != 0) {
                register_typed_var(ctx, name, type_name);
            }
        }
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL && *typed_count < MAX_SLOT_VARS) {
            strncpy(typed_names[*typed_count], name, sizeof(typed_names[*typed_count]) - 1);
            typed_names[*typed_count][sizeof(typed_names[*typed_count]) - 1] = '\0';
            (*typed_count)++;
        }
    } else {
        const char *type_name = transpiler_current_local_type_name(ctx, name);
        if (type_name != NULL && type_name[0] != '\0'
            && strcmp(type_name, "Unknown") != 0
            && !transpiler_parallel_capture_has_name(slot_names,
                    slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                    typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL && *typed_count < MAX_SLOT_VARS) {
            register_typed_var(ctx, name, type_name);
            strncpy(typed_names[*typed_count], name, sizeof(typed_names[*typed_count]) - 1);
            typed_names[*typed_count][sizeof(typed_names[*typed_count]) - 1] = '\0';
            (*typed_count)++;
        }
    }
}

static void
transpiler_parallel_collect_stmt_captures(ASTNode *node,
                                          TranspilerCtx *ctx,
                                          char slot_names[MAX_SLOT_VARS][64],
                                          int *slot_count,
                                          char typed_names[MAX_SLOT_VARS][64],
                                          int *typed_count)
{
    if (node == NULL || ctx == NULL)
        return;

    switch (node->type) {
    case AST_IDENTIFIER:
        transpiler_parallel_add_capture_name(ctx, node->data.identifier.name,
            slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_LET_DECL:
        transpiler_parallel_collect_stmt_captures(node->data.let_decl.initializer,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ASSIGNMENT:
        transpiler_parallel_collect_stmt_captures(node->data.assignment.target,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.assignment.value,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CHANNEL_SEND:
        transpiler_parallel_collect_stmt_captures(node->data.channel_send.channel,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.channel_send.value,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CHANNEL_RECV:
        transpiler_parallel_collect_stmt_captures(node->data.channel_recv.channel,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CALL:
        transpiler_parallel_collect_stmt_captures(node->data.call.callee,
            ctx, slot_names, slot_count, typed_names, typed_count);
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.call.arguments[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_MEMBER_ACCESS:
        transpiler_parallel_collect_stmt_captures(node->data.member.object,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ARRAY_ACCESS:
        transpiler_parallel_collect_stmt_captures(node->data.array_access.array,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.array_access.index,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.array_literal.elements[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_BINARY:
        transpiler_parallel_collect_stmt_captures(node->data.binary.left,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.binary.right,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_UNARY:
        transpiler_parallel_collect_stmt_captures(node->data.unary.operand,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_AWAIT_EXPR:
        transpiler_parallel_collect_stmt_captures(node->data.await_expr.expression,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_SPAWN_EXPR:
        transpiler_parallel_collect_stmt_captures(node->data.spawn_expr.function,
            ctx, slot_names, slot_count, typed_names, typed_count);
        for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.spawn_expr.arguments[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_IF_STMT:
        transpiler_parallel_collect_stmt_captures(node->data.if_stmt.condition,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.if_stmt.then_branch,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.if_stmt.else_branch,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_RETURN:
        transpiler_parallel_collect_stmt_captures(node->data.return_stmt.value,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.block.statements[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.parallel.tasks[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
            transpiler_parallel_collect_stmt_captures(
                node->data.async_block.statements[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    default:
        break;
    }
}

#endif /* PGY_TRANSPILER_PARALLEL_CAPTURE_H */
