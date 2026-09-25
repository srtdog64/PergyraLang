#include <stdlib.h>
#include <string.h>
#include "parallel_capture_storage_reach.h"
#include "parallel_capture_reach_internal.h"
#include "../parser/ast_analysis.h"

/*
 * Writes a parallel task makes through a captured binding without assigning
 * it: a method call that writes the receiver, or the binding handed to a
 * callee that may write it. See parallel_capture_storage_reach.c for the
 * storage half and the host field model this walk reads.
 */

typedef struct {
    SemanticContext *ctx;
    const char *name;        /* the binding; "self" inside a method body */
    const Type *type;        /* the binding's nominal type */
    ASTNode *decl;           /* declaration of `type` */
    bool in_method;          /* bare field and method names mean self */
    unsigned depth;
    const ASTNode *const *active; /* methods on the analysis stack */
    size_t active_count;
} ReachWalk;

static bool reach_walk(const ReachWalk *w, const ASTNode *node);

static bool
reach_names_host_member(const ReachWalk *w, const char *name)
{
    size_t method_count = 0;
    ASTNode **methods;

    if (!w->in_method || name == NULL)
        return false;
    if (parallel_reach_field_type(w->ctx, w->decl, name) != NULL)
        return true;
    methods = semantic_host_decl_methods(w->decl, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        const char *method_name = ast_declaration_name(methods[i]);
        if (method_name != NULL && strcmp(method_name, name) == 0)
            return true;
    }
    return false;
}

/* Is `node` the binding itself, or (in a method) a bare field of self? */
static bool
reach_is_binding_root(const ReachWalk *w, const ASTNode *node)
{
    const char *id;

    if (node == NULL || node->type != AST_IDENTIFIER)
        return false;
    id = ast_identifier_name(node);
    if (id == NULL)
        return false;
    if (strcmp(id, w->name) == 0)
        return true;
    return w->in_method
        && (ast_identifier_binding_is_host_field(node)
            || parallel_reach_field_type(w->ctx, w->decl, id) != NULL);
}

/* The type of a member path rooted at the binding (`b`, `b.f`, `b.f.g`, or a
 * bare self field), or NULL when the path is not rooted at the binding or a
 * step does not resolve. *rooted reports which. */
static const Type *
reach_path_type(const ReachWalk *w, const ASTNode *node, bool *rooted)
{
    const Type *object_type;
    ASTNode *decl;

    *rooted = false;
    if (node == NULL)
        return NULL;
    if (node->type == AST_IDENTIFIER) {
        if (!reach_is_binding_root(w, node))
            return NULL;
        *rooted = true;
        if (strcmp(ast_identifier_name(node), w->name) == 0)
            return w->type;
        return parallel_reach_field_type(w->ctx, w->decl, ast_identifier_name(node));
    }
    if (node->type != AST_MEMBER_ACCESS)
        return NULL;
    object_type = reach_path_type(w, ast_member_object(node), rooted);
    if (!*rooted || object_type == NULL)
        return NULL;
    decl = parallel_reach_nominal_decl(w->ctx, object_type);
    return parallel_reach_field_type(w->ctx, decl, ast_member_name(node));
}

/* A value read of a path rooted at the binding copies a scalar out; a nominal
 * value read hands the reference on, which another task may write through. */
static bool
reach_value_read_escapes(const ReachWalk *w, const ASTNode *node, bool *rooted)
{
    const Type *type = reach_path_type(w, node, rooted);

    if (!*rooted)
        return false;
    if (type == NULL)
        return true;
    return parallel_reach_nominal_decl(w->ctx, type) != NULL;
}

/* Walk the body of `callable` as the code that holds the binding under
 * `name`: self in a method, the parameter's name in a function. A callable
 * already on the stack adds nothing new (the cycle's other statements are
 * walked already); no body to read or the depth limit fails closed. */
static bool
reach_callable_body_writes(const ReachWalk *w, const ASTNode *callable,
                           const char *name, const Type *type, ASTNode *decl,
                           bool in_method)
{
    const ASTNode *stack[PARALLEL_REACH_DEPTH_LIMIT + 1];
    ReachWalk inner;

    if (callable == NULL || name == NULL)
        return true;
    for (size_t a = 0; a < w->active_count; a++) {
        if (w->active[a] == callable)
            return false;
    }
    if (w->depth >= PARALLEL_REACH_DEPTH_LIMIT || ast_func_body(callable) == NULL)
        return true;
    for (size_t a = 0; a < w->active_count; a++)
        stack[a] = w->active[a];
    stack[w->active_count] = callable;
    inner = (ReachWalk){ w->ctx, name, type, decl, in_method, w->depth + 1,
                         stack, w->active_count + 1 };
    return reach_walk(&inner, ast_func_body(callable));
}

