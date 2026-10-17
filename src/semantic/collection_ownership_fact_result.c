#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "indexed_string_borrow_owner.h"
#include "../parser/ast_api.h"

struct PgyCollectionEffectSnapshot {
    const ASTNode *program;
    uint32_t program_id;
    PgyCollectionEffectCaller *callers;
    FunctionParamFlowIndexClaims **claims;
    PgyCollectionExpectedEffect *effects;
    size_t caller_count, effect_count, sealed_callers, sealed_effects;
};

void semantic_collection_effect_snapshot_destroy(PgyCollectionEffectSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    for (size_t i = 0; i < snapshot->caller_count; i++)
        function_param_flow_claims_destroy(snapshot->claims[i]);
    free(snapshot->claims);
    free(snapshot->callers);
    free(snapshot->effects);
    free(snapshot);
}

bool semantic_collection_effect_snapshot_current(const PgyCollectionEffectSnapshot *s,
    const ASTNode *program, PgyCollectionEffectView *out)
{
    if (out != NULL) *out = (PgyCollectionEffectView){0};
    if (s == NULL || program == NULL || program->type != AST_PROGRAM ||
        s->program != program || s->program_id == 0 ||
        s->program_id != ast_node_stable_id(program) ||
        s->caller_count != s->sealed_callers || s->effect_count != s->sealed_effects ||
        (s->caller_count != 0 && (s->callers == NULL || s->claims == NULL)) ||
        (s->effect_count != 0 && s->effects == NULL)) return false;
    for (size_t i = 0; i < s->caller_count; i++) {
        const PgyCollectionEffectCaller *c = &s->callers[i];
        for (size_t j = 0; j < i; j++)
            if (s->callers[j].decl == c->decl || s->callers[j].decl_id == c->decl_id ||
                s->claims[j] == s->claims[i]) return false;
        if (c->decl == NULL || c->decl->type != AST_FUNC_DECL || c->decl_id == 0 ||
            c->decl_id != ast_node_stable_id(c->decl) || c->body == NULL ||
            c->body != ast_func_body(c->decl) || c->body_id == 0 ||
            c->body_id != ast_node_stable_id(c->body) ||
            !function_param_flow_syntax_claims_current(NULL, s->claims[i])) return false;
        bool root = false;
        for (size_t r = 0; r < s->claims[i]->count; r++)
            if (s->claims[i]->rows[r].node == c->body &&
                s->claims[i]->rows[r].id == c->body_id) root = true;
        if (!root) return false;
    }
    for (size_t i = 0; i < s->effect_count; i++) {
        const PgyCollectionExpectedEffect *e = &s->effects[i];
        uint32_t kind = 0, receiver = 0, source = 0;
        if (e->caller >= s->caller_count || e->call == NULL || e->call->type != AST_CALL ||
            e->id == 0 || e->id != ast_node_stable_id(e->call) ||
            e->kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION || e->receiver == 0 || e->source != 0 ||
            !ast_call_semantic_collection_effect(e->call, &kind, &receiver, &source) ||
            kind != e->kind || receiver != e->receiver || source != e->source) return false;
        bool member = false;
        const FunctionParamFlowIndexClaims *claims = s->claims[e->caller];
        for (size_t r = 0; r < claims->count; r++)
            if (claims->rows[r].node == e->call && claims->rows[r].id == e->id) member = true;
        if (!member) return false;
    }
    if (out != NULL) *out = (PgyCollectionEffectView){s->callers, s->caller_count,
        s->effects, s->effect_count};
    return true;
}

