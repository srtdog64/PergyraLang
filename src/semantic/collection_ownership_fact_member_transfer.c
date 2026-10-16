#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

bool
collection_member_move_identity(const ASTNode *member_access,
                                SemanticContext *ctx,
                                uint32_t *root_binding_id_out,
                                uint32_t *field_syntax_id_out)
{
    ASTNode *root;
    Symbol *binding;
    ASTNode *decl;
    const char *field_name;
    uint32_t root_binding_id;

    if (root_binding_id_out != NULL)
        *root_binding_id_out = 0;
    if (field_syntax_id_out != NULL)
        *field_syntax_id_out = 0;
    if (member_access == NULL || member_access->type != AST_MEMBER_ACCESS
        || ctx == NULL)
        return false;
    root = ast_member_object(member_access);
    if (root == NULL || root->type != AST_IDENTIFIER)
        return false;
    binding = scope_lookup(ctx->scope, ast_identifier_name(root));
    root_binding_id = receiver_binding_syntax_id(root, binding);
    field_name = ast_member_name(member_access);
    decl = binding != NULL
        ? semantic_host_decl_for_type(ctx, binding->type) : NULL;
    if (root_binding_id == 0 || field_name == NULL || decl == NULL
        || decl->type != AST_CLASS_DECL)
        return false;
    for (size_t i = 0; i < projection_source_field_count(decl); i++) {
        PgyDeclField field = projection_source_field_at(decl, i);
        if (field.name != NULL && strcmp(field.name, field_name) == 0
            && field.declaration_syntax_id != 0) {
            if (root_binding_id_out != NULL)
                *root_binding_id_out = root_binding_id;
            if (field_syntax_id_out != NULL)
                *field_syntax_id_out = field.declaration_syntax_id;
            return true;
        }
    }
    return false;
}

bool
semantic_collection_reject_moved_member_use(ASTNode *member_access,
                                             SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    const char *field_name;

    if (!collection_member_move_identity(member_access, ctx,
            &root_binding_id, &field_syntax_id))
        return false;
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || fact->source_binding_syntax_id != root_binding_id
            || fact->origin_syntax_id != field_syntax_id)
            continue;
        field_name = ast_member_name(member_access);
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MOVE_FROM_RELEASED,
            PGY_CAUSE_MOVE_FROM_RELEASED,
            PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
            member_access,
            "Field '%s' was moved into collection binding syntax %u and cannot be used again.\n"
            "Reason:\n"
            "- field extraction transfers the collection descriptor and its storage lifetime\n"
            "- using the source field again would recreate a shallow alias\n"
            "Fix:\n"
            "- use Clone(source.%s) when both values must remain live\n"
            "- or keep using only the moved local binding",
            field_name != NULL ? field_name : "<field>",
            fact->binding_syntax_id,
            field_name != NULL ? field_name : "<field>");
        return true;
    }
    return false;
}

bool
semantic_collection_admit_owned_string_push(ASTNode *receiver,
                                             SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    const char *name = binding != NULL && binding->name != NULL
        ? binding->name : "<array>";

    if (binding != NULL && binding->has_escaped_array_storage) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver, "ArrayPushOwnedString cannot mutate escaped array storage in '%s'",
            name);
        return false;
    }
    if (fact == NULL)
        return true;
    if (fact->element_ownership != PGY_STRING_ARRAY_BORROWED_ELEMENTS
        && !(fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
             && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE))
        return true;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver,
        "ArrayPushOwnedString cannot mix an owned String into '%s' without uniform element ownership.\n"
        "Reason:\n"
        "- the existing elements are borrowed or have no deep-release proof\n"
        "- the resulting array would have no sound whole-array cleanup policy\n"
        "Fix:\n"
        "- Clone the collection into an owned snapshot before the owned push\n"
        "- or use ArrayPush for a borrowed String element",
        name);
    return false;
}

bool
semantic_collection_restore_moved_member(ASTNode *target,
                                         ASTNode *value,
                                         SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    uint32_t value_binding_id;
    Symbol *value_binding;

    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL
        || !collection_member_move_identity(target, ctx,
            &root_binding_id, &field_syntax_id))
        return false;
    value_binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    value_binding_id = receiver_binding_syntax_id(value, value_binding);
    if (value_binding == NULL || value_binding_id == 0)
        return false;

    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || fact->binding_syntax_id != value_binding_id
            || fact->source_binding_syntax_id != root_binding_id
            || fact->origin_syntax_id != field_syntax_id)
            continue;
        /* Exact move-back restores the field and consumes the temporary.
         * No descriptor is copied and no second owner remains live. */
        fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        value_binding->is_consumed = true;
        return true;
    }
    return false;
}
