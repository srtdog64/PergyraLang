/* Exact storage-source liveness for String values borrowed from Array<String>.
 * The semantic identity is a SyntaxNodeId carried by the current collection
 * definition. Symbol pointers are used only for transient scope lookup. */

#include <stdint.h>
#include <string.h>
#include "diag_codes.h"
#include "indexed_string_borrow_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_owned_element_requirement_owner.h"
#include "collection_ownership_fact.h"
#include "indexed_string_definition_recipe.h"
#include "indexed_string_named_call_owner.h"
#include "loan_copy_landing_owner.h"
#include "formal_string_store_landing_owner.h"
#include "type_checker/type_checker_internal.h"

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
    /* This query also runs inside value publication. The expression lookup
     * re-enters that issuer; read the already-typed binding without publishing. */
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(node));
    return binding != NULL && binding->decl_syntax_id != 0
        && binding->decl_syntax_id == ast_identifier_binding_syntax_id(node)
        ? binding : NULL;
}

static bool
indexed_string_path_rooted_in_caller_parameter(ASTNode *expression,
    SemanticContext *ctx)
{
    if (expression == NULL
        || (expression->type != AST_MEMBER_ACCESS
            && expression->type != AST_ARRAY_ACCESS))
        return false;
    ASTNode *root = expression;
    while (root != NULL
        && (root->type == AST_MEMBER_ACCESS
            || root->type == AST_ARRAY_ACCESS))
        root = root->type == AST_MEMBER_ACCESS
            ? ast_member_object(root) : ast_array_access_array(root);
    Symbol *binding = identifier_symbol(root, ctx);
    ASTNode *routine = ctx != NULL ? ctx->current_function_decl : NULL;
    if (binding == NULL || !binding->is_parameter
        || routine == NULL || routine->type != AST_FUNC_DECL)
        return false;
    for (size_t p = 0; p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL
            && ast_func_param_stable_id(param) == binding->decl_syntax_id
            && (param->mode == PARAM_MODE_DEFAULT
                || param->mode == PARAM_MODE_REF))
            return true;
    }
    return false;
}

bool semantic_indexed_string_borrow_array_binding_source(const Symbol *binding,
    uint32_t *source_id)
{
    if (source_id == NULL || !symbol_is_string_array(binding)) return false;
    *source_id = collection_storage_source_id(binding);
    return *source_id != 0;
}

bool semantic_indexed_string_borrow_array_receiver_source(ASTNode *receiver,
    SemanticContext *ctx, bool *tracked, uint32_t *source_id)
{
    if (tracked == NULL || source_id == NULL) return false;
    *tracked = false; *source_id = 0;
    if (receiver == NULL || receiver->type != AST_IDENTIFIER) return true;
    Symbol *binding = identifier_symbol(receiver, ctx);
    if (binding == NULL) return false;
    *tracked = symbol_is_string_array(binding);
    return !*tracked || semantic_indexed_string_borrow_array_binding_source(binding, source_id);
}

