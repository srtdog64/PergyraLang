#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "function_param_publication_internal.h"
#include "callable_capability_inference.h"

bool publication_ref_call_tuple(const SemanticContext *ctx, const ASTNode *call,
    const ASTNode *site, const ASTNode *callee, size_t ordinal, size_t *formal)
{
    size_t index = 0;
    if (formal == NULL || call == NULL || call->type != AST_CALL || site == NULL ||
        callee == NULL || callee->type != AST_FUNC_DECL ||
        ast_node_stable_id(call) == 0 || ast_node_stable_id(site) == 0 ||
        ast_node_stable_id(callee) == 0 || ordinal >= ast_call_arg_count(call) ||
        ast_call_argument(call, ordinal) != site ||
        ast_call_semantic_callee_decl_id(call) != ast_node_stable_id(callee) ||
        ast_call_semantic_callee_value_binding_id(call) != 0 ||
        !(semantic_param_publication_pending(ctx)
            ? semantic_param_publication_selected_formal_for_actual((SemanticContext *)ctx,
                call, callee, ordinal, &index)
            : semantic_param_publication_formal_for_actual(ctx, call, ordinal, &index)) ||
        ast_func_param(callee, index) == NULL ||
        ast_func_param(callee, index)->mode != PARAM_MODE_REF) return false;
    *formal = index;
    return true;
}

bool semantic_param_publication_selected_formal_for_actual(SemanticContext *ctx,
    const ASTNode *call, const ASTNode *decl, size_t actual, size_t *formal)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL ||
        decl == NULL || formal == NULL || actual >= ast_call_arg_count(call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    const ASTNode *target = ast_call_callee(call);
    const ASTNode *receiver = target != NULL && target->type == AST_MEMBER_ACCESS
        ? ast_member_object(target) : NULL;
    PublishedNode *value = publication_node(ctx->function_param_publications, receiver, false);
    const Type *receiver_type = value != NULL && value->value_present &&
        value->id == ast_node_stable_id(receiver) && value->value_shape == receiver->type &&
        !value->meta ? value->value_type : NULL;
    return row != NULL && row->selected_callable == decl &&
        row->selected_callable_id == ast_node_stable_id(decl) &&
        ast_call_semantic_callee_declared_callable(call) &&
        ast_call_semantic_callee_value_binding_id(call) == 0 &&
        semantic_host_index_selected_call_formal(ctx, call, decl, receiver_type,
            callable_capability_current_declaration(ctx), actual, formal);
}

