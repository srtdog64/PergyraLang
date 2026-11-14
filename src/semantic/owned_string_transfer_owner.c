/* Transfer of String storage to a new owner; see the header. Symbol pointers
 * are used only for transient scope lookup; identities are SyntaxNodeIds. */

#include <stdlib.h>
#include <string.h>
#include "owned_string_transfer_owner.h"
#include "collection_ownership_fact.h"
#include "diag_codes.h"
#include "indexed_string_borrow_owner.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_call_constructor_internal.h"

#define OWNED_STRING_MAX_DEPTH 32

typedef struct {
    uint8_t state;
    uint32_t source;
    const ASTNode *producer;
} OwnedStringValue;

static const OwnedStringValue owned_string_unknown = {OWNED_STRING_UNKNOWN, 0, NULL};

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

static bool
type_is_string_array(const Type *type)
{
    return type != NULL && type_is_constructed_named((Type *)type, "Array")
        && type_equals(type_get_constructed_arg((Type *)type, 0), TYPE_STRING);
}

/* A String, or a struct or Array<String> that can be built from one. */
static bool
binding_tracked(const Symbol *binding)
{
    return binding != NULL && binding->type != NULL
        && (type_equals(binding->type, TYPE_STRING)
            || binding->type->kind == TYPE_KIND_CLASS
            || type_is_string_array(binding->type));
}

static OwnedStringValue
binding_value(const Symbol *binding)
{
    if (binding->is_parameter) {
        if (type_equals(binding->type, TYPE_STRING)
            && binding->param_mode == PARAM_MODE_OWN)
            return (OwnedStringValue){OWNED_STRING_OWNED, binding->decl_syntax_id, NULL};
        return (OwnedStringValue){OWNED_STRING_BORROWED, 0, NULL};
    }
    return (OwnedStringValue){binding->owned_string_state,
        binding->owned_string_source_syntax_id, binding->owned_string_producer};
}

/* The struct declaration a constructor call builds, or NULL. */
static ASTNode *
call_constructed_decl(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *callee = ast_call_callee(call);
    Symbol *symbol = callee != NULL && callee->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(callee)) : NULL;
    if (symbol == NULL || symbol->kind != SYMBOL_CLASS)
        return NULL;
    ASTNode *decl = semantic_host_decl_for_type(ctx, symbol->type);
    return decl != NULL && decl->type == AST_CLASS_DECL ? decl : NULL;
}

static bool
call_is_constructor_like(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *callee = ast_call_callee(call);
    Symbol *symbol = callee != NULL && callee->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(callee)) : NULL;
    return symbol != NULL && (symbol->kind == SYMBOL_CLASS
        || symbol->kind == SYMBOL_ENUM_CONSTRUCTOR);
}

static bool
call_is_user_function(const ASTNode *call)
{
    return ast_call_semantic_callee_decl_id(call) != 0
        && ast_call_semantic_callee_declared_callable(call)
        && ast_call_semantic_callee_value_binding_id(call) == 0;
}

/* Parts of one struct or literal: a borrowed or unknown part makes the
 * whole untransferable; owned parts from two different bindings, or two
 * pending producers, cannot be released through one identity. */
static OwnedStringValue
value_join(OwnedStringValue whole, OwnedStringValue part, bool *first)
{
    if (*first) {
        *first = false;
        return part;
    }
    if (whole.state == OWNED_STRING_BORROWED || part.state == OWNED_STRING_BORROWED)
        return (OwnedStringValue){OWNED_STRING_BORROWED, 0, NULL};
    if (whole.state == OWNED_STRING_UNKNOWN || part.state == OWNED_STRING_UNKNOWN)
        return owned_string_unknown;
    if ((whole.source != 0 && part.source != 0 && whole.source != part.source)
        || (whole.producer != NULL && part.producer != NULL))
        return owned_string_unknown;
    if (whole.source == 0)
        whole.source = part.source;
    if (whole.producer == NULL && part.producer != NULL) {
        whole.producer = part.producer;
        whole.state = OWNED_STRING_PENDING;
    }
    return whole;
}


static OwnedStringValue value_of(ASTNode *expression, SemanticContext *ctx, int depth);