static uint32_t
borrow_source_for_expression(ASTNode *expression, SemanticContext *ctx)
{
    Symbol *source;
    ASTNode *base;
    if (expression == NULL)
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    if (semantic_indexed_string_independent_result(expression, ctx))
        return 0;
    uint32_t producer = 0;
    if (expression->type == AST_CALL
        && semantic_collection_owned_string_call_result(expression, ctx, &producer))
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

bool
semantic_indexed_string_borrow_publish_source(ASTNode *expression,
    SemanticContext *ctx)
{
    PublishedNode *row = ctx != NULL && ctx->function_param_publications != NULL
        ? publication_node(ctx->function_param_publications, expression, false) : NULL;
    if (row == NULL || !row->value_present) return false;
    if (row->string_value && expression->type == AST_CALL && !publication_call_ready(ctx, row))
        return !row->indexed_string_source_present;
    Symbol *array = identifier_symbol(expression, ctx);
    row->indexed_array_source_present = symbol_is_string_array(array);
    row->indexed_array_source_id = row->indexed_array_source_present
        ? collection_storage_source_id(array) : 0;
    row->indexed_string_source_present = row->string_value;
    row->indexed_string_source_id = row->string_value
        ? borrow_source_for_expression(expression, ctx) : 0;
    if (row->string_value && expression->type == AST_IDENTIFIER
        && row->indexed_string_source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
        Symbol *binding = identifier_symbol(expression, ctx);
        PublishedNode *let = binding == NULL ? NULL : publication_node_by_id(
            ctx->function_param_publications, binding->decl_syntax_id);
        if (let != NULL && let->node != NULL && let->node->type == AST_LET_DECL
            && let->indexed_string_let.present)
            row->indexed_string_let = let->indexed_string_let;
    }
    if (row->string_value) {
        bool array_receiver = expression->type == AST_ARRAY_ACCESS;
        Symbol *binding = identifier_symbol(array_receiver ? ast_array_access_array(expression) : expression, ctx);
        if (!semantic_array_storage_record_deferred_string_read(expression, row->indexed_string_source_id,
                binding, array_receiver, false, ctx)) return false;
    }
    return true;
}

bool
semantic_indexed_string_borrow_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id)
{
    if (expression == NULL || source_id == NULL || !publication_generation_ready(ctx))
        return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, expression, false);
    if (row == NULL || row->id == 0 || row->id != ast_node_stable_id(expression)
        || !row->value_present || row->meta || row->value_shape != expression->type
        || !row->string_value || !type_equals(row->value_type, TYPE_STRING)
        || !row->indexed_string_source_present
        || (expression->type == AST_IDENTIFIER
            && row->binding_id != ast_identifier_binding_syntax_id(expression))) return false;
    bool concat = row->indexed_string_source_id == 0 && expression->type == AST_BINARY;
    if (concat && !semantic_indexed_string_independent_result(expression, ctx)) return false;
    if (!concat && semantic_param_publication_inputs_ready(ctx)
        && !semantic_param_publication_value(ctx, expression).ok) return false;
    if (row->indexed_string_source_id == 0 && expression->type == AST_CALL) {
        uint32_t producer = 0;
        if (!semantic_indexed_string_independent_result(expression, ctx)
            && !semantic_collection_owned_string_call_result(expression, ctx, &producer)) return false;
    }
    uint32_t current_source = row->indexed_string_source_id;
    if (current_source == INDEXED_STRING_BORROW_UNKNOWN_SOURCE && expression->type == AST_MEMBER_ACCESS &&
        row->indexed_string_field != NULL && semantic_param_publication_inputs_ready(ctx) &&
        !semantic_indexed_string_field_source_current(ctx, expression, &current_source)) return false;
    if (semantic_param_publication_inputs_ready(ctx)
        && expression->type == AST_IDENTIFIER) {
        if (semantic_indexed_string_definition_selected(ctx, expression))
            return semantic_indexed_string_definition_source_current(ctx,
                expression, source_id);
        const ASTNode *dependency = NULL;
        if (!semantic_indexed_string_borrow_result_dependency_current(
                ctx, expression, &dependency)) return false;
        if (dependency != NULL && !function_param_flow_string_result_source_current(
                ctx, dependency, &current_source)) return false;
    } else if (semantic_param_publication_inputs_ready(ctx)
        && expression->type == AST_CALL
        && current_source == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
        && ast_call_semantic_callee_declared_callable(expression)
        && !function_param_flow_string_result_source_current(ctx, expression, &current_source))
        return false;
    *source_id = current_source;
    return true;
}

/* A builtin store of a String no owner proves alive: `wanted` reports that
 * an owned copy could stand in for the String; `refuse` keeps the refusal
 * as well, where no owned-copy store operation exists. */
typedef struct {
    bool refuse;
    bool wanted;
} IndexedStringStoreCopy;