bool semantic_param_publication_ref_call_diagnostic_current(
    const SemanticContext *ctx, const PendingRefCall *row, size_t *formal)
{
    if (formal == NULL || row == NULL || !semantic_param_publication_inputs_ready(ctx) ||
        row->call == NULL || row->call->type != AST_CALL || row->site == NULL ||
        row->callee == NULL || row->callee->type != AST_FUNC_DECL ||
        row->type == NULL || row->type == TYPE_UNKNOWN ||
        row->call_id == 0 || row->call_id != ast_node_stable_id(row->call) ||
        row->site_id == 0 || row->site_id != ast_node_stable_id(row->site) ||
        row->site_shape != row->site->type ||
        row->site_binding_id != ast_identifier_binding_syntax_id(row->site) ||
        row->callee_id == 0 || row->callee_id != ast_node_stable_id(row->callee) ||
        row->ordinal >= ast_call_arg_count(row->call) ||
        row->argument_count != ast_call_arg_count(row->call) ||
        ast_call_argument(row->call, row->ordinal) != row->site ||
        !ast_call_semantic_callee_declared_callable(row->call) ||
        ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
        ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
        row->call_target == NULL || row->call_target != ast_call_callee(row->call) ||
        row->call_target_id == 0 || row->call_target_id != ast_node_stable_id(row->call_target) ||
        row->target_shape != row->call_target->type ||
        row->target_binding_id != ast_identifier_binding_syntax_id(row->call_target)) return false;
    const char *name = row->target_shape == AST_IDENTIFIER ? ast_identifier_name(row->call_target) :
        row->target_shape == AST_MEMBER_ACCESS ? ast_member_name(row->call_target) : NULL;
    if (name == NULL || row->target_name == NULL || strcmp(name, row->target_name) != 0 ||
        row->call_receiver != (row->target_shape == AST_MEMBER_ACCESS ? ast_member_object(row->call_target) : NULL) ||
        row->call_receiver_id != ast_node_stable_id(row->call_receiver)) return false;
    PublishedNode *selected = publication_node(ctx->function_param_publications, row->call, false);
    PendingRoutine *callee = publication_routine(ctx->function_param_publications, row->callee_id);
    PublishedNode *receiver = publication_current_value(ctx, row->call_receiver);
    const Type *receiver_type = receiver != NULL && !receiver->meta ? receiver->value_type : NULL;
    if (selected == NULL || selected->selected_callable != row->callee ||
        selected->selected_callable_id != row->callee_id || callee == NULL ||
        callee->decl != row->callee || callee->function_type == NULL ||
        (callee->count != 0 && (callee->types == NULL || callee->formals == NULL)) ||
        !semantic_param_publication_routine_inputs_current(ctx, row->callee, false) ||
        receiver_type != row->call_receiver_type) return false;
    if (row->caller != NULL) {
        PendingRoutine *caller = publication_routine(ctx->function_param_publications, row->caller_id);
        if (row->caller_id == 0 || caller == NULL || caller->decl != row->caller ||
            caller->function_type == NULL ||
            (caller->count != 0 && (caller->types == NULL || caller->formals == NULL)) ||
            !semantic_param_publication_routine_inputs_current(ctx, row->caller, true) ||
            (row->module == NULL) != (caller->module == NULL) ||
            (row->module != NULL && strcmp(row->module, caller->module) != 0)) return false;
    } else if (row->caller_id != 0) return false;
    size_t index = 0;
    if (!semantic_host_index_selected_call_formal((SemanticContext *)ctx, row->call,
            row->callee, receiver_type, row->caller, row->ordinal, &index) ||
        index != row->formal_ordinal || index >= callee->count ||
        row->formal_id == 0 || row->formal_id != callee->formals[index].id ||
        row->formal_id != ast_func_param_stable_id(ast_func_param(row->callee, index)) ||
        callee->formals[index].mode != PARAM_MODE_REF) return false;
    PublishedNode *actual = publication_node(ctx->function_param_publications, row->site, false);
    if (actual != NULL && actual->value_present &&
        (actual->value_type == NULL || actual->value_shape != row->site_shape ||
            actual->binding_id != row->site_binding_id ||
            !type_equals(actual->value_type, row->type))) return false;
    *formal = index;
    return true;
}

static const char *
host_decl_index_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;
    /* LET and lifecycle also have declaration names, but never enter this
     * callable/type index. The AST accessor owns every other name here. */
    if (decl->type == AST_LET_DECL || decl->type == AST_LIFECYCLE_DECL)
        return NULL;
    return decl->type == AST_INTENT_DECL
        ? ast_intent_decl_name(decl) : ast_declaration_name(decl);
}

static bool
host_decl_index_reserve(SemanticContext *ctx, size_t next_count)
{
    size_t new_cap;
    ASTNode **new_decls;
    const char **new_names;
    ASTNodeType *new_types;

    if (ctx == NULL)
        return false;
    if (next_count <= ctx->host_decl_index.capacity)
        return true;
    if (ctx->host_decl_index.capacity > (size_t)-1 / 2)
        return false;

    new_cap = ctx->host_decl_index.capacity == 0
        ? 32
        : ctx->host_decl_index.capacity * 2;
    while (new_cap < next_count) {
        if (new_cap > (size_t)-1 / 2)
            return false;
        new_cap *= 2;
    }
    if (new_cap > (size_t)-1 / sizeof(ASTNode *))
        return false;

    new_decls = calloc(new_cap, sizeof(ASTNode *));
    new_names = calloc(new_cap, sizeof(const char *));
    new_types = calloc(new_cap, sizeof(ASTNodeType));
    if (new_decls == NULL || new_names == NULL || new_types == NULL) {
        free(new_decls);
        free(new_names);
        free(new_types);
        return false;
    }

    if (ctx->host_decl_index.count > 0) {
        memcpy(new_decls, ctx->host_decl_index.decls,
               ctx->host_decl_index.count * sizeof(ASTNode *));
        memcpy(new_names, ctx->host_decl_index.names,
               ctx->host_decl_index.count * sizeof(const char *));
        memcpy(new_types, ctx->host_decl_index.types,
               ctx->host_decl_index.count * sizeof(ASTNodeType));
    }

    free(ctx->host_decl_index.decls);
    free(ctx->host_decl_index.names);
    free(ctx->host_decl_index.types);
    ctx->host_decl_index.decls = new_decls;
    ctx->host_decl_index.names = new_names;
    ctx->host_decl_index.types = new_types;
    ctx->host_decl_index.capacity = new_cap;
    return true;
}