bool semantic_collection_effect_snapshot_call(const PgyCollectionEffectSnapshot *s,
    size_t caller, const ASTNode *call, size_t *effect)
{
    if (effect == NULL) return false;
    *effect = SIZE_MAX;
    if (s == NULL || caller >= s->caller_count || s->caller_count != s->sealed_callers ||
        s->effect_count != s->sealed_effects || s->claims == NULL ||
        call == NULL || call->type != AST_CALL || ast_node_stable_id(call) == 0 ||
        (s->effect_count != 0 && s->effects == NULL)) return false;
    const FunctionParamFlowIndexClaims *claims = s->claims[caller];
    if (claims == NULL || claims->failed || claims->rows == NULL) return false;
    const FunctionParamFlowIndexClaim *matched = NULL;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[r];
        if (row->node != call) continue;
        if (matched != NULL || row->shape != AST_CALL || row->id != ast_node_stable_id(call) ||
            !row->snapshot_complete || row->snapshot_active) return false;
        matched = row;
    }
    if (matched == NULL) return false;
    uint32_t kind = 0, receiver = 0, source = 0;
    bool present = ast_call_semantic_collection_effect(call, &kind, &receiver, &source);
    if (present != matched->collection_effect_present || kind != matched->collection_effect_kind ||
        receiver != matched->collection_effect_receiver || source != matched->collection_effect_source) return false;
    for (size_t i = 0; i < s->effect_count; i++) {
        const PgyCollectionExpectedEffect *e = &s->effects[i];
        if (e->call != call) continue;
        if (*effect != SIZE_MAX || e->caller != caller || e->id != matched->id ||
            !present || e->kind != kind || e->receiver != receiver || e->source != source) return false;
        *effect = i;
    }
    return true;
}

bool semantic_collection_effect_snapshot_take(SemanticContext *ctx,
    PgyCollectionEffectSnapshot **out)
{
    if (out == NULL || *out != NULL || ctx == NULL || ctx->has_error ||
        !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *pub = ctx->function_param_publications;
    size_t count = pub->private_string_push_count;
    if (count > SIZE_MAX / sizeof(PgyCollectionExpectedEffect) ||
        count > SIZE_MAX / sizeof(PgyCollectionEffectCaller) ||
        count > SIZE_MAX / sizeof(FunctionParamFlowIndexClaims *)) return false;
    PgyCollectionEffectSnapshot *s = calloc(1, sizeof(*s));
    if (s == NULL) return false;
    s->program = pub->program;
    s->program_id = pub->program_id;
    s->effects = count == 0 ? NULL : calloc(count, sizeof(*s->effects));
    s->callers = count == 0 ? NULL : calloc(count, sizeof(*s->callers));
    s->claims = count == 0 ? NULL : calloc(count, sizeof(*s->claims));
    if (count != 0 && (s->effects == NULL || s->callers == NULL || s->claims == NULL)) goto fail;
    for (size_t n = 0; n < pub->node_capacity; n++) {
        PublishedNode *row = &pub->nodes[n];
        if (row->private_string_push == NULL) continue;
        if (s->effect_count >= count) goto fail;
        s->effects[s->effect_count++].call = row->node;
    }
    if (s->effect_count != count) goto fail;
    for (size_t i = 0; i < count; i++)
        if (!semantic_indexed_string_private_push_current(ctx, s->effects[i].call)) goto fail;
    if (pub->private_string_push_count != count) goto fail;
    /* Currentness queries may grow the index vector. Reacquire only afterwards. */
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    for (size_t i = 0; i < count; i++) {
        PgyCollectionExpectedEffect *e = &s->effects[i];
        PublishedNode *row = publication_node(pub, e->call, false);
        const ASTNode *caller = row == NULL ? NULL : row->caller;
        size_t c = 0;
        for (; c < s->caller_count; c++) if (s->callers[c].decl == caller) break;
        if (c == s->caller_count) {
            FunctionParamFlowProgramPointIndex *index = NULL;
            if (flow == NULL || flow->failed || flow->solving) goto fail;
            for (size_t p = 0; p < flow->program_point_index_count; p++)
                if (flow->program_point_indexes[p].function_decl == caller)
                    index = &flow->program_point_indexes[p];
            if (caller == NULL || caller->type != AST_FUNC_DECL || index == NULL ||
                index->function_id != ast_node_stable_id(caller) ||
                index->dependency_body != ast_func_body(caller) ||
                index->dependency_body_id != ast_node_stable_id(index->dependency_body) ||
                !function_param_flow_syntax_claims_current(NULL, index->claims)) goto fail;
            s->callers[c] = (PgyCollectionEffectCaller){caller, index->dependency_body,
                index->function_id, index->dependency_body_id};
            s->claims[c] = index->claims;
            s->caller_count++;
        }
        e->caller = c;
        e->id = ast_node_stable_id(e->call);
        if (!ast_call_semantic_collection_effect(e->call, &e->kind, &e->receiver, &e->source)) goto fail;
    }
    s->sealed_callers = s->caller_count;
    s->sealed_effects = s->effect_count;
    if (!semantic_collection_effect_snapshot_current(s, ctx->program_root, NULL)) goto fail;
    /* All validation/allocation is complete: the sole ownership handoff starts. */
    for (size_t c = 0; c < s->caller_count; c++)
        for (size_t p = 0; p < flow->program_point_index_count; p++)
            if (flow->program_point_indexes[p].claims == s->claims[c])
                flow->program_point_indexes[p].claims = NULL;
    *out = s;
    return true;
fail:
    /* Claims are still owned by their indexes until the successful handoff. */
    s->caller_count = 0;
    semantic_collection_effect_snapshot_destroy(s);
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
    if (fact != NULL && (fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS
        || (fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
            && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE))) {
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
    if (semantic_array_storage_has_pending(binding)
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_MUTATE_OWNED_STRINGS)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver, "ArrayPushOwnedString requires current preserved array storage in '%s'",
            name);
        return false;
    }
    return true;
}

bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name)
{
    ASTNode *callee;
    const char *name;

    if (expr == NULL || expr->type != AST_CALL || expected_name == NULL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    return name != NULL && strcmp(name, expected_name) == 0
        && ast_identifier_binding_syntax_id(callee) == 0;
}

bool
semantic_collection_record_call_effect(ASTNode *call, ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind, SemanticContext *ctx)
{
    return semantic_collection_record_call_effect_from(call, receiver, kind, 0, ctx);
}

bool
semantic_collection_record_call_effect_from(ASTNode *call, ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind, uint32_t source_syntax_id, SemanticContext *ctx)
{
    Symbol *binding = NULL;
    uint32_t binding_id, issued_kind = 0, issued_binding = 0, issued_source = 0;
    if (call == NULL || call->type != AST_CALL || receiver == NULL
        || kind <= PGY_COLLECTION_EFFECT_NONE || kind > PGY_COLLECTION_EFFECT_DROP
        || (source_syntax_id != 0 && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH))
        return false;
    PgyCollectionOwnershipFact *fact = receiver_collection_fact(receiver, ctx, &binding);
    if (fact == NULL)
        return !ast_call_semantic_collection_effect(call, NULL, NULL, NULL);
    bool owned_local_drop = kind == PGY_COLLECTION_EFFECT_DROP
        && (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        && (fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            || fact->origin == PGY_COLLECTION_ORIGIN_CLONE
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING);
    bool borrowed_shallow = fact->origin == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL
        && fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && collection_private_push_recorded(ctx, call, receiver,
            receiver_binding_syntax_id(receiver, binding));
    if (fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
        && !owned_local_drop && !borrowed_shallow)
        return semantic_array_storage_record_return_element_effect(receiver, kind,
            !ast_call_semantic_collection_effect(call, NULL, NULL, NULL), ctx);
    if (fact->disposition != (owned_local_drop
            ? PGY_COLLECTION_DISPOSITION_RETIRED
            : PGY_COLLECTION_DISPOSITION_LIVE)) return false;
    binding_id = receiver_binding_syntax_id(receiver, binding);
    if (binding == NULL || !collection_array_string_type(binding->type) || binding_id == 0
        || binding->decl_syntax_id != binding_id) return false;
    /* The parser receipt stays write-once; only its exact recheck is valid. */
    return semantic_array_storage_record_return_element_effect(receiver, kind,
        ast_call_set_semantic_collection_effect(call, (uint32_t)kind, binding_id,
            source_syntax_id) || (ast_call_semantic_collection_effect(
        call, &issued_kind, &issued_binding, &issued_source)
        && issued_kind == (uint32_t)kind && issued_binding == binding_id
        && issued_source == source_syntax_id), ctx);
}

static ASTNode *
collection_direct_let_for_binding(ASTNode *body, uint32_t binding_syntax_id)
{
    if (body == NULL || body->type != AST_BLOCK || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt != NULL && stmt->type == AST_LET_DECL
            && ast_node_stable_id(stmt) == binding_syntax_id)
            return stmt;
    }
    return NULL;
}

typedef struct OwnedStringResultVisit {
    const ASTNode *decl;
    const struct OwnedStringResultVisit *parent;
} OwnedStringResultVisit;

static bool collection_callable_type_returns_owned_string_at(const ASTNode *call,
    SemanticContext *ctx, uint32_t *producer_id_out, const OwnedStringResultVisit *parent);

static bool collection_owned_string_source_current(const ASTNode *call,
    SemanticContext *ctx, const OwnedStringResultVisit *parent)
{
    if (ctx == NULL || !publication_generation_ready(ctx) ||
        (!ctx->function_param_publications->pending && !ctx->function_param_publications->inputs_ready)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || call == NULL || row->node != call || row->id != ast_node_stable_id(call) ||
        call->type != AST_CALL || !publication_call_ready(row) ||
        !row->value_present || row->meta || row->value_shape != AST_CALL ||
        row->value_type != TYPE_STRING || !row->string_value) return false;
    for (size_t p = 0; p < row->argument_count; p++) {
        const PublishedCallArgument *arg = &row->arguments[p];
        PublishedNode *value = publication_node(ctx->function_param_publications, arg->node, false);
        if (value == NULL || value->node != arg->node || value->id != arg->id ||
            !value->value_present || value->meta || value->value_type == NULL ||
            value->value_type == TYPE_UNKNOWN || value->value_type != arg->type ||
            value->value_shape != arg->shape || (arg->shape == AST_IDENTIFIER && value->binding_id != arg->binding) ||
            (arg->shape == AST_CALL && !publication_call_ready(value))) return false;
    }
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH)
        return semantic_param_publication_checked_finish_independent(ctx, call);
    if (row->kind_present && row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib &&
        row->declaration == 0 && row->value_binding == 0 && row->target_binding == 0 &&
        row->target_shape == AST_IDENTIFIER && row->target_name != NULL &&
        strcmp(row->target_name, "Concat") == 0 && row->argument_count == 2 &&
        row->arguments[0].type == TYPE_STRING && row->arguments[1].type == TYPE_STRING) return true;
    return collection_callable_type_returns_owned_string_at(call, ctx, NULL, parent);
}

