/* Liveness of a String MapGet returned from a HashMap<K, String>; see the
 * header. Symbol pointers are used only for transient scope lookup; the
 * identities are SyntaxNodeIds carried by the bindings. */

#include <stdlib.h>
#include <string.h>
#include "diag_codes.h"
#include "map_string_payload_view_owner.h"
#include "indexed_string_borrow_owner.h"
#include "type_checker_internal.h"

#define MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE UINT32_MAX
#define MAP_PAYLOAD_VIEW_MAX_DEPTH 64

static bool
type_is_string_payload_map(const Type *type)
{
    Type *value;
    if (type == NULL || !type_is_constructed_named((Type *)type, "HashMap")
        || type_constructed_arg_count((Type *)type) != 2)
        return false;
    value = type_constructed_arg((Type *)type, 1);
    return value != NULL && type_equals(value, TYPE_STRING);
}

/* A String, or a value that can hold one (Array, Option, tuple, struct).
 * A map holds its own copies of the Strings it is given. */
static bool
type_can_carry_view(const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN)
        return false;
    if (type_equals((Type *)type, TYPE_STRING))
        return true;
    if (type_is_constructed_named((Type *)type, "HashMap"))
        return false;
    return type->kind == TYPE_KIND_CONSTRUCTED || type->kind == TYPE_KIND_TUPLE
        || type->kind == TYPE_KIND_CLASS;
}

static Symbol *
identifier_symbol(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL || node->type != AST_IDENTIFIER)
        return NULL;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(node));
    return binding != NULL && binding->decl_syntax_id != 0
        && binding->decl_syntax_id == ast_identifier_binding_syntax_id(node)
        ? binding : NULL;
}

static uint32_t
map_storage_source(const Symbol *map)
{
    return map->map_storage_source_syntax_id != 0
        ? map->map_storage_source_syntax_id : map->decl_syntax_id;
}

static uint32_t
view_join(uint32_t current, uint32_t incoming)
{
    if (current == 0)
        return incoming;
    if (incoming == 0 || current == incoming)
        return current;
    return MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE;
}

static bool
stdlib_call_named(ASTNode *call, const char *name)
{
    ASTNode *callee = call != NULL && call->type == AST_CALL
        ? ast_call_callee(call) : NULL;
    return callee != NULL && callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL
        && strcmp(ast_identifier_name(callee), name) == 0
        && ast_call_semantic_callee_is_stdlib(call);
}

/* The map storage a value may point into, 0 for none. A call that is not a
 * known copy may return any String argument it was given. */
static uint32_t
view_source_of(ASTNode *expression, SemanticContext *ctx, int depth)
{
    uint32_t source = 0;
    if (expression == NULL || depth > MAP_PAYLOAD_VIEW_MAX_DEPTH)
        return expression == NULL ? 0 : MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE;
    switch (expression->type) {
    case AST_IDENTIFIER: {
        Symbol *binding = identifier_symbol(expression, ctx);
        return binding != NULL ? binding->map_payload_view_source_syntax_id : 0;
    }
    case AST_CALL: {
        if (stdlib_call_named(expression, "MapGet")) {
            Symbol *map = ast_call_arg_count(expression) == 2
                ? identifier_symbol(ast_call_argument(expression, 0), ctx) : NULL;
            if (map == NULL)
                return MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE;
            return type_is_string_payload_map(map->type) ? map_storage_source(map) : 0;
        }
        if (stdlib_call_named(expression, "Clone")
            || semantic_indexed_string_independent_result(expression, ctx))
            return 0;
        for (size_t i = 0; i < ast_call_arg_count(expression); i++)
            source = view_join(source, view_source_of(ast_call_argument(expression, i), ctx, depth + 1));
        return source;
    }
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(expression); i++)
            source = view_join(source, view_source_of(ast_array_literal_element(expression, i), ctx, depth + 1));
        return source;
    case AST_MEMBER_ACCESS:
        return view_source_of(ast_member_object(expression), ctx, depth + 1);
    case AST_ARRAY_ACCESS:
        return view_source_of(ast_array_access_array(expression), ctx, depth + 1);
    default:
        return 0;
    }
}

/* The storage a map value names: an alias shares it, MapNew or a call result
 * is fresh, anything else (a field or element path) is unknown. */
static uint32_t
map_storage_of_value(ASTNode *value, uint32_t fresh, SemanticContext *ctx)
{
    Symbol *source = identifier_symbol(value, ctx);
    if (source != NULL && type_is_string_payload_map(source->type))
        return map_storage_source(source);
    if (value != NULL && value->type == AST_CALL)
        return fresh;
    return MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE;
}

void
semantic_map_payload_view_initialize_binding(Symbol *binding,
    ASTNode *initializer, SemanticContext *ctx)
{
    if (binding == NULL || ctx == NULL)
        return;
    binding->map_payload_view_invalidated = false;
    binding->map_payload_view_source_syntax_id = 0;
    if (type_is_string_payload_map(binding->type)) {
        binding->map_storage_source_syntax_id = initializer == NULL
            ? binding->decl_syntax_id
            : map_storage_of_value(initializer, binding->decl_syntax_id, ctx);
        return;
    }
    if (type_can_carry_view(binding->type))
        binding->map_payload_view_source_syntax_id = view_source_of(initializer, ctx, 0);
}