static bool
host_decl_index_append(SemanticContext *ctx, ASTNode *decl)
{
    const char *name = host_decl_index_name(decl);
    size_t index;

    if (ctx == NULL || decl == NULL || name == NULL)
        return true;
    if (!host_decl_index_reserve(ctx, ctx->host_decl_index.count + 1))
        return false;

    index = ctx->host_decl_index.count++;
    ctx->host_decl_index.decls[index] = decl;
    ctx->host_decl_index.names[index] = name;
    ctx->host_decl_index.types[index] = decl->type;
    return true;
}

static bool
host_decl_index_append_nested_callable(SemanticContext *ctx, ASTNode *decl,
                                       ASTNode *alias_owner)
{
    SemanticNestedCallable *next;
    size_t capacity;
    if (decl == NULL || decl->type != AST_FUNC_DECL) return true;
    if (ast_node_stable_id(decl) == 0 ||
        (alias_owner != NULL && ast_node_stable_id(alias_owner) == 0)) return false;
    if (ctx->host_decl_index.nested_count == ctx->host_decl_index.nested_capacity) {
        capacity = ctx->host_decl_index.nested_capacity == 0
            ? 16 : ctx->host_decl_index.nested_capacity * 2;
        if (capacity < ctx->host_decl_index.nested_capacity ||
            capacity > SIZE_MAX / sizeof(*next)) return false;
        next = realloc(ctx->host_decl_index.nested_callables,
                       capacity * sizeof(*next));
        if (next == NULL) return false;
        ctx->host_decl_index.nested_callables = next;
        ctx->host_decl_index.nested_capacity = capacity;
    }
    ctx->host_decl_index.nested_callables[ctx->host_decl_index.nested_count++] =
        (SemanticNestedCallable){decl, alias_owner, ast_node_stable_id(decl),
            ast_node_stable_id(alias_owner)};
    return true;
}

static int
host_nested_callable_compare(const void *left, const void *right)
{
    uint32_t a = ((const SemanticNestedCallable *)left)->id;
    uint32_t b = ((const SemanticNestedCallable *)right)->id;
    return (a > b) - (a < b);
}

/* FNV-1a over the name, mixed with the decl type. Must be byte-identical
 * between build and lookup. */
static size_t
host_decl_key_hash(ASTNodeType type, const char *name)
{
    size_t h = 1469598103934665603ULL;
    const unsigned char *p;

    for (p = (const unsigned char *)name; *p != '\0'; p++) {
        h ^= (size_t)*p;
        h *= 1099511628211ULL;
    }
    h ^= (size_t)type * 2654435761ULL;
    return h;
}

static void
host_decl_index_free_hash(SemanticContext *ctx)
{
    free(ctx->host_decl_index.hash);
    ctx->host_decl_index.hash = NULL;
    ctx->host_decl_index.hash_capacity = 0;
}

/* Best-effort: on OOM the hash stays NULL and lookups fall back to the linear
 * scan, so failure here is slow, not wrong. Keeps the FIRST index for a given
 * (type, name) to match the linear scan's first-match semantics. */