static OwnedStringValue
call_value(ASTNode *call, SemanticContext *ctx, int depth)
{
    ASTNode *decl = call_constructed_decl(call, ctx);
    if (decl != NULL) {
        /* Positional arguments fill the declared fields in order. */
        OwnedStringValue whole = {OWNED_STRING_OWNED, 0, NULL};
        bool first = true;
        for (size_t i = 0; i < ast_call_arg_count(call); i++) {
            const char *name = NULL;
            Type *field = constructor_decl_field_resolved_type_at(decl, i, &name, ctx);
            if (name == NULL)
                return owned_string_unknown;
            if (type_equals(field, TYPE_STRING))
                whole = value_join(whole, value_of(ast_call_argument(call, i), ctx, depth + 1), &first);
        }
        return first ? (OwnedStringValue){OWNED_STRING_BORROWED, 0, NULL} : whole;
    }
    if (call_is_constructor_like(call, ctx))
        return owned_string_unknown;
    if (call_is_user_function(call))
        return (OwnedStringValue){OWNED_STRING_PENDING, 0, call};
    if (semantic_indexed_string_independent_result(call, ctx))
        return (OwnedStringValue){OWNED_STRING_OWNED, 0, NULL};
    return owned_string_unknown;
}

static OwnedStringValue
value_of(ASTNode *expression, SemanticContext *ctx, int depth)
{
    if (expression == NULL || depth > OWNED_STRING_MAX_DEPTH)
        return owned_string_unknown;
    switch (expression->type) {
    case AST_STRING:
    case AST_ARRAY_ACCESS:
        return (OwnedStringValue){OWNED_STRING_BORROWED, 0, NULL};
    case AST_IDENTIFIER: {
        Symbol *binding = identifier_symbol(expression, ctx);
        return binding_tracked(binding) ? binding_value(binding) : owned_string_unknown;
    }
    case AST_MEMBER_ACCESS: {
        Symbol *root = identifier_symbol(ast_member_object(expression), ctx);
        return binding_tracked(root) && !root->is_parameter
            ? binding_value(root)
            : (OwnedStringValue){OWNED_STRING_BORROWED, 0, NULL};
    }
    case AST_BINARY:
        return semantic_indexed_string_independent_result(expression, ctx)
            ? (OwnedStringValue){OWNED_STRING_OWNED, 0, NULL} : owned_string_unknown;
    case AST_ARRAY_LITERAL: {
        OwnedStringValue whole = owned_string_unknown;
        bool first = true;
        for (size_t i = 0; i < ast_array_literal_count(expression); i++)
            whole = value_join(whole, value_of(ast_array_literal_element(expression, i),
                ctx, depth + 1), &first);
        return whole;
    }
    case AST_CALL:
        return call_value(expression, ctx, depth);
    default:
        return owned_string_unknown;
    }
}

static uint32_t
read_root_binding(ASTNode *value, SemanticContext *ctx)
{
    if (value != NULL && value->type == AST_IDENTIFIER) {
        Symbol *source = identifier_symbol(value, ctx);
        if (source == NULL)
            return 0;
        return source->storage_root_binding_id != 0 ? source->storage_root_binding_id
            : source->is_parameter ? source->decl_syntax_id : 0;
    }
    bool path = false;
    while (value != NULL && (value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS)) {
        value = value->type == AST_MEMBER_ACCESS ? ast_member_object(value) : ast_array_access_array(value);
        path = true;
    }
    Symbol *root = path ? identifier_symbol(value, ctx) : NULL;
    return root != NULL ? root->decl_syntax_id : 0;
}

/* The binding a value names directly or through a field/element path. */
static Symbol *
value_root_symbol(ASTNode *value, SemanticContext *ctx)
{
    while (value != NULL && (value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS))
        value = value->type == AST_MEMBER_ACCESS ? ast_member_object(value) : ast_array_access_array(value);
    return identifier_symbol(value, ctx);
}

/* A part built from String literals or from a binding that holds borrowed
 * String elements. */
