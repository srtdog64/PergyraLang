/* Exact storage-source liveness for String values borrowed from Array<String>.
 * The semantic identity is a SyntaxNodeId carried by the current collection
 * definition. Symbol pointers are used only for transient scope lookup. */

#include <stdint.h>
#include <string.h>

#include "diag_codes.h"
#include "indexed_string_borrow_owner.h"
#include "type_checker_internal.h"

#define INDEXED_STRING_BORROW_UNKNOWN_SOURCE UINT32_MAX

static bool
symbol_is_string_array(const Symbol *symbol)
{
    Type *element;
    if (symbol == NULL ||
        !type_is_constructed_named(symbol->type, "Array") ||
        type_constructed_arg_count(symbol->type) != 1)
        return false;
    element = type_constructed_arg(symbol->type, 0);
    return element != NULL && type_equals(element, TYPE_STRING);
}

static uint32_t
collection_storage_source_id(const Symbol *symbol)
{
    if (symbol == NULL)
        return 0;
    if (symbol->collection_storage_source_syntax_id != 0)
        return symbol->collection_storage_source_syntax_id;
    return symbol->decl_syntax_id;
}

static Symbol *
identifier_symbol(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL || node->type != AST_IDENTIFIER)
        return NULL;
    return lookup_identifier_symbol(node, ctx);
}

static bool
expression_produces_independent_string(ASTNode *expression)
{
    ASTNode *callee;
    const char *name;

    if (expression == NULL)
        return false;
    if (expression->type == AST_STRING)
        return true;
    if (expression->type != AST_CALL
        || !ast_call_semantic_callee_is_stdlib(expression))
        return false;
    callee = ast_call_callee(expression);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    return name != NULL && (strcmp(name, "Concat") == 0
        || strcmp(name, "StringConcat") == 0);
}

static uint32_t
borrow_source_for_expression(ASTNode *expression, SemanticContext *ctx)
{
    Symbol *source;
    ASTNode *base;
    if (expression == NULL)
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    if (expression_produces_independent_string(expression))
        return 0;
    if (expression->type == AST_IDENTIFIER) {
        source = identifier_symbol(expression, ctx);
        if (source != NULL && type_equals(source->type, TYPE_STRING))
            return source->indexed_string_borrow_source_syntax_id;
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    }
    if (expression->type != AST_ARRAY_ACCESS)
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    base = ast_array_access_array(expression);
    source = identifier_symbol(base, ctx);
    if (!symbol_is_string_array(source))
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    return collection_storage_source_id(source);
}

static uint32_t
borrow_source_join(uint32_t current, uint32_t incoming)
{
    if (current == 0)
        return incoming;
    if (incoming == 0 || current == incoming)
        return current;
    return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
}

void
semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *initializer, SemanticContext *ctx)
{
    Symbol *source;
    if (binding == NULL)
        return;
    if (type_is_constructed_named(binding->type, "Array")) {
        binding->collection_storage_source_syntax_id =
            binding->decl_syntax_id;
        source = identifier_symbol(initializer, ctx);
        if (source != NULL &&
            type_is_constructed_named(source->type, "Array")) {
            uint32_t source_id = collection_storage_source_id(source);
            if (source_id != 0)
                binding->collection_storage_source_syntax_id = source_id;
        }
    }
    if (type_equals(binding->type, TYPE_STRING)) {
        binding->indexed_string_borrow_source_syntax_id =
            borrow_source_for_expression(initializer, ctx);
        binding->indexed_string_borrow_invalidated = false;
    }
}

void
semantic_indexed_string_borrow_record_assignment(
    Symbol *binding, ASTNode *value, ASTNode *assignment,
    SemanticContext *ctx)
{
    Symbol *source;
    if (binding == NULL)
        return;
    if (type_is_constructed_named(binding->type, "Array")) {
        uint32_t source_id = ast_node_stable_id(assignment);
        uint32_t current_id = collection_storage_source_id(binding);
        source = identifier_symbol(value, ctx);
        if (source != NULL &&
            type_is_constructed_named(source->type, "Array"))
            source_id = collection_storage_source_id(source);
        binding->collection_storage_source_syntax_id =
            borrow_source_join(current_id, source_id);
        return;
    }
    if (type_equals(binding->type, TYPE_STRING)) {
        uint32_t incoming = borrow_source_for_expression(value, ctx);
        binding->indexed_string_borrow_source_syntax_id = borrow_source_join(
            binding->indexed_string_borrow_source_syntax_id, incoming);
    }
}

void
semantic_indexed_string_borrow_invalidate_deep_drop(
    ASTNode *receiver, SemanticContext *ctx)
{
    Symbol *owner = identifier_symbol(receiver, ctx);
    uint32_t source_id;
    if (!symbol_is_string_array(owner) || ctx == NULL)
        return;
    source_id = collection_storage_source_id(owner);
    if (source_id == 0)
        return;
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *candidate = scope->symbols[i];
            if (candidate != NULL &&
                (source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                 || candidate->indexed_string_borrow_source_syntax_id ==
                        INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                 || candidate->indexed_string_borrow_source_syntax_id ==
                        source_id))
                candidate->indexed_string_borrow_invalidated = true;
        }
    }
}

bool
semantic_indexed_string_borrow_validate_use(
    Symbol *binding, ASTNode *use, SemanticContext *ctx)
{
    if (binding == NULL ||
        binding->indexed_string_borrow_source_syntax_id == 0 ||
        !binding->indexed_string_borrow_invalidated)
        return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, use,
        "String '%s' borrows an Array<String> element that was released.\n"
        "Reason:\n"
        "- ArrayDropOwnedStrings retired the exact storage source that owns this String payload\n"
        "- using the indexed alias afterwards would read freed memory\n"
        "Fix:\n"
        "- copy the String with Concat(\"\", value) before the deep drop\n"
        "- or keep the Array<String> owner live until the final alias use",
        binding->name != NULL ? binding->name : "<string>");
    return false;
}