static bool
reach_method_writes_self(const ReachWalk *w, const Type *receiver_type,
                         const char *method_name)
{
    ASTNode *decl = parallel_reach_nominal_decl(w->ctx, receiver_type);
    size_t method_count = 0;
    ASTNode **methods;
    bool found = false;

    if (decl == NULL || method_name == NULL)
        return true;
    methods = semantic_host_decl_methods(decl, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        const char *name = ast_declaration_name(methods[i]);

        if (name == NULL || strcmp(name, method_name) != 0)
            continue;
        found = true;
        if (reach_callable_body_writes(w, methods[i], "self", receiver_type,
                                       decl, true))
            return true;
    }
    return !found; /* a callable field or an unknown member: fail closed */
}

/* The declared mode of positional parameter `index` of a free function
 * callee, or PARAM_MODE_DEFAULT when the callee or its modes do not resolve
 * or the call names its arguments. */
static ParamMode
reach_callee_param_mode(const ReachWalk *w, const ASTNode *call, size_t index)
{
    const ASTNode *callee = ast_call_callee(call);
    const Symbol *sym;
    const Type *type;

    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return PARAM_MODE_DEFAULT;
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        if (ast_call_argument_name(call, i) != NULL)
            return PARAM_MODE_DEFAULT;
    }
    sym = scope_lookup(w->ctx->scope, ast_identifier_name(callee));
    type = sym != NULL ? sym->type : NULL;
    return type_function_param_mode(type, index);
}

/* A ref parameter forbids assigning through it, but the callee may still
 * call a method that writes the receiver, so the callee's body is walked with
 * the parameter as the binding. Only identifier callees resolve a mode. */
static bool
reach_ref_argument_writes(const ReachWalk *w, const ASTNode *call, size_t index)
{
    const ASTNode *callee = ast_call_callee(call);
    ASTNode *function = semantic_find_function_decl_by_name(
        w->ctx, ast_identifier_name(callee));
    FuncParam *param = function != NULL && index < ast_func_param_count(function)
        ? ast_func_param(function, index) : NULL;

    if (param == NULL || param->name == NULL)
        return true;
    return reach_callable_body_writes(w, function, param->name, w->type,
                                      w->decl, false);
}

static bool
reach_walk_call(const ReachWalk *w, const ASTNode *node)
{
    const ASTNode *callee = ast_call_callee(node);

    for (size_t i = 0; i < ast_call_arg_count(node); i++) {
        const ASTNode *arg = ast_call_argument(node, i);

        /* The binding handed to an own parameter is a move, which the
         * parallel resource snapshot already rejects when another task also
         * moves or borrows it; handed to a ref parameter it writes only if
         * the callee's body writes through that parameter. */
        if (arg != NULL && arg->type == AST_IDENTIFIER
            && strcmp(ast_identifier_name(arg), w->name) == 0) {
            ParamMode mode = reach_callee_param_mode(w, node, i);
            if (mode == PARAM_MODE_OWN)
                continue;
            if (mode == PARAM_MODE_REF) {
                if (reach_ref_argument_writes(w, node, i))
                    return true;
                continue;
            }
        }
        if (reach_walk(w, arg))
            return true;
    }
    if (callee == NULL)
        return false;
    if (callee->type == AST_MEMBER_ACCESS) {
        bool rooted = false;
        const Type *receiver =
            reach_path_type(w, ast_member_object(callee), &rooted);

        if (!rooted)
            return reach_walk(w, ast_member_object(callee));
        if (receiver == NULL)
            return true;
        if (worker_boundary_storage_display_name(receiver) != NULL)
            return true; /* a collection field method may grow the storage */
        if (parallel_reach_nominal_decl(w->ctx, receiver) == NULL)
            return false; /* builtin method on a scalar or String value */
        return reach_method_writes_self(w, receiver, ast_member_name(callee));
    }
    if (callee->type == AST_IDENTIFIER
        && strcmp(ast_identifier_name(callee), w->name) == 0)
        return true; /* the binding itself is called */
    if (callee->type == AST_IDENTIFIER
        && reach_names_host_member(w, ast_identifier_name(callee)))
        return reach_method_writes_self(w, w->type,
                                        ast_identifier_name(callee));
    if (callee->type == AST_IDENTIFIER)
        return false; /* a free function or builtin: its arguments were walked */
    return reach_walk(w, callee);
}

static bool
reach_walk_list(const ReachWalk *w, ASTNode *const *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (reach_walk(w, nodes[i]))
            return true;
    }
    return false;
}