static void
indexed_string_borrow_argument(ASTNode *expression,
    ASTNode *call, ASTNode *callee, size_t ordinal, bool constructor,
    SemanticContext *ctx, IndexedStringStoreCopy *copy)
{
    uint32_t source_id = INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    if (ctx == NULL || ctx->current_function_decl == NULL) return;
    bool refusal_tail_current =
        semantic_loan_copy_landing_refusal_tail_current(ctx);
    /* A first exact store refusal owns the diagnostic. After its tail moves
     * to another routine, only that routine's first proven store becomes a
     * seed. The program-point owner later expands the seed across its bounded
     * contiguous store run; checking every later store here would cross an
     * intervening-statement ownership boundary. Non-store callers retain the
     * old fail-fast boundary. */
    if (!refusal_tail_current && (copy == NULL
            || semantic_formal_string_store_landing_current_routine_seeded(ctx)))
        return;
    bool current = semantic_indexed_string_borrow_source_current(expression, ctx, &source_id);
    IndexedStringDefinitionRecord definition_use =
        semantic_indexed_string_definition_record_use(ctx, expression, call, ordinal);
    if (definition_use == INDEXED_STRING_DEFINITION_FAILED) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            expression, "A String use has no exact current definition receipt");
        return;
    }
    /* A current aggregate-member carrier may deep-release all of its String
     * elements. Before its first ordinary store withdraws that premise,
     * record the exact call as a copy candidate. Selection is deliberately
     * deferred until the aggregate requirement and all caller moves are
     * sealed: an irreparably reused caller value must not trigger speculative
     * landing rounds inside its callee. `source_id == 0` is not enough to
     * grant transfer because it also describes scalar default parameters. */
    if (copy != NULL && call != NULL && callee == NULL && !constructor
        && ordinal == 1) {
        ASTNode *receiver = ast_call_argument(call, 0);
        uint32_t owned_producer = 0;
        bool owned_call_result =
            semantic_collection_owned_string_call_result(
                expression, ctx, &owned_producer)
            && owned_producer != 0;
        bool independent_result =
            semantic_indexed_string_independent_result(expression, ctx);
        if (receiver != NULL && receiver->type == AST_IDENTIFIER
            && !owned_call_result && !independent_result
            && !indexed_string_path_rooted_in_caller_parameter(
                expression, ctx)
            && semantic_collection_owned_element_member_push_provisional(
                receiver, ctx)
            && !semantic_formal_string_store_landing_value_copy(
                call, ctx)) {
            if (!semantic_formal_string_store_landing_record_member_value_copy_candidate(
                    ctx, call)) {
                semantic_error(ctx, expression,
                    "Aggregate member String copy candidate recording failed");
            }
            return;
        }
    }
    uint32_t independent_producer = 0;
    bool independent_push = source_id == 0 && copy != NULL && call != NULL
        && expression != NULL && callee == NULL && !constructor && ordinal == 1
        && semantic_collection_owned_string_call_result(
            expression, ctx, &independent_producer)
        && independent_producer != 0;
    if (independent_push) {
        /* Returned Arrays have an additional provisional all-elements-owned
         * plan.  Record it when applicable, but the independently produced
         * String itself already has the typed source receipt needed by the
         * general ArrayPush route.  A borrowed-element receiver cannot adopt
         * that owned result.  Select one value-copy landing unless this is
         * already the compiler-inserted independent Clone from that landing;
         * otherwise the call would pass without either an owned or a shallow
         * effect receipt. */
        (void)semantic_formal_string_store_landing_record_owned_array_candidate(
            ctx, call);
        bool value_copy_landed =
            semantic_formal_string_store_landing_value_copy(call, ctx);
        ASTNode *receiver = ast_call_argument(call, 0);
        bool parameter_receiver = false;
        bool parameter_landing = value_copy_landed
            && semantic_collection_owned_string_parameter_push_admit(
                call, receiver, ctx,
                &parameter_receiver);
        bool member_landing = value_copy_landed
            && (semantic_collection_owned_element_member_push_provisional(
                    receiver, ctx)
                || semantic_formal_string_store_landing_member_value_copy_selected(
                    call, ctx));
        bool landed_copy_current = value_copy_landed
            && ast_call_compiler_builtin(expression)
            && ast_call_arg_count(expression) == 1
            && ast_call_argument(expression, 0) != NULL
            && ast_call_argument(expression, 0)->type == AST_IDENTIFIER
            && receiver != NULL && receiver->type == AST_IDENTIFIER
            && semantic_collection_owned_string_push_admissible(
                receiver, ctx);
        /* The compiler-inserted Clone is already the exact independent
         * producer consumed by the general ArrayPush effect receipt.  A
         * private-push receipt would duplicate that authority and, because
         * it validates only the pre-copy shallow effect, would spuriously
         * refine the same call into a second store contract on replay. */
        if (value_copy_landed && !landed_copy_current
            && !parameter_landing && !member_landing) {
            if (parameter_receiver)
                return;
            uint32_t receiver_binding_id = receiver != NULL
                && receiver->type == AST_IDENTIFIER
                ? ast_identifier_binding_syntax_id(receiver) : 0;
            IndexedStringPushRecord recorded =
                semantic_indexed_string_record_private_push(ctx, call,
                    expression, receiver, receiver_binding_id);
            if (recorded != INDEXED_STRING_PUSH_RECORDED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, expression,
                    "A landed String copy has no exact private-push receipt");
                return;
            }
        }
        if (!semantic_collection_owned_string_push_admissible(
                ast_call_argument(call, 0), ctx)
            && !landed_copy_current) {
            copy->wanted = true;
        }
        return;
    }
    if (source_id == 0) return;
    /* A shallow store has no callee that can prove non-retention, so a next
     * analysis copies the String; this also covers a borrowed formal whose
     * payload owner is in the caller's frame. */
    if (call == NULL && callee == NULL
        && source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
        bool selected = semantic_loan_copy_landing_select_string(ctx, expression);
        if (!selected)
            semantic_error(ctx, expression, "String copy landing selection failed");
        if (selected)
            semantic_loan_copy_landing_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, expression,
                "A borrowed String element cannot be retained beyond its Array owner without a copy");
        if (selected) semantic_loan_copy_landing_refusal_tail_seal(ctx);
        return;
    }
    bool named = !constructor && callee != NULL && callee->type == AST_FUNC_DECL
        && ast_func_body(callee) != NULL && !callee->is_async_decl
        && ast_call_semantic_callee_declared_callable(call)
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee)
        && ast_call_semantic_callee_value_binding_id(call) == 0;
    /* A field or element path rooted at a default or ref parameter is caller
     * storage: no local owner holds it and this call cannot release it, as
     * with a String parameter. A declared callee's own checks govern what it
     * retains. Builtin stores still prove their element source below. */
    if (named && source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE &&
        (expression->type == AST_MEMBER_ACCESS || expression->type == AST_ARRAY_ACCESS)) {
        if (indexed_string_path_rooted_in_caller_parameter(expression, ctx))
            return;
    }
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *owner = scope->symbols[i];
            if (!symbol_is_string_array(owner)
                || (source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                    && collection_storage_source_id(owner) != source_id)) continue;
            PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
                semantic_collection_ownership_fact_find(ctx,
                    ast_node_stable_id(ctx->current_function_decl), owner->decl_syntax_id);
            if (owner->is_consumed || (fact != NULL &&
                fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE)) {
                bool reserved = false;
                if (!semantic_indexed_string_borrow_deferred_release_current(ctx,
                        owner->decl_syntax_id, collection_storage_source_id(owner), &reserved)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "A deferred Array release has no current String element source");
                    return;
                }
                if (!reserved) continue;
            }
            if (fact == NULL || fact->element_ownership ==
                    PGY_STRING_ARRAY_BORROWED_ELEMENTS) continue;
            if (source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                && semantic_param_publication_pending(ctx) && call != NULL
                && expression != NULL && expression->type == AST_MEMBER_ACCESS
                && semantic_indexed_string_field_record(ctx, expression, call, ordinal)) return;
            if (source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                && semantic_param_publication_pending(ctx) && expression != NULL
                && call != NULL && callee == NULL && !constructor && ordinal == 1) {
                ASTNode *receiver = ast_call_argument(call, 0);
                IndexedStringPushRecord decision = semantic_indexed_string_record_private_push(
                    ctx, call, expression, receiver, owner->decl_syntax_id);
                if (decision == INDEXED_STRING_PUSH_RECORDED) return;
                if (decision == INDEXED_STRING_PUSH_FAILED) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "A private String push lacks an exact current source receipt");
                    return;
                }
                /* A control-dependent named String with no private source
                 * receipt is copied at this exact store.  The definition
                 * recipe establishes the candidate identity; the owned push
                 * keeps the copy decision at the collection boundary instead
                 * of cloning every reaching definition. */
                if (definition_use == INDEXED_STRING_DEFINITION_RECORDED &&
                    semantic_indexed_string_definition_assignment_use_candidate(
                        ctx, expression, call, ordinal)) {
                    /* The definition recipe is only a candidate until the
                     * publication/flow owners have finished.  Keep the
                     * source AST unchanged in this round and let the landing
                     * plan replay this exact store as an owned push. */
                    if (!semantic_formal_string_store_landing_select_owned(
                            ctx, call)) {
                        semantic_error(ctx, expression,
                            "Owned String push landing selection failed");
                        return;
                    }
                    semantic_loan_copy_landing_error_with_hints(ctx,
                        PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE,
                        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression,
                        "A control-dependent String store is replayed as an owned copy");
                    return;
                }
            }
            if (source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                && semantic_param_publication_pending(ctx) && expression != NULL) {
                FunctionParamPublicationStore *store = ctx->function_param_publications;
                PublishedNode *row = publication_node(store, expression, false);
                PublishedNode *let = expression->type == AST_IDENTIFIER
                    ? publication_node_by_id(store, ast_identifier_binding_syntax_id(expression)) : NULL;
                bool provisional = (expression->type == AST_CALL
                    && ast_call_semantic_callee_declared_callable(expression)
                    && ast_call_semantic_callee_decl_id(expression) != 0)
                    || (let != NULL && let->indexed_string_let.present);
                if (provisional && current && row != NULL &&
                    row->value_type == TYPE_STRING && row->id != 0 &&
                    row->id == ast_node_stable_id(expression) &&
                    row->indexed_string_source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE &&
                    (let == NULL || !let->indexed_string_let.present ||
                        (let->indexed_string_let.available && row->indexed_string_let.present))) {
                    (void)semantic_indexed_string_definition_protect_use(ctx,
                        expression, call, ordinal, false);
                    if (store->indexed_string_shallow_retention_count == SIZE_MAX) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                            expression, "A shallow String store has no current typed source receipt");
                        return;
                    }
                    if (!row->indexed_string_shallow_retention) {
                        row->indexed_string_shallow_retention = true;
                        store->indexed_string_shallow_retention_count++;
                    }
                    return;
                }
                /* Only a withdrawn initializer with a typed replacement enters
                 * the definition proof ahead of the old nonowner exclusion. */
                if (let != NULL && let->indexed_string_let.present &&
                    !let->indexed_string_let.available &&
                    definition_use == INDEXED_STRING_DEFINITION_RECORDED &&
                    semantic_indexed_string_definition_protect_use(ctx, expression,
                        call, ordinal, true)) {
                    if (row == NULL || row->node != expression ||
                        row->id != ast_node_stable_id(expression) || !row->value_present ||
                        !row->string_value || !type_equals(row->value_type, TYPE_STRING) ||
                        !row->indexed_string_source_present ||
                        row->indexed_string_source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE ||
                        store->indexed_string_shallow_retention_count == SIZE_MAX) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                            expression, "A shallow String store has no current definition candidate");
                        return;
                    }
                    if (!row->indexed_string_shallow_retention) {
                        row->indexed_string_shallow_retention = true;
                        store->indexed_string_shallow_retention_count++;
                    }
                    return;
                }
                if (current && call != NULL &&
                    semantic_indexed_string_nonowner_record(ctx, owner, expression, call, ordinal)) continue;
                if (current && call != NULL &&
                    definition_use == INDEXED_STRING_DEFINITION_RECORDED &&
                    row != NULL && row->node == expression &&
                    row->id == ast_node_stable_id(expression) && row->value_present &&
                    row->string_value && type_equals(row->value_type, TYPE_STRING) &&
                    row->indexed_string_source_present &&
                    row->indexed_string_source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE &&
                    semantic_indexed_string_definition_protect_option_use(ctx,
                        expression, call, ordinal)) {
                    if (store->indexed_string_shallow_retention_count == SIZE_MAX) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                            expression, "A shallow String store has no current Option source receipt");
                        return;
                    }
                    if (!row->indexed_string_shallow_retention) {
                        row->indexed_string_shallow_retention = true;
                        store->indexed_string_shallow_retention_count++;
                    }
                    return;
                }
                if (provisional) {
                    /* A call result whose source receipt is still unknown at
                     * a concrete Array<String> store needs one exact value
                     * copy, not every carrier through which the surrounding
                     * aggregate later flows. The next fixed-point round wraps
                     * this expression in Clone and rechecks the ordinary
                     * owned-element transition. */
                    bool selected = copy != NULL && call != NULL
                        && callee == NULL && !constructor
                        && !semantic_loan_copy_landing_satisfied(
                            ctx, expression)
                        && semantic_loan_copy_landing_select_string(
                            ctx, expression);
                    if (selected) {
                        semantic_loan_copy_landing_error_with_hints(ctx,
                            PGY_CODE_SEM_BORROW_ESCAPE,
                            PGY_CAUSE_BORROW_ESCAPE,
                            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                            expression,
                            "A provisional String store result is replayed as one exact owned copy");
                        semantic_loan_copy_landing_refusal_tail_seal(ctx);
                        return;
                    }
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "A shallow String store has no current typed source receipt");
                    return;
                }
            }
            if (named && source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
                if (!semantic_array_storage_record_pending_payload(ctx, owner, call, callee, ordinal) ||
                    !semantic_array_storage_record_pending_call_consumer(ctx, owner, call, ordinal)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "String element handoff has no current source-lifetime receipt");
                    fact->return_elements_owned = false;
                    fact->member_move_elements_current = false;
                }
            } else {
                /* An Array<String> store of a String no owner proves alive
                 * lands as an owned copy of that String. */
                if (copy != NULL && call != NULL && callee == NULL && !constructor) {
                    copy->wanted = true;
                    if (!copy->refuse || !refusal_tail_current) return;
                }
                /* A declared callee's final summary decides whether it keeps
                 * the String; that owner refuses it after Pass 2 if not. */
                if (named && semantic_param_publication_pending(ctx)) {
                    if (semantic_indexed_string_named_call_record(ctx, expression, call, callee, ordinal))
                        continue;
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "A String argument has no current named-call receipt");
                    return;
                }
                /* A retained scalar element invalidates the String lifetime, even
                 * though the Array descriptor never escaped. A builtin store's caller
                 * lands its copy; elsewhere a next analysis copies the String here. */
                fact->return_elements_owned = false;
                fact->member_move_elements_current = false;
                bool selected = false;
                if (!(copy != NULL && call != NULL && callee == NULL && !constructor)) {
                    selected = semantic_loan_copy_landing_select_string(ctx, expression);
                    if (!selected)
                        semantic_error(ctx, expression, "String copy landing selection failed");
                }
                if (selected)
                    semantic_loan_copy_landing_error_with_hints(ctx,
                        PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, expression,
                        "A String element cannot cross an opaque boundary without a current non-retention proof");
                else
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "A String element cannot cross an opaque boundary without a current non-retention proof");
                if (selected) semantic_loan_copy_landing_refusal_tail_seal(ctx);
            }
        }
    }
}