static void
host_decl_index_build_hash(SemanticContext *ctx)
{
    size_t count = ctx->host_decl_index.count;
    size_t cap = 16;
    size_t *hash;
    size_t mask;

    host_decl_index_free_hash(ctx);
    if (count == 0)
        return;
    if (count > ((size_t)-1) / 4)
        return;  /* pathological; keep linear */
    while (cap < count * 2)
        cap *= 2;
    hash = calloc(cap, sizeof(size_t));
    if (hash == NULL)
        return;
    mask = cap - 1;

    for (size_t i = 0; i < count; i++) {
        const char *nm = ctx->host_decl_index.names[i];
        size_t slot;

        if (nm == NULL)
            continue;
        slot = host_decl_key_hash(ctx->host_decl_index.types[i], nm) & mask;
        for (size_t probe = 0; probe < cap; probe++) {
            size_t entry = hash[slot];
            size_t j;

            if (entry == 0) {
                hash[slot] = i + 1;
                break;
            }
            j = entry - 1;
            if (ctx->host_decl_index.types[j] == ctx->host_decl_index.types[i]
                && ctx->host_decl_index.names[j] != NULL
                && strcmp(ctx->host_decl_index.names[j], nm) == 0) {
                break;  /* duplicate (type, name): keep the first index */
            }
            slot = (slot + 1) & mask;
        }
    }
    ctx->host_decl_index.hash = hash;
    ctx->host_decl_index.hash_capacity = cap;
}

bool
semantic_build_host_decl_index(SemanticContext *ctx, ASTNode *program)
{
    if (ctx == NULL || program == NULL || program->type != AST_PROGRAM)
        return false;

    ctx->host_decl_index.count = 0;
    ctx->host_decl_index.nested_count = 0;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *decl = ast_program_statement(program, i);
        ASTNode **nested = NULL;
        size_t count = 0;
        ASTNode *alias_owner = NULL;
        if (!host_decl_index_append(ctx, decl))
            return false;
        if (decl == NULL) continue;
        if (decl->type == AST_EXTERN_BLOCK) {
            nested = ast_extern_block_declarations(decl, &count);
            alias_owner = decl;
        } else if (decl->type != AST_ROLE_DECL) {
            nested = semantic_host_decl_methods(decl, &count);
            alias_owner = decl;
        }
        if (count != 0 && nested == NULL) return false;
        for (size_t j = 0; j < count; j++)
            if (!host_decl_index_append_nested_callable(ctx,
                    nested[j], alias_owner)) return false;
        if (decl->type == AST_ROLE_DECL) {
            for (size_t j = 0; j < ast_role_impl_count(decl); j++) {
                ASTNode *impl = ast_role_impl(decl, j);
                if (impl == NULL) return false;
                if (impl->type == AST_OVERRIDE_FUNC) {
                    if (!host_decl_index_append_nested_callable(ctx,
                            ast_override_func_decl(impl), decl)) return false;
                } else if (impl->type == AST_IMPL_ABILITY) {
                    for (size_t k = 0; k < ast_impl_ability_method_count(impl); k++)
                        if (!host_decl_index_append_nested_callable(ctx,
                                ast_impl_ability_method(impl, k), decl)) return false;
                }
            }
        }
    }
    if (ctx->host_decl_index.nested_count > 1)
        qsort(ctx->host_decl_index.nested_callables,
              ctx->host_decl_index.nested_count,
              sizeof(*ctx->host_decl_index.nested_callables),
              host_nested_callable_compare);
    for (size_t i = 1; i < ctx->host_decl_index.nested_count; i++)
        if (ctx->host_decl_index.nested_callables[i - 1].id ==
            ctx->host_decl_index.nested_callables[i].id) return false;
    host_decl_index_build_hash(ctx);
    return true;
}