static bool
value_elements_borrowed(ASTNode *value, SemanticContext *ctx, int depth)
{
    if (value == NULL || depth > OWNED_STRING_MAX_DEPTH)
        return false;
    if (value->type == AST_STRING)
        return true;
    if (value->type == AST_ARRAY_LITERAL) {
        for (size_t i = 0; i < ast_array_literal_count(value); i++)
            if (value_elements_borrowed(ast_array_literal_element(value, i), ctx, depth + 1))
                return true;
        return false;
    }
    if (value->type == AST_CALL && call_constructed_decl(value, ctx) != NULL) {
        for (size_t i = 0; i < ast_call_arg_count(value); i++)
            if (value_elements_borrowed(ast_call_argument(value, i), ctx, depth + 1))
                return true;
        return false;
    }
    Symbol *root = value->type == AST_IDENTIFIER || value->type == AST_MEMBER_ACCESS
        ? value_root_symbol(value, ctx) : NULL;
    return root != NULL && root->storage_elements_borrowed;
}

/* Two arguments of one constructor name the same storage-holding binding,
 * or one names a binding whose parts already share storage. */
static bool
value_fields_aliased(ASTNode *value, SemanticContext *ctx)
{
    if (value == NULL)
        return false;
    if (value->type == AST_IDENTIFIER || value->type == AST_MEMBER_ACCESS) {
        Symbol *root = value_root_symbol(value, ctx);
        return root != NULL && root->storage_fields_aliased;
    }
    if (value->type != AST_CALL || call_constructed_decl(value, ctx) == NULL)
        return false;
    for (size_t i = 0; i < ast_call_arg_count(value); i++) {
        ASTNode *argument = ast_call_argument(value, i);
        Symbol *held = argument != NULL && argument->type == AST_IDENTIFIER
            ? identifier_symbol(argument, ctx) : NULL;
        if (held == NULL || !binding_tracked(held) || type_equals(held->type, TYPE_STRING))
            continue;
        if (held->storage_fields_aliased)
            return true;
        for (size_t j = i + 1; j < ast_call_arg_count(value); j++)
            if (identifier_symbol(ast_call_argument(value, j), ctx) == held)
                return true;
    }
    return false;
}

static void
binding_set(Symbol *binding, ASTNode *value, SemanticContext *ctx)
{
    binding->storage_root_binding_id = read_root_binding(value, ctx);
    binding->storage_fields_aliased = value_fields_aliased(value, ctx);
    binding->storage_elements_borrowed = value_elements_borrowed(value, ctx, 0);
    OwnedStringValue held = value_of(value, ctx, 0);
    if ((held.state == OWNED_STRING_OWNED || held.state == OWNED_STRING_PENDING)
        && held.source == 0)
        held.source = binding->decl_syntax_id;
    binding->owned_string_state = held.state;
    binding->owned_string_source_syntax_id = held.source;
    binding->owned_string_producer = held.producer;
    binding->owned_string_released = false;
    binding->owned_string_literal = type_is_string_array(binding->type)
        && value != NULL && value->type == AST_ARRAY_LITERAL ? value : NULL;
}

void
semantic_owned_string_initialize_binding(Symbol *binding, ASTNode *initializer,
    SemanticContext *ctx)
{
    if (ctx != NULL && binding_tracked(binding) && !binding->is_parameter)
        binding_set(binding, initializer, ctx);
}

void
semantic_owned_string_record_assignment(Symbol *target, ASTNode *value,
    SemanticContext *ctx)
{
    if (ctx != NULL && binding_tracked(target) && !target->is_parameter)
        binding_set(target, value, ctx);
}

void
semantic_owned_string_record_member_assignment(ASTNode *target, ASTNode *value,
    SemanticContext *ctx)
{
    Symbol *root = ctx != NULL ? value_root_symbol(target, ctx) : NULL;
    if (root == NULL || !binding_tracked(root) || root->is_parameter)
        return;
    Symbol *source = (value != NULL && (value->type == AST_IDENTIFIER
        || value->type == AST_MEMBER_ACCESS)) ? value_root_symbol(value, ctx) : NULL;
    if (source == root || (source != NULL && source->storage_fields_aliased))
        root->storage_fields_aliased = true;
    if (value_elements_borrowed(value, ctx, 0))
        root->storage_elements_borrowed = true;
}