void
semantic_indexed_string_borrow_call_argument(ASTNode *expression,
    ASTNode *call, ASTNode *callee, size_t ordinal, bool constructor,
    SemanticContext *ctx)
{
    indexed_string_borrow_argument(expression, call, callee, ordinal, constructor, ctx, NULL);
}

void
semantic_indexed_string_borrow_return_value(ASTNode *expression,
    ASTNode *return_stmt, SemanticContext *ctx)
{
    uint32_t source_id = INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    bool current = semantic_indexed_string_borrow_source_current(
        expression, ctx, &source_id);
    if (current && source_id == 0)
        return;
    /* An exact nonzero source is still governed by the existing Array-owner
     * transfer/copy rule. The definition recipe exists only for the UNKNOWN
     * join produced by several local replacements; intercepting an exact loan
     * here would duplicate the established last-use transfer decision. */
    if (current && source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
        indexed_string_borrow_argument(
            expression, NULL, NULL, 0, false, ctx, NULL);
        return;
    }
    IndexedStringDefinitionRecord definition_use =
        semantic_indexed_string_definition_record_return(
            ctx, expression, return_stmt);
    if (definition_use == INDEXED_STRING_DEFINITION_FAILED) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            expression,
            "A returned String has no exact current definition receipt");
        return;
    }
    if (definition_use == INDEXED_STRING_DEFINITION_RECORDED
        && semantic_param_publication_pending(ctx)) {
        FunctionParamPublicationStore *store =
            ctx->function_param_publications;
        PublishedNode *row = publication_node(store, expression, false);
        if (row == NULL || row->node != expression ||
            row->id != ast_node_stable_id(expression) ||
            !row->value_present || row->meta || !row->string_value ||
            !type_equals(row->value_type, TYPE_STRING) ||
            !row->indexed_string_source_present ||
            store->indexed_string_shallow_retention_count == SIZE_MAX) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, expression,
                "A returned String has no current definition candidate");
            return;
        }
        if (!row->indexed_string_shallow_retention) {
            row->indexed_string_shallow_retention = true;
            store->indexed_string_shallow_retention_count++;
        }
        return;
    }
    indexed_string_borrow_argument(
        expression, NULL, NULL, 0, false, ctx, NULL);
}