static bool
reach_mentions(const ReachWalk *w, const ASTNode *node)
{
    ParallelReachFields fields;
    bool mentions;

    if (ast_contains_free_identifier_ref(node, w->name))
        return true;
    if (!w->in_method)
        return false;
    fields = parallel_reach_host_fields(w->ctx, w->decl);
    mentions = !fields.complete;
    for (size_t i = 0; i < fields.count && !mentions; i++)
        mentions = fields.items[i].name != NULL
            && ast_contains_free_identifier_ref(node, fields.items[i].name);
    free(fields.items);
    return mentions;
}

static bool
reach_walk(const ReachWalk *w, const ASTNode *node)
{
    bool rooted = false;

    if (node == NULL)
        return false;
    switch (node->type) {
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
    case AST_BREAK:
    case AST_CONTINUE:
        return false;
    case AST_IDENTIFIER:
        if (strcmp(ast_identifier_name(node), w->name) == 0)
            return true; /* the reference itself flows elsewhere */
        return reach_value_read_escapes(w, node, &rooted);
    case AST_MEMBER_ACCESS:
        if (reach_value_read_escapes(w, node, &rooted))
            return true;
        return rooted ? false : reach_walk(w, ast_member_object(node));
    case AST_ASSIGNMENT: {
        const ASTNode *root = ast_assignment_target(node);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS
                                || root->type == AST_ARRAY_ACCESS)) {
            if (root->type == AST_ARRAY_ACCESS
                && reach_walk(w, ast_array_access_index(root)))
                return true;
            root = root->type == AST_MEMBER_ACCESS
                ? ast_member_object(root) : ast_array_access_array(root);
        }
        if (reach_is_binding_root(w, root))
            return true;
        return reach_walk(w, ast_assignment_value(node));
    }
    case AST_CALL:
        return reach_walk_call(w, node);
    case AST_ARRAY_ACCESS:
        return reach_walk(w, ast_array_access_array(node))
            || reach_walk(w, ast_array_access_index(node));
    case AST_BINARY:
        return reach_walk(w, ast_binary_left(node))
            || reach_walk(w, ast_binary_right(node));
    case AST_UNARY:
        return reach_walk(w, ast_unary_operand(node));
    case AST_CAST:
        return reach_walk(w, ast_cast_operand(node));
    case AST_TYPE_TEST:
        return reach_walk(w, ast_type_test_operand(node));
    case AST_BLOCK: {
        size_t count = 0;
        ASTNode **statements = ast_block_statements(node, &count);
        return reach_walk_list(w, statements, count);
    }
    case AST_IF_STMT:
        return reach_walk(w, ast_if_condition(node))
            || reach_walk(w, ast_if_then_branch(node))
            || reach_walk(w, ast_if_else_branch(node));
    case AST_WHILE_LOOP:
        return reach_walk(w, ast_while_condition(node))
            || reach_walk(w, ast_while_body(node));
    case AST_FOR_LOOP:
        return reach_walk(w, ast_for_range_start(node))
            || reach_walk(w, ast_for_range_end(node))
            || reach_walk(w, ast_for_iterable(node))
            || reach_walk(w, ast_for_body(node));
    case AST_RETURN:
        return reach_walk(w, ast_return_value(node));
    case AST_GIVE_STMT:
        return reach_walk(w, ast_give_value(node));
    case AST_LET_DECL:
        return reach_walk(w, ast_let_initializer(node));
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
            if (reach_walk(w, ast_array_literal_element(node, i)))
                return true;
        }
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
            if (reach_walk(w, ast_tuple_literal_element(node, i)))
                return true;
        }
        return false;
    case AST_CHANNEL_SEND:
        return reach_walk(w, ast_channel_send_channel(node))
            || reach_walk(w, ast_channel_send_value(node));
    case AST_CHANNEL_RECV:
        return reach_walk(w, ast_channel_recv_channel(node));
    case AST_AWAIT_EXPR:
        return reach_walk(w, ast_await_expression(node));
    default:
        /* closures, match arms, channel ops, defers, ...: not modeled, so
         * any mention of the binding counts as a write */
        return reach_mentions(w, node);
    }
}

bool
parallel_task_writes_through_binding(SemanticContext *ctx,
                                     const ASTNode *task,
                                     const char *name,
                                     const Type *type)
{
    ReachWalk walk;
    ASTNode *decl;

    if (ctx == NULL || task == NULL || name == NULL)
        return false;
    decl = parallel_reach_nominal_decl(ctx, type);
    if (decl == NULL)
        return false;
    if (!ast_contains_free_identifier_ref(task, name))
        return false;
    walk = (ReachWalk){ ctx, name, type, decl, false, 0, NULL, 0 };
    return reach_walk(&walk, task);
}