static bool collection_owned_string_receipt_current(ASTNode *decl, SemanticContext *ctx,
    const OwnedStringResultVisit *parent)
{
    for (const OwnedStringResultVisit *visit = parent; visit != NULL; visit = visit->parent)
        if (visit->decl == decl) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, decl, false);
    PublishedOwnedStringResult *receipt = row == NULL ? NULL : row->owned_string_result;
    if (row == NULL || row->node != decl || row->id != ast_node_stable_id(decl) ||
        receipt == NULL || receipt->body == NULL || receipt->body != ast_func_body(decl) ||
        receipt->body_id == 0 || receipt->body_id != ast_node_stable_id(receipt->body) ||
        receipt->source_count == 0 || receipt->source_count != receipt->sealed_source_count ||
        receipt->sources == NULL || receipt->syntax == NULL ||
        !semantic_param_publication_routine_inputs_current(ctx, decl, true) ||
        !function_param_flow_syntax_claims_current(ctx, receipt->syntax)) return false;
    OwnedStringResultVisit visit = {decl, parent};
    for (size_t p = 0; p < receipt->source_count; p++) {
        const PublishedCallArgument *source = &receipt->sources[p];
        if (source->node == NULL || source->shape != AST_CALL || source->node->type != AST_CALL ||
            source->id == 0 || source->id != ast_node_stable_id(source->node) || source->type != TYPE_STRING ||
            source->binding != ast_identifier_binding_syntax_id(source->node) ||
            !collection_owned_string_source_current(source->node, ctx, &visit)) return false;
    }
    return true;
}