void
semantic_indexed_string_borrow_push_argument(ASTNode *expression,
    ASTNode *call, SemanticContext *ctx, bool *copy_landing)
{
    IndexedStringStoreCopy copy = {.refuse = false};
    indexed_string_borrow_argument(expression, call, NULL, 1, false, ctx, &copy);
    if (copy_landing != NULL) *copy_landing = copy.wanted;
}

void
semantic_indexed_string_borrow_set_argument(ASTNode *expression,
    ASTNode *call, SemanticContext *ctx, bool *copy_wanted)
{
    IndexedStringStoreCopy copy = {.refuse = true};
    indexed_string_borrow_argument(expression, call, NULL, 2, false, ctx, &copy);
    if (copy_wanted != NULL) *copy_wanted = copy.wanted;
}

bool
semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *declaration, ASTNode *initializer, SemanticContext *ctx)
{
    Symbol *source;
    if (binding == NULL)
        return false;
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
        if (semantic_indexed_string_definition_record_birth(ctx, binding,
                declaration, initializer) == INDEXED_STRING_DEFINITION_FAILED) return false;
        if (semantic_param_publication_pending(ctx)
            && semantic_param_publication_local_fact_after_borrow_refusals(
                ctx, ctx->diagnostic_count)
            && ctx->current_function_decl != NULL
            && binding->indexed_string_borrow_source_syntax_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
            uint32_t binding_id = binding->decl_syntax_id;
            uint32_t initializer_id = ast_node_stable_id(initializer);
            uint32_t caller_id = ast_node_stable_id(ctx->current_function_decl);
            PublishedNode *value = publication_node(ctx->function_param_publications, initializer, false);
            bool named = initializer != NULL && (
                (initializer->type == AST_CALL
                    && ast_call_semantic_callee_declared_callable(initializer)
                    && ast_call_semantic_callee_decl_id(initializer) != 0)
                || (initializer->type == AST_IDENTIFIER && value != NULL
                    && value->indexed_string_let.present));
            if (!named) return true;
            if (declaration == NULL || declaration->type != AST_LET_DECL
                || ast_let_initializer(declaration) != initializer || binding_id == 0
                || binding_id != ast_node_stable_id(declaration) || initializer_id == 0
                || caller_id == 0 || value == NULL || !value->value_present
                || value->value_shape != initializer->type || value->value_type != TYPE_STRING)
                return false;
            PublishedNode *row = publication_node(ctx->function_param_publications, declaration, true);
            if (row == NULL) return false;
            IndexedStringLetLineage *lineage = &row->indexed_string_let;
            if (lineage->present) return lineage->declaration == declaration
                && lineage->initializer == initializer
                && lineage->caller == ctx->current_function_decl
                && lineage->declaration_id == binding_id
                && lineage->initializer_id == initializer_id
                && lineage->caller_id == caller_id
                && lineage->binding_id == binding_id
                && lineage->source_id == binding->indexed_string_borrow_source_syntax_id
                && lineage->initializer_shape == initializer->type
                && lineage->dependent;
            *lineage = (IndexedStringLetLineage){declaration, initializer,
                ctx->current_function_decl, binding_id, initializer_id, caller_id,
                binding_id, binding->indexed_string_borrow_source_syntax_id,
                initializer->type, true, true, true};
        }
    }
    return true;
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
        semantic_array_storage_deferred_string_read_assignment(binding, source_id, true, assignment, value, ctx);
        return;
    }
    if (type_equals(binding->type, TYPE_STRING)) {
        if (semantic_indexed_string_definition_record_assignment(ctx, binding,
                assignment, value) == INDEXED_STRING_DEFINITION_FAILED) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                assignment, "A String replacement has no exact definition receipt");
            return;
        }
        uint32_t incoming = borrow_source_for_expression(value, ctx);
        binding->indexed_string_borrow_source_syntax_id = borrow_source_join(
            binding->indexed_string_borrow_source_syntax_id, incoming);
        if (semantic_param_publication_pending(ctx)) {
            PublishedNode *let = publication_node_by_id(ctx->function_param_publications,
                binding->decl_syntax_id);
            if (let != NULL && let->indexed_string_let.present)
                let->indexed_string_let.available = false;
        }
        semantic_array_storage_deferred_string_read_assignment(binding, incoming, false, assignment, value, ctx);
    }
}