void
semantic_owned_string_record_push(ASTNode *receiver, ASTNode *value,
    SemanticContext *ctx)
{
    Symbol *root = ctx != NULL ? value_root_symbol(receiver, ctx) : NULL;
    if (root != NULL && binding_tracked(root) && !root->is_parameter
        && value_of(value, ctx, 0).state == OWNED_STRING_BORROWED)
        root->storage_elements_borrowed = true;
}

bool
semantic_owned_string_storage_unexclusive(ASTNode *argument, SemanticContext *ctx)
{
    Symbol *binding = ctx != NULL && argument != NULL && argument->type == AST_IDENTIFIER
        ? identifier_symbol(argument, ctx) : NULL;
    return binding != NULL && !binding->is_parameter
        && (binding->storage_fields_aliased || binding->storage_elements_borrowed);
}

static void
release_source(uint32_t source, SemanticContext *ctx)
{
    if (source == 0)
        return;
    ctx->owned_string_release_count++;
    for (Scope *scope = ctx->scope; scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *candidate = scope->symbols[i];
            if (candidate == NULL || candidate->owned_string_released
                || !binding_tracked(candidate))
                continue;
            if (binding_value(candidate).source == source)
                candidate->owned_string_released = true;
        }
    }
}

bool
semantic_owned_string_validate_use(Symbol *binding, ASTNode *use,
    SemanticContext *ctx)
{
    if (binding == NULL || !binding->owned_string_released)
        return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
        PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, use,
        "'%s' holds String storage that an own hand-off or ArrayDropOwnedStrings released.\n"
        "Reason:\n"
        "- the String was handed to a new owner, which frees it\n"
        "- this binding (or an alias sharing its storage) would read freed memory\n"
        "Fix:\n"
        "- finish every read before the hand-off\n"
        "- or hand off an independent copy (Concat(\"\", value))",
        binding->name != NULL ? binding->name : "<value>");
    return false;
}

static bool
record_pending(ASTNode *site, const ASTNode *producer, SemanticContext *ctx)
{
    if (ctx->owned_string_pending_count == ctx->owned_string_pending_capacity) {
        size_t capacity = ctx->owned_string_pending_capacity == 0
            ? 16 : ctx->owned_string_pending_capacity * 2;
        OwnedStringPendingProof *grown = realloc(ctx->owned_string_pending,
            capacity * sizeof(*grown));
        if (grown == NULL) {
            semantic_error(ctx, site, "Owned String producer record allocation failed");
            return false;
        }
        ctx->owned_string_pending = grown;
        ctx->owned_string_pending_capacity = capacity;
    }
    ctx->owned_string_pending[ctx->owned_string_pending_count++] =
        (OwnedStringPendingProof){site, producer, ast_node_stable_id(producer)};
    return true;
}

static const char *
value_label(ASTNode *value)
{
    if (value != NULL && value->type == AST_IDENTIFIER && ast_identifier_name(value) != NULL)
        return ast_identifier_name(value);
    return "<value>";
}

static bool
transfer_value(ASTNode *value, const char *consumer, SemanticContext *ctx)
{
    ASTNode *root = value != NULL && value->type == AST_MEMBER_ACCESS
        ? ast_member_object(value) : value;
    Symbol *binding = identifier_symbol(root, ctx);
    if (binding != NULL && !semantic_owned_string_validate_use(binding, value, ctx))
        return false;
    OwnedStringValue held = value_of(value, ctx, 0);
    if (held.state == OWNED_STRING_PENDING && held.producer != NULL
        && !record_pending(value, held.producer, ctx))
        return false;
    if (held.state == OWNED_STRING_OWNED || held.state == OWNED_STRING_PENDING) {
        release_source(held.source, ctx);
        return true;
    }
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, value,
        "%s takes ownership of '%s', but it is not an owned String allocation.\n"
        "Reason:\n"
        "- %s\n"
        "- the new owner frees the String it receives\n"
        "Fix:\n"
        "- hand over an independent copy (Concat(\"\", value)) or a value an owned producer returned",
        consumer, value_label(value),
        held.state == OWNED_STRING_BORROWED
            ? "a literal, a borrowed formal or a field/element read is not its storage to give"
            : "its provenance is not a proved owned allocation");
    return false;
}

bool
semantic_owned_string_transfer(ASTNode *value, const char *consumer,
    SemanticContext *ctx)
{
    return ctx == NULL || transfer_value(value, consumer, ctx);
}

