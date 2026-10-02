/* Public storage release admission. Element resources never belong to this
 * operation; unresolved metadata and descriptor aliases refuse release. */
#include "array_storage_release_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "builtin_kind.h"
#include "compiler/decl_field_model.h"
#include "diag_codes.h"
#include <stdlib.h>

static bool
array_storage_plain_element(const Type *type, SemanticContext *ctx, unsigned depth)
{
    if (type == NULL || depth >= 32)
        return false;
    if (type_equals(type, TYPE_INT) || type_equals(type, TYPE_LONG) || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE) || type_equals(type, TYPE_BOOL) || type_equals(type, TYPE_DURATION)) return true;
    if (type_is_constructed_named(type, "Result"))
        return type_constructed_arg_count(type) == 2 && array_storage_plain_element(type_get_constructed_arg(type, 0), ctx, depth + 1)
            && array_storage_plain_element(type_get_constructed_arg(type, 1), ctx, depth + 1);
    if (type->kind == TYPE_KIND_ENUM) {
        ASTNode *decl = semantic_find_enum_decl_by_name(ctx, type->name);
        size_t count = 0;
        if (decl == NULL)
            return false;
        (void)ast_enum_variants(decl, &count);
        for (size_t v = 0; v < count; v++) {
            for (size_t p = 0; p < ast_enum_variant_param_count(decl, v); p++) {
                Type *payload = semantic_type_resolution_lookup_metadata_type_ref(ctx, ast_enum_variant_param(decl, v, p));
                if (!array_storage_plain_element(payload, ctx, depth + 1))
                    return false;
            }
        }
        return count != 0;
    }
    if (type->kind != TYPE_KIND_CLASS || type->nominal_flavor != TYPE_NOMINAL_STRUCT) return false;
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);
    if (decl == NULL || decl->type != AST_CLASS_DECL) return false;
    PgyDeclField *fields = NULL;
    size_t count = 0;
    if (!pgy_class_decl_field_model_try_build(decl, &fields, &count)) return false;
    bool plain = true;
    for (size_t i = 0; i < count && plain; i++) {
        Type *field = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, fields[i].type_ast);
        plain = array_storage_plain_element(field, ctx, depth + 1);
    }
    free(fields);
    return plain;
}

static void
array_storage_invalidate_exclusivity(ASTNode *source, const Type *type,
    bool stored_alias, SemanticContext *ctx)
{
    if (ctx == NULL || source == NULL || source->type != AST_IDENTIFIER
        || !type_is_constructed_named(type, "Array")) return;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(source));
    if (binding != NULL) {
        binding->has_exclusive_array_storage = false;
        if (stored_alias) binding->has_escaped_array_storage = true;
    }
}

void
semantic_array_storage_escape(ASTNode *source, const Type *type,
                              SemanticContext *ctx)
{
    array_storage_invalidate_exclusivity(source, type, true, ctx);
}

void
semantic_array_storage_call_argument(ASTNode *source, const Type *type,
    const Type *result_type, ParamMode mode, bool constructor, SemanticContext *ctx)
{
    /* A borrow can return an alias inside an aggregate. Until return/escape
     * carriage proves otherwise, a resource-bearing result or inout handoff
     * ends the caller's exclusive release provenance. */
    if (constructor || (mode != PARAM_MODE_OWN && (mode == PARAM_MODE_MUT_REF
        || (!type_equals(result_type, TYPE_VOID)
            && !array_storage_plain_element(result_type, ctx, 0)))))
        /* A possible call-result alias is not a recorded descriptor store.
         * Preserve that distinction for legacy Clone/MapKeys deep cleanup. */
        array_storage_invalidate_exclusivity(source, type, constructor, ctx);
}

void
semantic_array_storage_initialize(Symbol *binding, const ASTNode *initializer,
                                  SemanticContext *ctx)
{
    if (binding == NULL || !type_is_constructed_named(binding->type, "Array"))
        return;
    /* A literal allocates fresh backing storage.  General call results remain
     * unproved until return ownership is carried through the active collection
     * ownership rung; callable identity alone is not storage provenance. */
    binding->has_exclusive_array_storage = ctx->current_function_decl != NULL
        && initializer != NULL && initializer->type == AST_ARRAY_LITERAL;
    if (initializer != NULL && initializer->type == AST_IDENTIFIER) {
        Symbol *source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        /* A local move cannot erase a store retained by another descriptor.
         * Capture the prior state before this handoff invalidates the source. */
        binding->has_escaped_array_storage = source == NULL
            || ast_identifier_binding_syntax_id(initializer) != source->decl_syntax_id
            || source->has_escaped_array_storage;
    }
    semantic_array_storage_escape((ASTNode *)initializer, binding->type, ctx);
}

void
semantic_array_storage_assignment(ASTNode *target, const Type *target_type,
    ASTNode *value, const Type *value_type, SemanticContext *ctx)
{
    Symbol *binding = target != NULL && target->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(target, ctx) : NULL;
    Symbol *source = value != NULL && value->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(value, ctx) : NULL;
    if (binding != NULL && binding == source)
        return;
    semantic_array_storage_escape(value, value_type, ctx);
    bool alias = source != NULL || (value != NULL
        && (value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS));
    array_storage_invalidate_exclusivity(target, target_type, alias, ctx);
    if (binding == NULL || ctx->in_defer_cleanup
        || !type_is_constructed_named(target_type, "Array")
        || scope_lookup_current(ctx->scope, binding->name) != binding)
        return;
    uint32_t builtin = 0;
    bool fresh = value != NULL && ((value->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(value) == 0)
        || (value->type == AST_CALL
            && ast_call_semantic_callee_builtin_kind(value, &builtin)
            && builtin == BUILTIN_CLONE));
    /* Only an exact independent producer at this lexical frontier replaces
     * the escaped storage. No element fact or exclusive grant is invented;
     * outer bindings written in branch/loop/defer scopes stay unproved. */
    if (fresh)
        binding->has_escaped_array_storage = false;
}

bool
semantic_array_storage_admit_drop(ASTNode *receiver, Type *array_type,
                                 SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    bool owns = binding != NULL && !binding->is_host_field
        && binding->has_exclusive_array_storage
        && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN);
    if (!type_is_constructed_named(array_type, "Array") || !owns) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    if (!array_storage_plain_element(type_get_constructed_arg(array_type, 0), ctx, 0)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            receiver, "ArrayDrop requires plain value elements without independent resource lifetimes; use ArrayDropOwnedStrings for owned strings");
        return false;
    }
    return consume_array_storage_binding(receiver, ctx);
}