static void
indexed_string_borrow_invalidate_owner_source(
    ASTNode *receiver, SemanticContext *ctx, bool owner_release)
{
    Symbol *owner = identifier_symbol(receiver, ctx);
    uint32_t source_id;
    if (!symbol_is_string_array(owner) || ctx == NULL)
        return;
    source_id = collection_storage_source_id(owner);
    if (source_id == 0)
        return;
    if (ctx->in_defer_cleanup
        && !semantic_array_storage_record_deferred_string_read(
            receiver, source_id, owner, false, owner_release, ctx))
        return;
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *candidate = scope->symbols[i];
            if (candidate != NULL && type_equals(candidate->type, TYPE_STRING) &&
                (source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                 || candidate->indexed_string_borrow_source_syntax_id ==
                        INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                 || candidate->indexed_string_borrow_source_syntax_id ==
                        source_id)) {
                candidate->indexed_string_borrow_invalidated = true;
            }
        }
    }
}

void
semantic_indexed_string_borrow_invalidate_deep_drop(
    ASTNode *receiver, SemanticContext *ctx)
{
    indexed_string_borrow_invalidate_owner_source(receiver, ctx, true);
}

void
semantic_indexed_string_borrow_invalidate_owned_set(
    ASTNode *receiver, SemanticContext *ctx)
{
    indexed_string_borrow_invalidate_owner_source(receiver, ctx, false);
}