ASTNode *semantic_param_publication_direct_callee(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return NULL;
    PublishedNode *node = publication_node(ctx->function_param_publications, call, false);
    if (node == NULL || node->declaration == 0 || node->value_binding != 0 ||
        node->selected_callable == NULL || node->selected_callable_id != node->declaration) return NULL;
    PendingRoutine *row = publication_routine(ctx->function_param_publications, node->declaration);
    PublishedNode *receiver = publication_current_value(ctx, node->call_receiver);
    const Type *receiver_type = receiver != NULL && !receiver->meta ? receiver->value_type : NULL;
    size_t last = 0;
    if (row == NULL || row->decl != node->selected_callable ||
        !semantic_host_index_selected_call_formal((SemanticContext *)ctx, call, row->decl,
            receiver_type, node->caller, node->argument_count, &last) ||
        last != row->count ||
        !semantic_param_publication_routine_inputs_current(ctx, row->decl, true)) return NULL;
    return row->decl;
}
/* The intent a checked call selects; NULL for every other call. */
ASTNode *semantic_param_publication_intent_callee(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return NULL;
    PublishedNode *node = publication_node(ctx->function_param_publications, call, false);
    if (node == NULL || !node->declared_callable || node->declaration == 0 || node->value_binding != 0 ||
        node->selected_callable != NULL || node->call_receiver != NULL || node->target_name == NULL) return NULL;
    ASTNode *decl = semantic_host_index_find_selected_callable((SemanticContext *)ctx, SYMBOL_INTENT,
        node->declaration, node->target_name);
    return decl != NULL && decl->type == AST_INTENT_DECL ? decl : NULL;
}
bool semantic_param_publication_bodyless_selected_call_current(
    const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *node = publication_result_current(ctx, call);
    if (node == NULL || !node->declared_callable || node->value_binding != 0 ||
        node->selected_callable == NULL || node->selected_callable_id != node->declaration) return false;
    PendingRoutine *row = publication_routine(ctx->function_param_publications, node->declaration);
    if (row == NULL || row->decl != node->selected_callable || row->id != node->declaration ||
        row->function_type == NULL || (row->count != 0 && (row->types == NULL || row->formals == NULL)) ||
        !semantic_param_publication_routine_inputs_current(ctx, row->decl, false) ||
        row->body != NULL || row->body_id != 0 || ast_func_body(row->decl) != NULL ||
        row->normal_exit_closed || row->async != row->decl->is_async_decl ||
        row->return_type == NULL || row->return_type == TYPE_UNKNOWN ||
        row->return_type != type_function_return_type(row->function_type) ||
        row->return_node != ast_func_return_type(row->decl) ||
        row->return_node_id != ast_node_stable_id(row->return_node)) return false;
    PublishedNode *receiver = publication_current_value(ctx, node->call_receiver);
    const Type *receiver_type = receiver != NULL && !receiver->meta ? receiver->value_type : NULL;
    size_t last = 0;
    if (!semantic_host_index_selected_call_formal((SemanticContext *)ctx, call, row->decl,
        receiver_type, node->caller, node->argument_count, &last) || last != row->count) return false;
    for (size_t p = 0; p < row->count; p++)
        if (row->types[p] == NULL || row->types[p] == TYPE_UNKNOWN ||
            row->formals[p].kind != row->types[p]->kind ||
            row->formals[p].anchored != type_is_anchored_resource_handle(row->types[p])) return false;
    size_t offset = last - node->argument_count;
    for (size_t a = 0; a < node->argument_count; a++) {
        size_t formal = a + offset;
        if (formal >= row->count || node->arguments[a].formal_ordinal != formal ||
            node->arguments[a].formal_id != row->formals[formal].id) return false;
    }
    return true;
}
bool semantic_param_publication_instance_receiver(const SemanticContext *ctx,
    const ASTNode *call, ASTNode **receiver_out)
{
    if (receiver_out == NULL) return false;
    *receiver_out = NULL;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    if (decl == NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || row->target == NULL) return false;
    PublishedNode *value = publication_current_value(ctx, row->call_receiver);
    const Type *receiver_type = value != NULL && !value->meta ? value->value_type : NULL;
    size_t first = 0;
    if (!semantic_host_index_selected_call_formal((SemanticContext *)ctx, call, decl,
            receiver_type, row->caller, 0, &first)) return false;
    if (first == 0) return true; /* Static/namespace qualifier is not evaluated as a value. */
    if (first != 1 || row->target_shape != AST_MEMBER_ACCESS || value == NULL ||
        row->call_receiver != ast_member_object(row->target) || row->call_receiver_id == 0 ||
        row->call_receiver_id != ast_node_stable_id(row->call_receiver)) return false;
    *receiver_out = row->call_receiver;
    return true;
}
bool semantic_param_publication_formal_for_actual(const SemanticContext *ctx,
    const ASTNode *call, size_t actual, size_t *formal)
{
    if (formal == NULL || !semantic_param_publication_checked_argument(ctx, call, actual)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    ASTNode *decl = row->selected_callable;
    if (decl == NULL || (!semantic_param_publication_bodyless_selected_call_current(ctx, call) && semantic_param_publication_direct_callee(ctx, call) != decl)) return false;
    PublishedNode *receiver = publication_current_value(ctx, row->call_receiver);
    const Type *type = receiver != NULL && !receiver->meta ? receiver->value_type : NULL;
    size_t index = 0;
    if (!semantic_host_index_selected_call_formal((SemanticContext *)ctx, call, decl,
            type, row->caller, actual, &index) ||
        row->arguments[actual].formal_ordinal != index ||
        row->arguments[actual].formal_id != ast_func_param_stable_id(
            ast_func_param(decl, index))) return false;
    *formal = index;
    return true;
}

ASTNode *semantic_param_publication_callee(const SemanticContext *ctx,
    const ASTNode *call, size_t ordinal)
{
    size_t formal = 0;
    if (!semantic_param_publication_formal_for_actual(ctx, call, ordinal, &formal)) return NULL;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    return decl != NULL && formal < ast_func_param_count(decl) ? decl : NULL;
}

ASTNode *
semantic_host_index_find_decl_by_name(SemanticContext *ctx,
                                      ASTNodeType decl_type,
                                      const char *name)
{
    if (ctx == NULL || name == NULL || ctx->host_decl_index.count == 0)
        return NULL;

    if (ctx->host_decl_index.hash != NULL
        && ctx->host_decl_index.hash_capacity > 0) {
        size_t mask = ctx->host_decl_index.hash_capacity - 1;
        size_t slot = host_decl_key_hash(decl_type, name) & mask;

        for (size_t probe = 0; probe < ctx->host_decl_index.hash_capacity;
             probe++) {
            size_t entry = ctx->host_decl_index.hash[slot];
            size_t idx;

            if (entry == 0)
                return NULL;  /* empty slot: key absent */
            idx = entry - 1;
            if (ctx->host_decl_index.types[idx] == decl_type
                && ctx->host_decl_index.names[idx] != NULL
                && strcmp(ctx->host_decl_index.names[idx], name) == 0) {
                return ctx->host_decl_index.decls[idx];
            }
            slot = (slot + 1) & mask;
        }
        return NULL;
    }

    /* Hash not built (OOM or empty): linear fallback. */
    for (size_t i = 0; i < ctx->host_decl_index.count; i++) {
        if (ctx->host_decl_index.types[i] == decl_type
            && ctx->host_decl_index.names[i] != NULL
            && strcmp(ctx->host_decl_index.names[i], name) == 0) {
            return ctx->host_decl_index.decls[i];
        }
    }
    return NULL;
}

ASTNode *
semantic_host_index_find_top_level_decl_by_label(SemanticContext *ctx,
                                                 const char *label,
                                                 TypeResolutionNodeKind kind)
{
    if (ctx == NULL || label == NULL || ctx->host_decl_index.count == 0)
        return NULL;

    for (size_t i = 0; i < ctx->host_decl_index.count; i++) {
        TypeResolutionNodeKind entry_kind =
            ctx->host_decl_index.types[i] == AST_TYPE_ALIAS
                ? TYPE_RES_NODE_ALIAS
                : TYPE_RES_NODE_DECL;
        if (entry_kind == kind
            && ctx->host_decl_index.names[i] != NULL
            && strcmp(ctx->host_decl_index.names[i], label) == 0) {
            return ctx->host_decl_index.decls[i];
        }
    }
    return NULL;
}

ASTNode *
semantic_host_index_find_next_decl_of_type(SemanticContext *ctx,
                                           ASTNodeType decl_type,
                                           const ASTNode *after)
{
    bool past_after = after == NULL;

    if (ctx == NULL || ctx->host_decl_index.count == 0)
        return NULL;

    for (size_t i = 0; i < ctx->host_decl_index.count; i++) {
        ASTNode *decl = ctx->host_decl_index.decls[i];
        if (ctx->host_decl_index.types[i] != decl_type)
            continue;
        if (!past_after) {
            if (decl == after)
                past_after = true;
            continue;
        }
        return decl;
    }
    return NULL;
}