static bool
collection_callable_type_returns_owned_string_at(const ASTNode *call,
    SemanticContext *ctx, uint32_t *producer_id_out, const OwnedStringResultVisit *parent)
{
    if (producer_id_out != NULL) *producer_id_out = 0;
    if (ctx == NULL || !publication_generation_ready(ctx) ||
        (!ctx->function_param_publications->pending && !ctx->function_param_publications->inputs_ready)) return false;
    ASTNode *callee = call == NULL ? NULL : ast_call_callee(call);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER ? ast_identifier_name(callee) : NULL;
    uint32_t producer = call == NULL ? 0 : ast_call_semantic_callee_decl_id(call);
    ASTNode *decl = name == NULL ? NULL : semantic_find_callable_decl_by_name(ctx, name);
    Symbol *symbol = name == NULL ? NULL : scope_lookup(ctx->scope, name);
    PublishedNode *value = publication_node(ctx->function_param_publications, call, false);
    if (producer == 0 || decl == NULL || decl->type != AST_FUNC_DECL || ast_node_stable_id(decl) != producer ||
        symbol == NULL || symbol->kind != SYMBOL_FUNCTION || symbol->decl_syntax_id != producer ||
        symbol->type == NULL || symbol->type->kind != TYPE_KIND_FUNCTION ||
        !type_function_has_body_summary(symbol->type) ||
        !type_equals(type_function_return_type(symbol->type), TYPE_STRING) ||
        (type_function_body_summary(symbol->type) & BODY_SUMMARY_RETURNS_OWNED_STRING) == 0 ||
        value == NULL || value->node != call || value->id != ast_node_stable_id(call) ||
        !publication_call_ready(value) || !value->value_present || value->meta ||
        value->value_type != TYPE_STRING || !value->string_value || value->value_shape != AST_CALL) return false;
    for (size_t p = 0; p < value->argument_count; p++) {
        const PublishedCallArgument *arg = &value->arguments[p];
        PublishedNode *actual = publication_node(ctx->function_param_publications, arg->node, false);
        if (actual == NULL || actual->node != arg->node || actual->id != arg->id ||
            !actual->value_present || actual->meta || actual->value_type == NULL ||
            actual->value_type == TYPE_UNKNOWN || actual->value_type != arg->type ||
            actual->value_shape != arg->shape || (arg->shape == AST_IDENTIFIER && actual->binding_id != arg->binding) ||
            (arg->shape == AST_CALL && !publication_call_ready(actual))) return false;
    }
    if (!collection_owned_string_receipt_current(decl, ctx, parent)) return false;
    if (producer_id_out != NULL) *producer_id_out = producer;
    return true;
}

static void collection_owned_string_receipt_destroy(PublishedOwnedStringResult *receipt)
{
    if (receipt == NULL) return;
    function_param_flow_claims_destroy(receipt->syntax);
    free(receipt->sources);
    free(receipt);
}

static bool
collection_owned_string_expression_ready(ASTNode *body,
                                         const ASTNode *expression,
                                         SemanticContext *ctx,
                                         unsigned depth, PublishedOwnedStringResult *receipt)
{
    ASTNode *decl;
    uint32_t binding_id;

    if (body == NULL || expression == NULL || depth > 8)
        return false;
    if (expression->type == AST_IDENTIFIER) {
        binding_id = ast_identifier_binding_syntax_id(expression);
        decl = collection_direct_let_for_binding(body, binding_id);
        /* A named initializer is only a straight-line provenance proof.
         * Control-dependent rebinding needs a value fact, not the old let. */
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            ASTNode *stmt = ast_block_statement(body, i);
            if (stmt == NULL)
                return false;
            if (stmt->type == AST_ASSIGNMENT) {
                ASTNode *target = ast_assignment_target(stmt);
                if (target != NULL && target->type == AST_IDENTIFIER
                    && ast_identifier_binding_syntax_id(target) == binding_id)
                    return false;
            } else if (stmt->type != AST_RETURN && stmt->type != AST_LET_DECL
                       && stmt->type != AST_CALL
                       && stmt->type != AST_DEFER_STMT) {
                return false;
            }
        }
        return decl != NULL && !ast_let_is_mutable(decl)
            && collection_owned_string_expression_ready(
                body, ast_let_initializer(decl), ctx, depth + 1, receipt);
    }
    if (expression->type != AST_CALL)
        return false;
    if (!collection_owned_string_source_current(expression, ctx, NULL)) return false;
    for (size_t p = 0; p < receipt->source_count; p++)
        if (receipt->sources[p].node == expression) return true;
    if (receipt->source_count == SIZE_MAX / sizeof(*receipt->sources)) return false;
    PublishedCallArgument *sources = realloc(receipt->sources,
        (receipt->source_count + 1) * sizeof(*sources));
    if (sources == NULL) return false;
    receipt->sources = sources;
    sources[receipt->source_count++] = (PublishedCallArgument){expression,
        ast_node_stable_id(expression), expression->type,
        ast_identifier_binding_syntax_id(expression), TYPE_STRING, 0, 0};
    return true;
}