bool
semantic_indexed_string_borrow_validate_use(
    Symbol *binding, ASTNode *use, SemanticContext *ctx)
{
    if (binding != NULL && type_equals(binding->type, TYPE_STRING)
        && !semantic_array_storage_record_deferred_string_read(use, binding->indexed_string_borrow_source_syntax_id,
            binding, false, false, ctx)) return false;
    if (binding == NULL ||
        binding->indexed_string_borrow_source_syntax_id == 0 ||
        !binding->indexed_string_borrow_invalidated)
        return true;
    bool selected_independent = false;
    if (binding->indexed_string_borrow_source_syntax_id ==
            INDEXED_STRING_BORROW_UNKNOWN_SOURCE)
        selected_independent =
            semantic_indexed_string_definition_select_independent_storage(
                ctx, binding->decl_syntax_id);
    if (selected_independent) {
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, use,
            "An ambiguous String loan that survives an owned Array replacement is copied at each reaching definition");
        semantic_loan_copy_landing_refusal_tail_seal(ctx);
        return false;
    }
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, use,
        "String '%s' borrows an Array<String> element that was replaced or released.\n"
        "Reason:\n"
        "- an ownership-aware ArraySet or ArrayDropOwnedStrings retired a payload from the exact storage source\n"
        "- using the indexed alias afterwards would read freed memory\n"
        "Fix:\n"
        "- copy the String with Concat(\"\", value) before the deep drop\n"
        "- or keep the Array<String> owner live until the final alias use",
        binding->name != NULL ? binding->name : "<string>");
    return false;
}
