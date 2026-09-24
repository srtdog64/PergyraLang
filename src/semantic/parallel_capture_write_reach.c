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
    id = node->data.identifier.name;
    if (id == NULL)
        return false;
    if (strcmp(id, w->name) == 0)
        return true;
    return w->in_method
        && (node->data.identifier.semantic_binding_is_host_field
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
        if (strcmp(node->data.identifier.name, w->name) == 0)
            return w->type;
        return parallel_reach_field_type(w->ctx, w->decl, node->data.identifier.name);
    }
    if (node->type != AST_MEMBER_ACCESS)
        return NULL;
    object_type = reach_path_type(w, node->data.member.object, rooted);
    if (!*rooted || object_type == NULL)
        return NULL;
    decl = parallel_reach_nominal_decl(w->ctx, object_type);
    return parallel_reach_field_type(w->ctx, decl, node->data.member.name);
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
    const ASTNode *callee = call->data.call.callee;
    const Symbol *sym;
    const Type *type;

    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return PARAM_MODE_DEFAULT;
    for (size_t i = 0; call->data.call.arg_names != NULL
                       && i < call->data.call.arg_count; i++) {
        if (call->data.call.arg_names[i] != NULL)
            return PARAM_MODE_DEFAULT;
    }
    sym = scope_lookup(w->ctx->scope, callee->data.identifier.name);
    type = sym != NULL ? sym->type : NULL;
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION
        || type->data.function.param_modes == NULL
        || index >= type->data.function.param_count)
        return PARAM_MODE_DEFAULT;
    return type->data.function.param_modes[index];
}

/* A ref parameter forbids assigning through it, but the callee may still
 * call a method that writes the receiver, so the callee's body is walked with
 * the parameter as the binding. Only identifier callees resolve a mode. */
static bool
reach_ref_argument_writes(const ReachWalk *w, const ASTNode *call, size_t index)
{
    const ASTNode *callee = call->data.call.callee;
    ASTNode *function = semantic_find_function_decl_by_name(
        w->ctx, callee->data.identifier.name);
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
    const ASTNode *callee = node->data.call.callee;

    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        const ASTNode *arg = node->data.call.arguments[i];

        /* The binding handed to an own parameter is a move, which the
         * parallel resource snapshot already rejects when another task also
         * moves or borrows it; handed to a ref parameter it writes only if
         * the callee's body writes through that parameter. */
        if (arg != NULL && arg->type == AST_IDENTIFIER
            && strcmp(arg->data.identifier.name, w->name) == 0) {
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
            reach_path_type(w, callee->data.member.object, &rooted);

        if (!rooted)
            return reach_walk(w, callee->data.member.object);
        if (receiver == NULL)
            return true;
        if (worker_boundary_storage_display_name(receiver) != NULL)
            return true; /* a collection field method may grow the storage */
        if (parallel_reach_nominal_decl(w->ctx, receiver) == NULL)
            return false; /* builtin method on a scalar or String value */
        return reach_method_writes_self(w, receiver, callee->data.member.name);
    }
    if (callee->type == AST_IDENTIFIER
        && strcmp(callee->data.identifier.name, w->name) == 0)
        return true; /* the binding itself is called */
    if (callee->type == AST_IDENTIFIER
        && reach_names_host_member(w, callee->data.identifier.name))
        return reach_method_writes_self(w, w->type,
                                        callee->data.identifier.name);
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
        if (strcmp(node->data.identifier.name, w->name) == 0)
            return true; /* the reference itself flows elsewhere */
        return reach_value_read_escapes(w, node, &rooted);
    case AST_MEMBER_ACCESS:
        if (reach_value_read_escapes(w, node, &rooted))
            return true;
        return rooted ? false : reach_walk(w, node->data.member.object);
    case AST_ASSIGNMENT: {
        const ASTNode *root = node->data.assignment.target;
        while (root != NULL && (root->type == AST_MEMBER_ACCESS
                                || root->type == AST_ARRAY_ACCESS)) {
            if (root->type == AST_ARRAY_ACCESS
                && reach_walk(w, root->data.array_access.index))
                return true;
            root = root->type == AST_MEMBER_ACCESS
                ? root->data.member.object : root->data.array_access.array;
        }
        if (reach_is_binding_root(w, root))
            return true;
        return reach_walk(w, node->data.assignment.value);
    }
    case AST_CALL:
        return reach_walk_call(w, node);
    case AST_ARRAY_ACCESS:
        return reach_walk(w, node->data.array_access.array)
            || reach_walk(w, node->data.array_access.index);
    case AST_BINARY:
        return reach_walk(w, node->data.binary.left)
            || reach_walk(w, node->data.binary.right);
    case AST_UNARY:
        return reach_walk(w, node->data.unary.operand);
    case AST_CAST:
        return reach_walk(w, node->data.cast.operand);
    case AST_TYPE_TEST:
        return reach_walk(w, node->data.type_test.operand);
    case AST_BLOCK:
        return reach_walk_list(w, node->data.block.statements,
                               node->data.block.count);
    case AST_IF_STMT:
        return reach_walk(w, node->data.if_stmt.condition)
            || reach_walk(w, node->data.if_stmt.then_branch)
            || reach_walk(w, node->data.if_stmt.else_branch);
    case AST_WHILE_LOOP:
        return reach_walk(w, node->data.while_loop.condition)
            || reach_walk(w, node->data.while_loop.body);
    case AST_FOR_LOOP:
        return reach_walk(w, node->data.for_loop.range_start)
            || reach_walk(w, node->data.for_loop.range_end)
            || reach_walk(w, node->data.for_loop.iterable)
            || reach_walk(w, node->data.for_loop.body);
    case AST_RETURN:
        return reach_walk(w, node->data.return_stmt.value);
    case AST_GIVE_STMT:
        return reach_walk(w, node->data.give_stmt.value);
    case AST_LET_DECL:
        return reach_walk(w, node->data.let_decl.initializer);
    case AST_ARRAY_LITERAL:
        return reach_walk_list(w, node->data.array_literal.elements,
                               node->data.array_literal.count);
    case AST_TUPLE_LITERAL:
        return reach_walk_list(w, node->data.tuple_literal.elements,
                               node->data.tuple_literal.count);
    case AST_CHANNEL_SEND:
        return reach_walk(w, node->data.channel_send.channel)
            || reach_walk(w, node->data.channel_send.value);
    case AST_CHANNEL_RECV:
        return reach_walk(w, node->data.channel_recv.channel);
    case AST_AWAIT_EXPR:
        return reach_walk(w, node->data.await_expr.expression);
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