bool
semantic_owned_string_drop_literal_elements(ASTNode *receiver,
    SemanticContext *ctx)
{
    Symbol *binding = identifier_symbol(receiver, ctx);
    if (ctx == NULL || binding == NULL || binding->owned_string_literal == NULL)
        return true;
    const ASTNode *literal = binding->owned_string_literal;
    for (size_t i = 0; i < ast_array_literal_count(literal); i++)
        if (!transfer_value(ast_array_literal_element(literal, i),
                "ArrayDropOwnedStrings", ctx))
            return false;
    return true;
}

/* `node` (in `routine`'s body) returns the routine's formal `ordinal`. */
static bool
returns_formal(const ASTNode *node, const ASTNode *routine, size_t ordinal, int depth)
{
    if (node == NULL || depth > 256)
        return false;
    switch (node->type) {
    case AST_RETURN: {
        const ASTNode *value = ast_return_value(node);
        FuncParam *formal = ast_func_param((ASTNode *)routine, ordinal);
        return value != NULL && value->type == AST_IDENTIFIER && formal != NULL
            && ast_func_param_stable_id(formal) != 0
            && ast_identifier_binding_syntax_id(value) == ast_func_param_stable_id(formal);
    }
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (returns_formal(ast_block_statement(node, i), routine, ordinal, depth + 1))
                return true;
        return false;
    case AST_IF_STMT:
        return returns_formal(ast_if_then_branch(node), routine, ordinal, depth + 1)
            || returns_formal(ast_if_else_branch(node), routine, ordinal, depth + 1);
    case AST_WHILE_LOOP:
        return returns_formal(ast_while_body(node), routine, ordinal, depth + 1);
    case AST_FOR_LOOP:
        return returns_formal(ast_for_body(node), routine, ordinal, depth + 1);
    default:
        return false;
    }
}

/* The producer returns one of its formals, and the call passes a String
 * literal for it: the result may be that static storage. */
static bool
producer_forwards_literal(const ASTNode *call, SemanticContext *ctx)
{
    const ASTNode *callee = ast_call_callee(call);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    ASTNode *routine = name != NULL ? semantic_find_callable_decl_by_name(ctx, name) : NULL;
    if (routine == NULL || routine->type != AST_FUNC_DECL
        || ast_node_stable_id(routine) != ast_call_semantic_callee_decl_id(call))
        return false;
    for (size_t i = 0; i < ast_call_arg_count(call) && i < ast_func_param_count(routine); i++) {
        const ASTNode *argument = ast_call_argument(call, i);
        if (argument != NULL && argument->type == AST_STRING
            && returns_formal(ast_func_body(routine), routine, i, 0))
            return true;
    }
    return false;
}

/* A producer whose ownership the owned-result receipt proves is admitted;
 * one the examination disproves (a literal or a kept local is returned, or a
 * literal argument is handed back) is refused. A result neither proved nor
 * disproved stays at the native bootstrap boundary for unknown provenance,
 * which the self-host analyzer closes. */
bool
semantic_owned_string_finalize(SemanticContext *ctx)
{
    bool ok = true;
    for (size_t i = 0; ctx != NULL && i < ctx->owned_string_pending_count; i++) {
        const OwnedStringPendingProof *row = &ctx->owned_string_pending[i];
        if (row->producer == NULL || row->producer_id != ast_node_stable_id(row->producer)) {
            semantic_error(ctx, row->site, "Owned String producer identity changed after Pass 2");
            ok = false;
            continue;
        }
        if (semantic_collection_owned_string_call_result(row->producer, ctx, NULL)
            || (!semantic_collection_owned_string_result_disproved(row->producer, ctx)
                && !producer_forwards_literal(row->producer, ctx)))
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->site,
            "The String handed to a new owner comes from a call whose result is not a proved owned allocation.\n"
            "Reason:\n"
            "- the callee may return a literal, a borrowed input or storage it also keeps elsewhere\n"
            "- the new owner would free storage it does not own\n"
            "Fix:\n"
            "- return a fresh allocation from the callee (Concat, StringJoin, TextBuilderFinish)\n"
            "- or hand over an independent copy (Concat(\"\", value))");
        ok = false;
    }
    return ok;
}