bool
semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out)
{
    return collection_callable_type_returns_owned_string_at(
        expression, ctx, producer_syntax_id_out, NULL);
}

static bool
collection_owned_string_returns_ready(ASTNode *node, ASTNode *function_body,
                                      SemanticContext *ctx,
                                      bool *saw_return, unsigned depth, PublishedOwnedStringResult *receipt)
{
    if (node == NULL)
        return true;
    if (depth > 8)
        return false;
    if (node->type == AST_RETURN) {
        *saw_return = true;
        return collection_owned_string_expression_ready(
            function_body, ast_return_value(node), ctx, 0, receipt);
    }
    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            ASTNode *stmt = ast_block_statement(node, i);
            if (stmt == NULL || !collection_owned_string_returns_ready(
                    stmt, function_body, ctx, saw_return, depth + 1, receipt)) {
                return false;
            }
        }
        return true;
    }
    if (node->type == AST_IF_STMT) {
        return collection_owned_string_returns_ready(
                   ast_if_then_branch(node), function_body, ctx,
                   saw_return, depth + 1, receipt)
            && collection_owned_string_returns_ready(
                   ast_if_else_branch(node), function_body, ctx,
                   saw_return, depth + 1, receipt);
    }
    /* Lambda bodies are not enclosing-function returns. Other control owners
     * remain unadmitted until their return facts are explicitly supported. */
    return node->type == AST_LET_DECL || node->type == AST_CALL
        || node->type == AST_ASSIGNMENT || node->type == AST_DEFER_STMT;
}

void
semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx)
{
    bool saw_return = false;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL || ctx == NULL ||
        ctx->function_param_publications == NULL) return;
    PublishedNode *row = publication_node(ctx->function_param_publications, function_decl, false);
    if (row != NULL) {
        collection_owned_string_receipt_destroy(row->owned_string_result);
        row->owned_string_result = NULL;
    }
    if (!publication_generation_ready(ctx) || !ctx->tracking_function_effects || !type_equals(ctx->current_return, TYPE_STRING) ||
        !ctx->function_param_publications->pending) return;
    PublishedOwnedStringResult *receipt = calloc(1, sizeof(*receipt));
    if (receipt == NULL) return;
    ASTNode *body = ast_func_body(function_decl);
    if (body != NULL && body->type == AST_BLOCK
        && collection_owned_string_returns_ready(
            body, body, ctx, &saw_return, 0, receipt) && saw_return &&
        receipt->source_count != 0 && ast_node_stable_id(body) != 0 &&
        function_param_flow_syntax_claims_snapshot(ctx, &receipt->syntax, body)) {
        receipt->body = body; receipt->body_id = ast_node_stable_id(body);
        receipt->sealed_source_count = receipt->source_count;
        row = publication_node(ctx->function_param_publications, function_decl, true);
        if (row == NULL) { collection_owned_string_receipt_destroy(receipt); return; }
        row->owned_string_result = receipt;
        semantic_record_body_summary(
            ctx, BODY_SUMMARY_RETURNS_OWNED_STRING);
    } else collection_owned_string_receipt_destroy(receipt);
}
