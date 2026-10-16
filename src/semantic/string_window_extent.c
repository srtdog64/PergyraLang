/* String-window extent owner. Rule and proof forms P1-P8 are fixed in
 * docs/agent_work_directives/string_window_extent_closure_2026-10-10.md.
 *
 * Identity is the declaration syntax id recorded on every identifier, never
 * a spelling. A binding is stable when it is a routine parameter or a plain
 * let binding, no assignment anywhere in the program targets it, and no call
 * passes it to an inout parameter. Loop, match and destructured bindings and
 * host fields are never stable. The facts are program-wide, so a binding read from two routines
 * gets one answer. Builtins never rebind an Int or String binding: their
 * inout parameters are collection receivers.
 *
 * Reads of one element of a readonly `ref` array parameter (or a field path
 * over one) at the same stable index prove the element's extent in place
 * (P8): nothing writes that array's elements while the routine runs.
 *
 * A record field pair (f, g) is an invariant keyed by the two field names:
 * every construction of every record declaring both fields proves its pair,
 * and no write anywhere targets a field of either name. Keying by name, not
 * by record type, over-approximates and needs no expression types. */
#include "string_window_extent_internal.h"

#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

/* Grow a record array by one slot; false leaves it unchanged. */
bool
swe_reserve(void **items, size_t *capacity, size_t count, size_t item_size)
{
    if (count < *capacity)
        return true;
    size_t next = *capacity == 0 ? 16 : *capacity * 2;
    void *grown = realloc(*items, next * item_size);
    if (grown == NULL)
        return false;
    *items = grown;
    *capacity = next;
    return true;
}


StringWindowExtentStore *
swe_store(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->string_window_extents : NULL;
}

void
semantic_string_window_extent_destroy(StringWindowExtentStore *store)
{
    if (store == NULL)
        return;
    for (size_t i = 0; i < store->routine_count; i++)
        free(store->routines[i].requirements);
    free(store->routines);
    free(store->calls);
    free(store->constructors);
    free(store->lets);
    free(store->mutated);
    free(store->inout_passed);
    free(store->parameters);
    free(store->field_writes);
    free(store->field_requirements);
    free(store->function_values);
    free(store);
}

bool
semantic_string_window_extent_begin(SemanticContext *ctx)
{
    if (ctx == NULL)
        return false;
    semantic_string_window_extent_destroy(ctx->string_window_extents);
    ctx->string_window_extents = calloc(1, sizeof(*ctx->string_window_extents));
    return ctx->string_window_extents != NULL;
}

void
semantic_string_window_extent_record_routine(SemanticContext *ctx,
                                             ASTNode *func_decl)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return;
    if (!swe_reserve((void **)&store->routines, &store->routine_capacity,
                     store->routine_count, sizeof(*store->routines))) {
        store->allocation_failed = true;
        return;
    }
    SweRoutine *routine = &store->routines[store->routine_count++];
    memset(routine, 0, sizeof(*routine));
    routine->decl_id = ast_node_stable_id(func_decl);
    routine->node = func_decl;
    /* A method's call arguments do not carry its receiver in the same
     * positions as its declaration, so a method may not carry a requirement. */
    routine->is_method = current_host_decl(ctx) != NULL
        || ctx->current_nominal_decl != NULL || ctx->current_party != NULL;
    for (size_t i = 0; i < ast_func_param_count(func_decl); i++)
        swe_push_id(store, &store->parameters, &store->parameter_count,
                    &store->parameter_capacity,
                    ast_func_param_stable_id(ast_func_param(func_decl, i)));
}

void
semantic_string_window_extent_record_let(SemanticContext *ctx,
                                         ASTNode *let_decl)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || let_decl == NULL)
        return;
    if (!swe_reserve((void **)&store->lets, &store->let_capacity,
                     store->let_count, sizeof(*store->lets))) {
        store->allocation_failed = true;
        return;
    }
    store->lets[store->let_count].decl_id = ast_node_stable_id(let_decl);
    store->lets[store->let_count].node = let_decl;
    store->let_count++;
}

void
swe_push_id(StringWindowExtentStore *store, uint32_t **items, size_t *count,
            size_t *capacity, uint32_t decl_id)
{
    if (store == NULL || decl_id == 0)
        return;
    if (!swe_reserve((void **)items, capacity, *count, sizeof(**items))) {
        store->allocation_failed = true;
        return;
    }
    (*items)[(*count)++] = decl_id;
}

static void
swe_mark_mutated(StringWindowExtentStore *store, uint32_t decl_id)
{
    swe_push_id(store, &store->mutated, &store->mutated_count,
                &store->mutated_capacity, decl_id);
}

