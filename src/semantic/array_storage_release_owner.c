/* Public Array storage release and mutation admission. */
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact_internal.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "indexed_string_named_call_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_defer_release_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "loan_copy_landing_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include "indexed_string_definition_recipe.h"
#include "indexed_string_permanent_element.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
semantic_array_storage_admit_drop(ASTNode *receiver, Type *array_type,
                                 SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    bool owns = binding != NULL && !binding->is_host_field
        && !binding->is_consumed && !binding->has_escaped_array_storage
        && binding->has_exclusive_array_storage
        && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN);
    if (!type_is_constructed_named(array_type, "Array") || !owns) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    if (!semantic_array_storage_plain_element(type_get_constructed_arg(array_type, 0), ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            receiver, "ArrayDrop requires plain value elements without independent resource lifetimes; use ArrayDropOwnedStrings for owned strings");
        return false;
    }
    /* Exclusivity that rests on a recorded inout handoff is decided after
     * Pass 2, which refuses this release there if a handoff is unproved. */
    if (semantic_array_storage_has_pending(binding)
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_RELEASE_DROP_STORAGE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    return consume_array_storage_binding(receiver, ctx);
}

/* ArraySort and ArrayReverse reorder the elements in place: they neither grow
 * nor free the buffer, so a binding that shares the storage (the result of
 * an earlier permutation of the same Array) keeps a valid descriptor and
 * valid elements. Such a shared binding needs live storage, not the exclusive
 * returned-storage ownership a growing or releasing mutation needs; which
 * holder may observe the new order stays with the copy and loan owners. An
 * exclusive pending result is still recorded and judged after Pass 2. */
bool
semantic_array_storage_admit_permutation(ASTNode *receiver, SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    if (binding == NULL || !binding->has_pending_array_result
        || binding->has_exclusive_array_storage)
        return semantic_array_storage_admit_mutation(receiver, ctx);
    if (!binding->is_consumed)
        return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver, "Array permutation requires live storage; '%s' was moved or released",
        binding->name != NULL ? binding->name : "<array>");
    return false;
}

bool
semantic_array_storage_admit_mutation(ASTNode *receiver, SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    if (binding == NULL || !binding->has_pending_array_result) return true;
    if (!binding->is_consumed && !binding->has_escaped_array_storage
        && binding->has_exclusive_array_storage
        && semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_MUTATE_STORAGE)) return true;
    if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
        const char *routine = ctx->current_function_decl == NULL ? NULL
            : ast_declaration_name(ctx->current_function_decl);
        fprintf(stderr,
            "pgy: array-mutation-storage-unproved routine=%s receiver=%s"
            " line=%u column=%u consumed=%d escaped=%d exclusive=%d\n",
            routine != NULL ? routine : "<unknown>",
            binding != NULL && binding->name != NULL ? binding->name : "<unknown>",
            receiver != NULL ? receiver->line : 0,
            receiver != NULL ? receiver->column : 0,
            binding != NULL && binding->is_consumed,
            binding != NULL && binding->has_escaped_array_storage,
            binding != NULL && binding->has_exclusive_array_storage);
    }
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver, "Array mutation requires current returned-storage ownership; the result's source or transfer is unproved");
    return false;
}