static void
log_change(uint32_t source, ASTNode *site, SemanticContext *ctx)
{
    if (ctx->map_payload_view_change_count == ctx->map_payload_view_change_capacity) {
        size_t capacity = ctx->map_payload_view_change_capacity == 0
            ? 16 : ctx->map_payload_view_change_capacity * 2;
        uint32_t *grown = realloc(ctx->map_payload_view_changes, capacity * sizeof(*grown));
        if (grown == NULL) {
            semantic_error(ctx, site, "HashMap String value change record allocation failed");
            return;
        }
        ctx->map_payload_view_changes = grown;
        ctx->map_payload_view_change_capacity = capacity;
    }
    ctx->map_payload_view_changes[ctx->map_payload_view_change_count++] = source;
}

static bool
sources_overlap(uint32_t a, uint32_t b)
{
    return a != 0 && b != 0 && (a == b || a == MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE
        || b == MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE);
}

static void
invalidate_source(uint32_t source, ASTNode *site, SemanticContext *ctx)
{
    log_change(source, site, ctx);
    for (Scope *scope = ctx->scope; scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *candidate = scope->symbols[i];
            if (candidate != NULL && !candidate->map_payload_view_invalidated
                && sources_overlap(candidate->map_payload_view_source_syntax_id, source)) {
                candidate->map_payload_view_invalidated = true;
                ctx->map_payload_view_invalidation_count++;
            }
        }
    }
}

void
semantic_map_payload_view_change(ASTNode *receiver, Type *map_type,
    SemanticContext *ctx)
{
    Symbol *map = identifier_symbol(receiver, ctx);
    if (map_type == NULL && map != NULL)
        map_type = map->type;
    if (ctx == NULL || !type_is_string_payload_map(map_type))
        return;
    invalidate_source(map != NULL ? map_storage_source(map)
        : MAP_PAYLOAD_VIEW_UNKNOWN_SOURCE, receiver, ctx);
}

void
semantic_map_payload_view_record_assignment(Symbol *target,
    ASTNode *value, ASTNode *assignment, SemanticContext *ctx)
{
    if (target == NULL || ctx == NULL)
        return;
    if (type_is_string_payload_map(target->type)) {
        /* The old storage is released (or, for an inout formal, the caller's
         * map is replaced): its values are gone. */
        invalidate_source(map_storage_source(target), assignment, ctx);
        target->map_storage_source_syntax_id = map_storage_of_value(value,
            ast_node_stable_id(assignment), ctx);
        return;
    }
    if (type_can_carry_view(target->type))
        target->map_payload_view_source_syntax_id = view_join(
            target->map_payload_view_source_syntax_id, view_source_of(value, ctx, 0));
}

bool
semantic_map_payload_view_validate_use(Symbol *binding, ASTNode *use,
    SemanticContext *ctx)
{
    if (binding == NULL || !binding->map_payload_view_invalidated
        || binding->map_payload_view_source_syntax_id == 0)
        return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, use,
        "'%s' holds a String MapGet returned from a HashMap value that was replaced or removed.\n"
        "Reason:\n"
        "- the map owns its String values; MapSet replaces and MapRemove frees the value the view points at\n"
        "- reading the view afterwards would read freed memory\n"
        "Fix:\n"
        "- copy the value with Concat(\"\", MapGet(map, key)) before the map changes\n"
        "- or finish reading the value before the map changes",
        binding->name != NULL ? binding->name : "<value>");
    return false;
}

void
semantic_map_payload_view_call_begin(MapPayloadViewCallFrame *frame)
{
    if (frame != NULL)
        memset(frame, 0, sizeof(*frame));
}

bool
semantic_map_payload_view_call_argument(MapPayloadViewCallFrame *frame,
    ASTNode *argument, Type *argument_type, SemanticContext *ctx)
{
    if (frame == NULL || ctx == NULL || !type_can_carry_view(argument_type))
        return true;
    uint32_t source = view_source_of(argument, ctx, 0);
    if (source == 0)
        return true;
    if (frame->count == frame->capacity) {
        size_t capacity = frame->capacity == 0 ? 4 : frame->capacity * 2;
        uint32_t *sources = realloc(frame->sources, capacity * sizeof(*sources));
        if (sources != NULL) frame->sources = sources;
        size_t *marks = realloc(frame->marks, capacity * sizeof(*marks));
        if (marks != NULL) frame->marks = marks;
        ASTNode **arguments = realloc(frame->arguments, capacity * sizeof(*arguments));
        if (arguments != NULL) frame->arguments = arguments;
        if (sources == NULL || marks == NULL || arguments == NULL) {
            semantic_error(ctx, argument, "HashMap String view argument record allocation failed");
            return false;
        }
        frame->capacity = capacity;
    }
    frame->sources[frame->count] = source;
    frame->marks[frame->count] = ctx->map_payload_view_change_count;
    frame->arguments[frame->count] = argument;
    frame->count++;
    return true;
}

bool
semantic_map_payload_view_call_finish(MapPayloadViewCallFrame *frame,
    SemanticContext *ctx)
{
    bool ok = true;
    for (size_t i = 0; frame != NULL && ctx != NULL && i < frame->count && ok; i++) {
        for (size_t c = frame->marks[i]; c < ctx->map_payload_view_change_count; c++) {
            if (!sources_overlap(frame->sources[i], ctx->map_payload_view_changes[c]))
                continue;
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                frame->arguments[i],
                "A String MapGet returned is passed while the same call changes its HashMap.\n"
                "Reason:\n"
                "- a later argument or the callee replaces or removes the map value the view points at\n"
                "- the callee would read freed memory\n"
                "Fix:\n"
                "- copy the value with Concat(\"\", MapGet(map, key)) first\n"
                "- or change the map in a separate statement after the call");
            ok = false;
            break;
        }
    }
    if (frame != NULL) {
        free(frame->sources);
        free(frame->marks);
        free(frame->arguments);
        memset(frame, 0, sizeof(*frame));
    }
    return ok;
}