static void
swe_mark_field_write(StringWindowExtentStore *store, const char *name,
                     ASTNode *owner, ASTNode *site)
{
    if (store == NULL || name == NULL)
        return;
    if (!swe_reserve((void **)&store->field_writes, &store->field_write_capacity,
                     store->field_write_count, sizeof(*store->field_writes))) {
        store->allocation_failed = true;
        return;
    }
    store->field_writes[store->field_write_count].name = name;
    store->field_writes[store->field_write_count].owner = owner;
    store->field_writes[store->field_write_count].site = site;
    store->field_write_count++;
}

/* The identifier a place expression writes through, if any. */
ASTNode *
swe_root_identifier(ASTNode *node)
{
    while (node != NULL) {
        switch (node->type) {
        case AST_IDENTIFIER:
            return node;
        case AST_MEMBER_ACCESS:
            node = ast_member_object(node);
            break;
        case AST_ARRAY_ACCESS:
            node = ast_array_access_array(node);
            break;
        default:
            return NULL;
        }
    }
    return NULL;
}

/* The record declaring the field that `object.field` names, when `object`
 * is a binding whose type is a program record; NULL otherwise. */
static ASTNode *
swe_member_owner(SemanticContext *ctx, ASTNode *object)
{
    if (ctx == NULL || object == NULL || object->type != AST_IDENTIFIER)
        return NULL;
    Symbol *sym = lookup_identifier_symbol(object, ctx);
    const char *type_name = sym != NULL && sym->type != NULL ? sym->type->name : NULL;
    return type_name != NULL ? semantic_find_class_decl_by_name(ctx, type_name) : NULL;
}

/* The field a place expression writes: its last member, or a bare host
 * field. Writing `a.b.c` replaces only field `c` of `a.b`. */
void
swe_mark_place_fields(SemanticContext *ctx, StringWindowExtentStore *store,
                      ASTNode *place, ASTNode *site)
{
    if (place == NULL)
        return;
    if (place->type == AST_MEMBER_ACCESS) {
        swe_mark_field_write(store, ast_member_name(place),
                             swe_member_owner(ctx, ast_member_object(place)), site);
        return;
    }
    if (place->type == AST_IDENTIFIER && ast_identifier_binding_is_host_field(place))
        swe_mark_field_write(store, ast_identifier_name(place),
                             ctx != NULL ? current_host_decl(ctx) : NULL, site);
}

void
semantic_string_window_extent_record_assignment(SemanticContext *ctx,
                                                ASTNode *target)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || target == NULL)
        return;
    if (target->type == AST_TUPLE_LITERAL) {
        for (size_t i = 0; i < ast_tuple_literal_count(target); i++)
            semantic_string_window_extent_record_assignment(
                ctx, ast_tuple_literal_element(target, i));
        return;
    }
    ASTNode *root = swe_root_identifier(target);
    if (root == NULL)
        return;
    Symbol *sym = lookup_identifier_symbol(root, ctx);
    swe_mark_mutated(store, sym != NULL ? sym->decl_syntax_id
                                        : ast_identifier_binding_syntax_id(root));
    swe_mark_place_fields(ctx, store, target, target);
}

void
semantic_string_window_extent_record_call(SemanticContext *ctx, ASTNode *call)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || call == NULL || call->type != AST_CALL)
        return;
    if (!swe_reserve((void **)&store->calls, &store->call_capacity,
                     store->call_count, sizeof(*store->calls))) {
        store->allocation_failed = true;
        return;
    }
    store->calls[store->call_count].call = call;
    store->calls[store->call_count].routine = ctx->current_function_decl;
    store->call_count++;
}

void
semantic_string_window_extent_record_constructor(SemanticContext *ctx,
                                                 ASTNode *call, ASTNode *decl)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || call == NULL || decl == NULL
        || decl->type != AST_CLASS_DECL)
        return;
    if (!swe_reserve((void **)&store->constructors,
                     &store->constructor_capacity, store->constructor_count,
                     sizeof(*store->constructors))) {
        store->allocation_failed = true;
        return;
    }
    store->constructors[store->constructor_count].call = call;
    store->constructors[store->constructor_count].routine = ctx->current_function_decl;
    store->constructors[store->constructor_count].decl = decl;
    store->constructor_count++;
}

void
semantic_string_window_extent_record_function_value(SemanticContext *ctx,
                                                    ASTNode *identifier,
                                                    uint32_t decl_id)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || identifier == NULL || decl_id == 0)
        return;
    if (!swe_reserve((void **)&store->function_values,
                     &store->function_value_capacity,
                     store->function_value_count,
                     sizeof(*store->function_values))) {
        store->allocation_failed = true;
        return;
    }
    store->function_values[store->function_value_count].identifier = identifier;
    store->function_values[store->function_value_count].decl_id = decl_id;
    store->function_value_count++;
}

/* ---- finalization ---------------------------------------------------- */
