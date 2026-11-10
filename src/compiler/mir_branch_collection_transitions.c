#include "mir_branch_source_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "mir_decl_headers.h"
#include "collection_owned_local_transition_owner.h"
#include "mir_call_fact.h"
#include "../semantic/builtin_kind.h"
#include "mir_json_expression_graph_materialize.h"
#include "mir_hir_fact_transfer.h"
#include "mir_loop_collection_cleanup.h"

#include "mir_branch_source_facts_internal.h"

typedef enum
{
    MIR_COLLECTION_STATE_BOTTOM = 0,
    MIR_COLLECTION_STATE_UNBORN = 1,
    MIR_COLLECTION_STATE_EMPTY = 2,
    MIR_COLLECTION_STATE_BORROWED = 4,
    MIR_COLLECTION_STATE_OWNED = 8,
    MIR_COLLECTION_STATE_RETIRED = 16
} MIRCollectionTransitionState;

/* A join retains every possible lifetime, not permission from one path. */

static const char *
mir_collection_call_name(const MIRInstruction *inst)
{
    ASTNode *callee;

    if (inst == NULL || inst->expr0 == NULL
        || inst->expr0->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(inst->expr0);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(callee) != 0
        || !ast_call_semantic_callee_is_stdlib(inst->expr0)) {
        return NULL;
    }
    return ast_identifier_name(callee);
}

static bool
mir_collection_effect_call_shape_ready(const MIRInstruction *inst,
                                       const char *name)
{
    bool expected = false;

    if (inst == NULL || name == NULL)
        return false;
    switch ((PgyCollectionOwnershipEffectKind)
                inst->collection_ownership_effect_kind) {
    case PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH:
        if (strcmp(name, "ArrayPushOwnedString") == 0) {
            expected = inst->collection_ownership_source_binding_id == 0;
        } else if (strcmp(name, "ArrayPush") == 0) {
            ASTNode *value = ast_call_argument(inst->expr0, 1);
            expected = value != NULL && value->type == AST_CALL
                && inst->collection_ownership_source_binding_id != 0
                && (ast_call_semantic_callee_decl_id(value)
                        == inst->collection_ownership_source_binding_id
                    || ast_node_stable_id(value)
                        == inst->collection_ownership_source_binding_id);
        }
        break;
    case PGY_COLLECTION_EFFECT_SHALLOW_MUTATION:
        expected = strcmp(name, "ArrayPush") == 0
            || strcmp(name, "ArraySet") == 0
            || strcmp(name, "ArrayPop") == 0;
        break;
    case PGY_COLLECTION_EFFECT_DROP:
        expected = strcmp(name, "ArrayDropOwnedStrings") == 0;
        break;
    case PGY_COLLECTION_EFFECT_OWNED_STRING_SET:
        expected = strcmp(name, "ArraySetOwnedString") == 0;
        break;
    case PGY_COLLECTION_EFFECT_OWNED_STRING_POP:
        expected = strcmp(name, "ArrayPop") == 0;
        break;
    default:
        expected = false;
        break;
    }
    return inst->has_collection_ownership_receipt && expected
        && (inst->collection_ownership_effect_kind
                == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
            || inst->collection_ownership_source_binding_id == 0);
}

static bool
mir_collection_receipt_shape_ready(const MIRInstruction *inst,
                                   uint32_t binding_syntax_id)
{
    const char *name;
    ASTNode *receiver;
    uint32_t receiver_id;

    if (inst == NULL)
        return false;
    name = mir_collection_call_name(inst);
    if (name == NULL)
        return !inst->has_collection_ownership_receipt;
    receiver = ast_call_argument(inst->expr0, 0);
    receiver_id = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    if (receiver_id != binding_syntax_id)
        return !inst->has_collection_ownership_receipt;
    return mir_collection_effect_call_shape_ready(inst, name)
        && inst->collection_ownership_receiver_binding_id
            == binding_syntax_id;
}

/* Parameter storage is owned by the interprocedural carriage seam, not by a
 * local collection-lifetime fact.  The semantic snapshot has already proved
 * the exact call receipt before this check; here we only prove that its
 * receiver is the one mutable Array<String> parameter identity carried by the
 * current MIR signature. */
static bool
mir_collection_parameter_receipt_ready(const MIRRoutine *routine,
                                        const MIRInstruction *inst)
{
    uint32_t binding_syntax_id;
    size_t matches = 0;

    if (routine == NULL || inst == NULL
        || !inst->has_collection_ownership_receipt)
        return false;
    binding_syntax_id = inst->collection_ownership_receiver_binding_id;
    if (binding_syntax_id == 0
        || !mir_collection_receipt_shape_ready(inst, binding_syntax_id))
        return false;

    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        const char *type_name;
        MIRParamCarriage carriage;

        if (param == NULL
            || ast_func_param_stable_id(param) != binding_syntax_id)
            continue;
        matches++;
        type_name = mir_routine_param_type_name(routine, i);
        carriage = mir_routine_param_carriage(routine, i);
        if (matches != 1 || type_name == NULL
            || strcmp(type_name, "Array<String>") != 0
            || carriage == MIR_PARAM_CARRIAGE_READONLY_REF
            || carriage != mir_param_carriage_from_source_mode(param->mode))
            return false;
    }
    return matches == 1;
}

/* A direct field receiver has no standalone collection fact. Its receipt
 * identity is the exact leaf field declaration, while either a parameter or
 * one MIR local owns the aggregate storage. Recompute the full path from MIR
 * declaration headers; the sealed semantic snapshot remains the authority
 * that the local root was immutable and its selected field owned. */
static bool
mir_collection_member_receipt_ready(const MIRRoutine *routine,
                                    const MIRInstruction *inst)
{
    ASTNode *receiver;
    uint32_t root_binding_syntax_id = 0;
    uint32_t leaf_field_syntax_id;
    size_t matches = 0;
    const MIRSourceLocalType *local;

    if (routine == NULL || inst == NULL
        || !inst->has_collection_ownership_receipt
        || inst->collection_ownership_effect_kind
            != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        || !mir_collection_effect_call_shape_ready(
            inst, mir_collection_call_name(inst)))
        return false;
    receiver = ast_call_argument(inst->expr0, 0);
    leaf_field_syntax_id =
        inst->collection_ownership_receiver_binding_id;
    if (receiver == NULL || receiver->type != AST_MEMBER_ACCESS
        || leaf_field_syntax_id == 0
        || !mir_collection_member_receiver_path_matches(
            routine, receiver, leaf_field_syntax_id,
            &root_binding_syntax_id))
        return false;

    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        MIRParamCarriage carriage;
        if (param == NULL
            || ast_func_param_stable_id(param) != root_binding_syntax_id)
            continue;
        matches++;
        carriage = mir_routine_param_carriage(routine, i);
        if (matches != 1
            || (param->mode != PARAM_MODE_DEFAULT
                && param->mode != PARAM_MODE_MUT_REF)
            || carriage == MIR_PARAM_CARRIAGE_READONLY_REF
            || carriage != mir_param_carriage_from_source_mode(param->mode))
            return false;
    }
    if (matches == 1)
        return true;
    if (matches != 0)
        return false;
    local = mir_routine_source_local_type_fact_by_binding_syntax_id(
        routine, root_binding_syntax_id);
    return local != NULL
        && local->binding_syntax_id == root_binding_syntax_id
        && local->name != NULL && local->name[0] != '\0'
        && local->type_name != NULL && local->type_name[0] != '\0';
}

static bool
mir_collection_relevant_call_for_binding(const MIRInstruction *inst,
                                         uint32_t binding_syntax_id)
{
    const char *name = mir_collection_call_name(inst);
    ASTNode *receiver;

    if (name == NULL
        || (strcmp(name, "ArrayPushOwnedString") != 0
            && strcmp(name, "ArraySetOwnedString") != 0
            && strcmp(name, "ArrayPush") != 0
            && strcmp(name, "ArraySet") != 0
            && strcmp(name, "ArrayPop") != 0
            && strcmp(name, "ArrayDropOwnedStrings") != 0)) {
        return false;
    }
    receiver = ast_call_argument(inst->expr0, 0);
    return receiver != NULL && receiver->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(receiver) == binding_syntax_id;
}

static bool
mir_collection_binding_requires_transition_validation(
    const MIRRoutine *routine,
    uint32_t binding_syntax_id)
{
    if (routine == NULL || binding_syntax_id == 0)
        return false;
    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_row = 0;
             inst_row < block->instruction_count;
             inst_row++) {
            const MIRInstruction *inst = &block->instructions[inst_row];
            if (mir_collection_relevant_call_for_binding(
                    inst, binding_syntax_id))
                return true;
            if (inst->has_collection_ownership_receipt
                && inst->collection_ownership_receiver_binding_id
                    == binding_syntax_id
                && (inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_DROP
                    || inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                    || inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
                    || inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_OWNED_STRING_SET
                    || inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_OWNED_STRING_POP)) {
                return true;
            }
        }
    }
    return false;
}

static bool
mir_collection_empty_birth_ready(const MIRRoutine *routine,
                                 const MIRInstruction *inst,
                                 const MIRCollectionOwnershipFact *fact)
{
    const MIRSourceLocalType *local =
        mir_routine_source_local_type_fact_by_binding_syntax_id(
            routine, fact->binding_syntax_id);
    return local != NULL && local->type_name != NULL
        && strcmp(local->type_name, "Array<String>") == 0
        && inst->kind == MIR_INST_DEF
        && inst->requires_source_local_decl_emit
        && inst->binding_syntax_id == fact->binding_syntax_id
        && inst->source_stable_id == fact->binding_syntax_id
        && inst->ast != NULL && inst->ast->type == AST_LET_DECL
        && ast_node_stable_id(inst->ast) == fact->binding_syntax_id
        && inst->expr0 == ast_let_initializer(inst->ast)
        && inst->expr1 != NULL && inst->expr1 == ast_let_type(inst->ast)
        && inst->expr0 != NULL && inst->expr0->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(inst->expr0) == 0
        && ast_node_stable_id(inst->expr0) == fact->origin_syntax_id;
}

static bool
mir_collection_transition_apply(const MIRInstruction *inst,
                                MIRCollectionTransitionState *state)
{
    if (inst == NULL || state == NULL
        || *state == MIR_COLLECTION_STATE_BOTTOM
        || !inst->has_collection_ownership_receipt)
        return false;
    switch ((PgyCollectionOwnershipEffectKind)
                inst->collection_ownership_effect_kind) {
    case PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH:
        if ((*state & ~(MIR_COLLECTION_STATE_EMPTY
                        | MIR_COLLECTION_STATE_OWNED)) != 0)
            return false;
        *state = MIR_COLLECTION_STATE_OWNED;
        return true;
    case PGY_COLLECTION_EFFECT_SHALLOW_MUTATION:
        if ((*state & ~(MIR_COLLECTION_STATE_EMPTY
                        | MIR_COLLECTION_STATE_BORROWED)) != 0)
            return false;
        *state = MIR_COLLECTION_STATE_BORROWED;
        return true;
    case PGY_COLLECTION_EFFECT_DROP:
        if ((*state & ~(MIR_COLLECTION_STATE_EMPTY
                        | MIR_COLLECTION_STATE_OWNED)) != 0)
            return false;
        *state = MIR_COLLECTION_STATE_RETIRED;
        return true;
    case PGY_COLLECTION_EFFECT_OWNED_STRING_SET:
        /* ArraySetOwnedString checks the index before it clones or retires an
         * element.  Therefore an EMPTY alternative cannot return from this
         * instruction; on every continuing path the replaced element was
         * already owned.  This also lets a loop-carried EMPTY|OWNED join
         * refine to OWNED without treating a borrowed or retired path as an
         * ownership grant. */
        if ((*state & ~(MIR_COLLECTION_STATE_EMPTY
                        | MIR_COLLECTION_STATE_OWNED)) != 0)
            return false;
        *state = MIR_COLLECTION_STATE_OWNED;
        return true;
    case PGY_COLLECTION_EFFECT_OWNED_STRING_POP:
        if (*state != MIR_COLLECTION_STATE_OWNED)
            return false;
        return true;
    default:
        return false;
    }
}

static bool
mir_collection_transition_error(const MIRRoutine *routine,
                                char **error_message,
                                uint32_t binding_syntax_id,
                                const char *stage)
{
    if (error_message != NULL) {
        const MIRSourceLocalType *local = mir_routine_source_local_type_fact_by_binding_syntax_id(
            routine, binding_syntax_id);
        char detail[512];
        snprintf(detail, sizeof(detail),
            "MIR collection ownership transition is invalid "
            "(routine=%s binding=%u local=%s stage=%s)",
            routine != NULL && routine->name != NULL
                ? routine->name : "<unknown>",
            binding_syntax_id,
            local != NULL && local->name != NULL
                ? local->name : "<unknown>",
            stage != NULL ? stage : "unknown");
        *error_message = pergyra_strdup(detail);
    }
    return false;
}

static bool
mir_collection_transition_apply_error(const MIRRoutine *routine,
                                      char **error_message,
                                      uint32_t binding_syntax_id,
                                      size_t block_id,
                                      size_t instruction_id,
                                      const MIRInstruction *inst,
                                      MIRCollectionTransitionState state)
{
    if (error_message != NULL) {
        const MIRSourceLocalType *local =
            mir_routine_source_local_type_fact_by_binding_syntax_id(
                routine, binding_syntax_id);
        const char *call = mir_collection_call_name(inst);
        const ASTNode *site = inst != NULL && inst->expr0 != NULL
            ? inst->expr0 : (inst != NULL ? inst->ast : NULL);
        char detail[768];
        snprintf(detail, sizeof(detail),
            "MIR collection ownership transition is invalid "
            "(routine=%s binding=%u local=%s stage=invalid-state-transition"
            " block=%zu instruction=%zu state=%u effect=%u call=%s"
            " source=%u receiver=%u effect_source=%u line=%u column=%u"
            " path=%s)",
            routine != NULL && routine->name != NULL
                ? routine->name : "<unknown>",
            binding_syntax_id,
            local != NULL && local->name != NULL
                ? local->name : "<unknown>",
            block_id, instruction_id, (unsigned)state,
            inst != NULL
                ? inst->collection_ownership_effect_kind : 0,
            call != NULL ? call : "<unknown>",
            inst != NULL ? inst->source_stable_id : 0,
            inst != NULL
                ? inst->collection_ownership_receiver_binding_id : 0,
            inst != NULL
                ? inst->collection_ownership_source_binding_id : 0,
            site != NULL ? site->line : 0,
            site != NULL ? site->column : 0,
            site != NULL && site->origin_path != NULL
                ? site->origin_path : "<unknown>");
        *error_message = pergyra_strdup(detail);
    }
    return false;
}

static bool
mir_collection_empty_birth_error(const MIRRoutine *routine,
                                 char **error_message,
                                 uint32_t binding_syntax_id,
                                 size_t block_id,
                                 size_t instruction_id,
                                 const MIRInstruction *inst,
                                 const MIRCollectionOwnershipFact *fact,
                                 MIRCollectionTransitionState state)
{
    if (error_message != NULL) {
        const MIRSourceLocalType *local =
            mir_routine_source_local_type_fact_by_binding_syntax_id(
                routine, binding_syntax_id);
        const ASTNode *declaration = inst != NULL ? inst->ast : NULL;
        const ASTNode *initializer = inst != NULL ? inst->expr0 : NULL;
        const ASTNode *decl_initializer = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_initializer(declaration) : NULL;
        const ASTNode *decl_type = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_type(declaration) : NULL;
        const ASTNode *site = initializer != NULL
            ? initializer : declaration;
        char detail[1024];
        snprintf(detail, sizeof(detail),
            "MIR collection ownership transition is invalid "
            "(routine=%s binding=%u local=%s stage=invalid-empty-birth"
            " block=%zu instruction=%zu state=%u inst_kind=%u decl_emit=%u"
            " inst_binding=%u source=%u fact_origin=%u fact_source=%u"
            " local_type=%s ast_kind=%u ast_id=%u initializer_same=%u"
            " type_same=%u initializer_kind=%u initializer_count=%zu"
            " initializer_id=%u line=%u column=%u path=%s)",
            routine != NULL && routine->name != NULL
                ? routine->name : "<unknown>",
            binding_syntax_id,
            local != NULL && local->name != NULL
                ? local->name : "<unknown>",
            block_id, instruction_id, (unsigned)state,
            inst != NULL ? (unsigned)inst->kind : 0,
            inst != NULL && inst->requires_source_local_decl_emit ? 1u : 0u,
            inst != NULL ? inst->binding_syntax_id : 0,
            inst != NULL ? inst->source_stable_id : 0,
            fact != NULL ? (unsigned)fact->origin : 0,
            fact != NULL ? fact->origin_syntax_id : 0,
            local != NULL && local->type_name != NULL
                ? local->type_name : "<unknown>",
            declaration != NULL ? (unsigned)declaration->type : 0,
            declaration != NULL ? ast_node_stable_id(declaration) : 0,
            initializer != NULL && initializer == decl_initializer ? 1u : 0u,
            inst != NULL && inst->expr1 != NULL
                && inst->expr1 == decl_type ? 1u : 0u,
            initializer != NULL ? (unsigned)initializer->type : 0,
            initializer != NULL && initializer->type == AST_ARRAY_LITERAL
                ? ast_array_literal_count(initializer) : 0,
            initializer != NULL ? ast_node_stable_id(initializer) : 0,
            site != NULL ? site->line : 0,
            site != NULL ? site->column : 0,
            site != NULL && site->origin_path != NULL
                ? site->origin_path : "<unknown>");
        *error_message = pergyra_strdup(detail);
    }
    return false;
}

static bool
mir_collection_owned_birth_ready(const MIRRoutine *routine, const MIRInstruction *inst,
                                const MIRCollectionOwnershipFact *fact)
{
    ASTNode *call = inst != NULL ? inst->expr0 : NULL;
    const MIRSourceLocalType *target = fact != NULL
        ? mir_routine_source_local_type_fact_by_binding_syntax_id(routine, fact->binding_syntax_id) : NULL;
    if (inst == NULL || fact == NULL || inst->kind != MIR_INST_DEF
        || inst->source_stable_id != fact->binding_syntax_id
        || inst->binding_syntax_id != fact->binding_syntax_id
        || call == NULL || target == NULL)
        return false;
    if (fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL) {
        return call->type == AST_ARRAY_LITERAL
            && ast_array_literal_count(call) != 0
            && ast_node_stable_id(call) == fact->origin_syntax_id
            && target->type_name != NULL
            && strcmp(target->type_name, "Array<String>") == 0
            && target->initializer_collection_producer == 0
            && target->initializer_call_syntax_id == 0
            && target->initializer_call_value_type_name == NULL;
    }
    if (fact->origin == PGY_COLLECTION_ORIGIN_CALL_RESULT) {
        if (call->type != AST_CALL
            || !ast_call_semantic_callee_declared_callable(call)
            || ast_call_semantic_callee_decl_id(call)
                != fact->origin_syntax_id
            || ast_call_semantic_callee_value_binding_id(call) != 0
            || target->initializer_collection_producer != 3
            || target->initializer_call_syntax_id
                != ast_node_stable_id(call)
            || target->initializer_call_value_type_name == NULL
            || strcmp(target->initializer_call_value_type_name,
                "Array<String>") != 0)
            return false;
        MIRJsonExpressionGraph graph = {0};
        int root = mir_json_expression_graph_build_for_routine(
            &graph, call, routine);
        int call_row = root;
        while (call_row >= 0 && (size_t)call_row < graph.count
            && strcmp(graph.nodes[call_row].kind, "call_argument") == 0)
            call_row = graph.nodes[call_row].left;
        bool ready = call_row >= 0 && (size_t)call_row < graph.count
            && strcmp(graph.nodes[call_row].kind, "call") == 0
            && strcmp(graph.nodes[call_row].call_target_kind, "direct") == 0
            && graph.nodes[call_row].call_target_syntax_id
                == fact->origin_syntax_id;
        mir_json_expression_graph_dispose(&graph);
        return ready;
    }
    if (target->initializer_call_syntax_id != fact->origin_syntax_id
        || ast_node_stable_id(call) != target->initializer_call_syntax_id
        || target->initializer_call_value_type_name == NULL
        || strcmp(target->initializer_call_value_type_name, "Array<String>") != 0)
        return false;
    uint32_t producer = fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS ? 1 : 2;
    if (target->initializer_collection_producer != producer) return false;
    MIRJsonExpressionGraph graph = {0};
    int root = mir_json_expression_graph_build_for_routine(&graph, call, routine);
    bool ready = false;
    if (root >= 0 && (size_t)root < graph.count
        && strcmp(graph.nodes[root].kind, "call_argument") == 0) {
        int call_row = graph.nodes[root].left;
        if (call_row >= 0 && (size_t)call_row < graph.count) {
            const MIRJsonExpressionGraphNode *node = &graph.nodes[call_row];
            int callee_row = node->left;
            while (callee_row >= 0 && (size_t)callee_row < graph.count
                && strcmp(graph.nodes[callee_row].kind, "generic_callee") == 0)
                callee_row = graph.nodes[callee_row].left;
            ready = strcmp(node->kind, "call") == 0
                && strcmp(node->call_target_kind, "direct") == 0
                && node->call_target_syntax_id == 0
                && strcmp(node->call_target_name, producer == 1 ? "MapKeys" : "Clone") == 0
                && callee_row >= 0 && (size_t)callee_row < graph.count
                && graph.nodes[callee_row].binding_syntax_id == 0
                && strcmp(graph.nodes[callee_row].binding_kind, "none") == 0;
        }
    }
    mir_json_expression_graph_dispose(&graph);
    return ready;
}

/* The existing wire has one receipt per statement. Walk only children that
 * the expression evaluates now: a lambda body, for example, belongs to its
 * later invocation. Collection-effect facts are the call-identity owner, so
 * expression-graph serialization coverage is not a safety prerequisite. */
static bool
mir_collection_owned_evaluated_calls_ready(const MIRRoutine *routine,
                                            ASTNode *expression,
                                            bool root_call)
{
    if (expression == NULL)
        return true;
    switch (expression->type) {
    case AST_CALL: {
        ASTNode *callee = ast_call_callee(expression);
        uint32_t effect = 0, receiver = 0, source = 0;
        bool has_effect = ast_call_semantic_collection_effect(
            expression, &effect, &receiver, &source);
        bool named_owned_drop = callee != NULL
            && callee->type == AST_IDENTIFIER
            && ast_identifier_name(callee) != NULL
            && strcmp(ast_identifier_name(callee),
                "ArrayDropOwnedStrings") == 0
            && !ast_call_semantic_callee_declared_callable(expression)
            && ast_call_semantic_callee_value_binding_id(expression) == 0;
        if (named_owned_drop
            && (!has_effect || effect != PGY_COLLECTION_EFFECT_DROP
                || receiver == 0 || source != 0))
            return false;
        if (has_effect && effect == PGY_COLLECTION_EFFECT_DROP) {
            const MIRCollectionOwnershipFact *fact =
                mir_routine_collection_ownership_fact(routine, receiver);
            if (receiver == 0 || source != 0
                || (pgy_collection_owned_local_fact_tracked(fact)
                    && !root_call))
                return false;
        }
        if (!mir_collection_owned_evaluated_calls_ready(
                routine, callee, false))
            return false;
        for (size_t i = 0; i < ast_call_arg_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_call_argument(expression, i), false))
                return false;
        return true;
    }
    case AST_BINARY:
        return mir_collection_owned_evaluated_calls_ready(
                routine, ast_binary_left(expression), false)
            && mir_collection_owned_evaluated_calls_ready(
                routine, ast_binary_right(expression), false);
    case AST_UNARY:
        return mir_collection_owned_evaluated_calls_ready(
            routine, ast_unary_operand(expression), false);
    case AST_MEMBER_ACCESS:
        return mir_collection_owned_evaluated_calls_ready(
            routine, ast_member_object(expression), false);
    case AST_ARRAY_ACCESS:
        return mir_collection_owned_evaluated_calls_ready(
                routine, ast_array_access_array(expression), false)
            && mir_collection_owned_evaluated_calls_ready(
                routine, ast_array_access_index(expression), false);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_array_literal_element(expression, i), false))
                return false;
        return true;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_tuple_literal_element(expression, i), false))
                return false;
        return true;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_map_literal_key(expression, i), false)
                || !mir_collection_owned_evaluated_calls_ready(
                    routine, ast_map_literal_value(expression, i), false))
                return false;
        return true;
    case AST_SET_LITERAL:
        for (size_t i = 0; i < ast_set_literal_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_set_literal_element(expression, i), false))
                return false;
        return true;
    case AST_CAST:
        return mir_collection_owned_evaluated_calls_ready(
            routine, ast_cast_operand(expression), false);
    case AST_TYPE_TEST:
        return mir_collection_owned_evaluated_calls_ready(
            routine, ast_type_test_operand(expression), false);
    case AST_ASSIGNMENT:
        return mir_collection_owned_evaluated_calls_ready(
                routine, ast_assignment_target(expression), false)
            && mir_collection_owned_evaluated_calls_ready(
                routine, ast_assignment_value(expression), false);
    case AST_AWAIT_EXPR:
        return mir_collection_owned_evaluated_calls_ready(
            routine, ast_await_expression(expression), false);
    case AST_CHANNEL_SEND:
        return mir_collection_owned_evaluated_calls_ready(
                routine, ast_channel_send_channel(expression), false)
            && mir_collection_owned_evaluated_calls_ready(
                routine, ast_channel_send_value(expression), false);
    case AST_CHANNEL_RECV:
        return mir_collection_owned_evaluated_calls_ready(
            routine, ast_channel_recv_channel(expression), false);
    case AST_SPAWN_EXPR:
        if (!mir_collection_owned_evaluated_calls_ready(
                routine, ast_spawn_function(expression), false))
            return false;
        for (size_t i = 0; i < ast_spawn_arg_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_spawn_argument(expression, i), false))
                return false;
        return true;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return mir_collection_owned_evaluated_calls_ready(
                routine, ast_event_op_event(expression), false)
            && mir_collection_owned_evaluated_calls_ready(
                routine, ast_event_op_handler(expression), false);
    case AST_EVENT_INVOKE:
        if (!mir_collection_owned_evaluated_calls_ready(
                routine, ast_event_invoke_event(expression), false))
            return false;
        for (size_t i = 0; i < ast_event_invoke_arg_count(expression); i++)
            if (!mir_collection_owned_evaluated_calls_ready(
                    routine, ast_event_invoke_argument(expression, i), false))
                return false;
        return true;
    case AST_LAMBDA_EXPR:
        return true; /* Constructing a closure does not execute its body. */
    case AST_IDENTIFIER:
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
        return true;
    default:
        return false;
    }
}

static bool
mir_collection_owned_nested_calls_ready(const MIRRoutine *routine,
                                         ASTNode *expression)
{
    return mir_collection_owned_evaluated_calls_ready(
        routine, expression, true);
}

static bool
mir_collection_evaluated_expression_roots(const MIRRoutine *routine,
                                          const MIRInstruction *inst,
                                          ASTNode *roots[2])
{
    if (!mir_instruction_evaluation_roots(routine, inst, roots))
        return false;
    if (!mir_instruction_source_is_defer_stmt(inst))
        return true;

    roots[0] = NULL;
    roots[1] = NULL;
    if (inst->arg0 == NULL)
        return false;
    if (strcmp(inst->arg0, "Assign") == 0) {
        roots[0] = mir_defer_assignment_value_fact(inst);
        roots[1] = mir_defer_assignment_target_fact(inst);
        return roots[0] != NULL && roots[1] != NULL;
    }
    if (strcmp(inst->arg0, "Log") == 0) {
        roots[0] = mir_defer_log_expression_fact(inst);
        return roots[0] != NULL;
    }
    if (strcmp(inst->arg0, "Call") == 0) {
        roots[0] = mir_defer_call_expression_fact(inst);
        return roots[0] != NULL;
    }
    return false;
}

static bool
mir_collection_terminal_return_block(const MIRBasicBlock *block)
{
    bool normal_return = false;
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++)
        normal_return |= block->instructions[i].kind == MIR_INST_RETURN;
    return !block->has_succ_true && !block->has_succ_false
        && !block->has_cleanup_succ && !block->has_rollback_succ
        && !block->has_invalidation_succ && normal_return;
}

static bool
mir_collection_retirement_append(
    PgyCollectionTransitionRetirement **rows, size_t *count,
    size_t *capacity, PgyCollectionTransitionRetirement row)
{
    if (*count == *capacity) {
        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        if (next < *capacity || next > SIZE_MAX / sizeof(**rows))
            return false;
        void *grown = realloc(*rows, next * sizeof(**rows));
        if (grown == NULL)
            return false;
        *rows = grown;
        *capacity = next;
    }
    (*rows)[(*count)++] = row;
    return true;
}

static bool
mir_collection_deferred_drop_binding(const MIRRoutine *routine,
                                     const MIRInstruction *part_inst,
                                     uint32_t *binding_out)
{
    ASTNode *call = mir_defer_call_expression_fact(part_inst);
    uint32_t kind = 0, receipt_binding = 0, source = 0;
    *binding_out = 0;
    if (call == NULL)
        return true;
    ASTNode *callee = ast_call_callee(call);
    ASTNode *receiver = ast_call_arg_count(call) > 0
        ? ast_call_argument(call, 0) : NULL;
    uint32_t receiver_binding = receiver != NULL
            && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    bool release_call = callee != NULL && callee->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(callee) == 0
        && ast_call_semantic_callee_is_stdlib(call)
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && !ast_call_semantic_callee_declared_callable(call)
        && ast_call_arg_count(call) == 1
        && strcmp(ast_identifier_name(callee),
            "ArrayDropOwnedStrings") == 0;
    bool receipt = ast_call_semantic_collection_effect(
        call, &kind, &receipt_binding, &source);
    const MIRCollectionOwnershipFact *receiver_fact =
        mir_routine_collection_ownership_fact(routine, receiver_binding);
    const MIRCollectionOwnershipFact *receipt_fact = receipt
        ? mir_routine_collection_ownership_fact(routine, receipt_binding)
        : NULL;
    if (!release_call
        && !pgy_collection_owned_local_fact_tracked(receipt_fact))
        return true;
    if (!pgy_collection_owned_local_fact_tracked(receiver_fact)
        || receipt_fact != receiver_fact || !release_call || !receipt
        || kind != PGY_COLLECTION_EFFECT_DROP
        || receipt_binding != receiver_binding || source != 0
        || receiver_fact->disposition
            != PGY_COLLECTION_DISPOSITION_RETIRED
        || ast_node_stable_id(call) == 0)
        return false;
    *binding_out = receiver_binding;
    return true;
}

static bool
mir_collection_append_deferred_retirements(
    const MIRRoutine *routine, size_t block_id, size_t instruction_index,
    PgyCollectionTransitionRetirement **rows, size_t *count,
    size_t *capacity, uint32_t *failure_binding, const char **failure_stage)
{
    const MIRBasicBlock *block = &routine->blocks[block_id];
    const MIRInstruction *registration =
        &block->instructions[instruction_index];
    if (!mir_instruction_source_is_defer_stmt(registration)
        || registration->defer_part_index != 0)
        return true;
    size_t parts = registration->defer_part_count;
    if (parts == 0 || parts > block->instruction_count - instruction_index
        || registration->kind != MIR_INST_STMT
        || registration->ast == NULL
        || registration->ast->type != AST_DEFER_STMT
        || registration->expr0 != ast_defer_body(registration->ast)
        || registration->expr0 == NULL
        || registration->expr0->type != AST_BLOCK
        || ast_block_statement_count(registration->expr0) != parts) {
        *failure_stage = "deferred-release-registration";
        return false;
    }
    for (size_t part_index = 0; part_index < parts; part_index++) {
        const MIRInstruction *part_inst =
            &block->instructions[instruction_index + part_index];
        if (!mir_instruction_source_is_defer_stmt(part_inst)
            || part_inst->kind != MIR_INST_STMT
            || part_inst->defer_part_index != part_index
            || part_inst->defer_part_count != parts
            || part_inst->ast != registration->ast
            || part_inst->expr0 != registration->expr0
            || part_inst->source_stable_id != registration->source_stable_id) {
            *failure_stage = "deferred-release-parts";
            return false;
        }
        uint32_t binding = 0;
        if (!mir_collection_deferred_drop_binding(
                routine, part_inst, &binding)) {
            *failure_stage = "deferred-release-receipt";
            return false;
        }
        if (binding == 0)
            continue;
        *failure_binding = binding;
        for (size_t exit = 0; exit < routine->block_count; exit++) {
            const MIRBasicBlock *exit_block = &routine->blocks[exit];
            if (!exit_block->is_reachable || exit_block->is_cleanup
                || !mir_collection_terminal_return_block(exit_block))
                continue;
            int relation = mir_defer_registration_exit_relation(
                routine, registration, exit_block);
            if (relation == 2)
                continue;
            if (relation != 1) {
                *failure_stage = "deferred-release-topology";
                return false;
            }
            if (!mir_collection_retirement_append(rows, count, capacity,
                    (PgyCollectionTransitionRetirement){
                        exit, SIZE_MAX, binding})) {
                *failure_stage = "deferred-release-retirement-allocation";
                return false;
            }
        }
    }
    return true;
}

static bool
mir_collection_validate_owned_local_transitions(
    const MIRRoutine *routine, char **error_message)
{
    size_t binding_count = 0, instruction_count = 0, predecessor_count = 0;
    PgyCollectionOwnershipFact *bindings = NULL;
    PgyCollectionTransitionBlock *blocks = NULL;
    PgyCollectionTransitionOperation *operations = NULL;
    PgyCollectionTransitionRetirement *retirements = NULL;
    size_t retirement_capacity = 0;
    size_t *predecessors = NULL;
    bool valid = false;
    const char *failure_stage = "owned-operation-admission";
    char failure_stage_detail[512] = {0};
    uint32_t failure_binding = 0;
    for (size_t row = 0; row < routine->collection_ownership_fact_count; row++)
        if (pgy_collection_owned_local_fact_tracked(&routine->collection_ownership_facts[row]))
            binding_count++;
    if (binding_count == 0) return true;
    if (routine->block_count == 0 || routine->blocks == NULL
        || routine->entry_block >= routine->block_count)
        return mir_collection_transition_error(routine, error_message, 0, "missing-owned-cfg");
    for (size_t b = 0; b < routine->block_count; b++) {
        if (routine->blocks[b].instruction_count > SIZE_MAX - instruction_count
            || routine->blocks[b].predecessor_count > SIZE_MAX - predecessor_count)
            goto done;
        instruction_count += routine->blocks[b].instruction_count;
        predecessor_count += routine->blocks[b].predecessor_count;
    }
    bindings = calloc(binding_count, sizeof(*bindings));
    blocks = calloc(routine->block_count, sizeof(*blocks));
    operations = calloc(instruction_count == 0 ? 1 : instruction_count, sizeof(*operations));
    predecessors = calloc(predecessor_count == 0 ? 1 : predecessor_count, sizeof(*predecessors));
    if (bindings == NULL || blocks == NULL || operations == NULL
        || predecessors == NULL)
        goto done;
    size_t next_binding = 0, next_operation = 0, next_predecessor = 0,
        next_retirement = 0;
    for (size_t row = 0; row < routine->collection_ownership_fact_count; row++)
        if (pgy_collection_owned_local_fact_tracked(&routine->collection_ownership_facts[row]))
            bindings[next_binding++] = routine->collection_ownership_facts[row];
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        blocks[b].reachable = block->is_reachable;
        blocks[b].terminal = mir_collection_terminal_return_block(block);
        blocks[b].predecessor_start = next_predecessor;
        blocks[b].predecessor_count = block->predecessor_count;
        if ((block->instruction_count != 0 && block->instructions == NULL)
            || (block->predecessor_count != 0 && block->predecessors == NULL))
            goto done;
        for (size_t p = 0; p < block->predecessor_count; p++)
            predecessors[next_predecessor++] = block->predecessors[p];
        blocks[b].operation_start = next_operation;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (!mir_collection_append_deferred_retirements(
                    routine, b, i, &retirements, &next_retirement,
                    &retirement_capacity, &failure_binding,
                    &failure_stage))
                goto done;
            if (inst->kind == MIR_INST_CLEANUP_EDGE &&
                inst->collection_cleanup_kind != 0) {
                /* Scalar backing has no String-element transition state.
                 * The shared cleanup validator has already checked its
                 * origin, private views, exact exits, and carried ABI. */
                if (inst->collection_cleanup_kind == 3)
                    continue;
                const MIRCollectionOwnershipFact *cleanup_fact =
                    mir_routine_collection_ownership_fact(routine,
                        inst->binding_syntax_id);
                failure_binding = inst->binding_syntax_id;
                failure_stage = "owned-edge-retirement-fact";
                if (cleanup_fact == NULL)
                    goto done;
                /* Empty locals can acquire owned elements through MIR effects
                 * even though their birth fact is not a transition-model
                 * binding. The loop-cleanup owner already validates those
                 * edges; only retirements for bindings represented in this
                 * transition view belong here. */
                if (!pgy_collection_owned_local_fact_tracked(cleanup_fact))
                    continue;
                /* mir_validate_loop_collection_cleanup already owns the
                 * exact shallow/deep choice after applying every MIR effect
                 * receipt. This consumer accepts only its sealed kind instead
                 * of re-deriving it from the birth fact. */
                failure_stage = "owned-edge-retirement-kind";
                if (inst->collection_cleanup_kind != 1
                    && inst->collection_cleanup_kind != 2)
                    goto done;
                failure_stage = "owned-edge-retirement-target";
                if (inst->collection_cleanup_target != SIZE_MAX &&
                    inst->collection_cleanup_target >= routine->block_count)
                    goto done;
                if (!mir_collection_retirement_append(&retirements,
                        &next_retirement, &retirement_capacity,
                        (PgyCollectionTransitionRetirement){
                            b, inst->collection_cleanup_target,
                            inst->binding_syntax_id})) {
                    failure_stage = "owned-edge-retirement-allocation";
                    goto done;
                }
                continue;
            }
            ASTNode *evaluated_roots[2] = {NULL, NULL};
            failure_stage = "owned-evaluated-expression";
            failure_binding = inst->binding_syntax_id;
            bool roots_ready = mir_collection_evaluated_expression_roots(
                routine, inst, evaluated_roots);
            bool root0_ready = roots_ready
                && mir_collection_owned_nested_calls_ready(
                    routine, evaluated_roots[0]);
            bool root1_ready = roots_ready && root0_ready
                && mir_collection_owned_nested_calls_ready(
                    routine, evaluated_roots[1]);
            if (!roots_ready || !root0_ready || !root1_ready) {
                ASTNode *root0_callee = evaluated_roots[0] != NULL
                        && evaluated_roots[0]->type == AST_CALL
                    ? ast_call_callee(evaluated_roots[0]) : NULL;
                const char *root0_call = root0_callee != NULL
                        && root0_callee->type == AST_IDENTIFIER
                    ? ast_identifier_name(root0_callee) : NULL;
                uint32_t root0_effect = 0, root0_receiver = 0,
                    root0_source = 0;
                bool root0_has_effect = evaluated_roots[0] != NULL
                        && evaluated_roots[0]->type == AST_CALL
                    && ast_call_semantic_collection_effect(
                        evaluated_roots[0], &root0_effect,
                        &root0_receiver, &root0_source);
                snprintf(failure_stage_detail,
                    sizeof(failure_stage_detail),
                    "owned-evaluated-expression:block=%zu:instruction=%zu:"
                    "kind=%u:root0=%u:root1=%u:roots=%u:ready0=%u:ready1=%u:"
                    "call=%s:has-effect=%u:effect=%u:receiver=%u:source=%u",
                    b, i, (unsigned)inst->kind,
                    evaluated_roots[0] != NULL
                        ? (unsigned)evaluated_roots[0]->type : 0u,
                    evaluated_roots[1] != NULL
                        ? (unsigned)evaluated_roots[1]->type : 0u,
                    roots_ready ? 1u : 0u,
                    root0_ready ? 1u : 0u,
                    root1_ready ? 1u : 0u,
                    root0_call != NULL ? root0_call : "<none>",
                    root0_has_effect ? 1u : 0u,
                    root0_effect, root0_receiver, root0_source);
                failure_stage = failure_stage_detail;
                goto done;
            }
            const MIRCollectionOwnershipFact *fact =
                mir_routine_collection_ownership_fact(routine, inst->binding_syntax_id);
            PgyCollectionTransitionOperation op = {0};
            if (pgy_collection_owned_local_fact_tracked(fact)) {
                failure_stage = "owned-definition-identity";
                const char *type = mir_collection_binding_type_name(routine, fact->binding_syntax_id);
                if (inst->kind != MIR_INST_DEF || inst->source_stable_id != fact->binding_syntax_id
                    || type == NULL || strcmp(type, "Array<String>") != 0)
                    goto done;
                op.binding_syntax_id = fact->binding_syntax_id;
                op.syntax_id = fact->binding_syntax_id;
                if (fact->origin == PGY_COLLECTION_ORIGIN_BINDING) {
                    failure_stage = "owned-move-source";
                    if (!mir_collection_binding_move_source_value_matches(
                            routine, inst, fact->source_binding_syntax_id))
                        goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_MOVE;
                    op.source_binding_syntax_id = fact->source_binding_syntax_id;
                } else {
                    failure_stage = "owned-canonical-birth";
                    if (!mir_collection_owned_birth_ready(routine, inst, fact)) goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_BIRTH;
                }
            } else {
                const char *name = mir_collection_call_name(inst);
                ASTNode *receiver = name != NULL ? ast_call_argument(inst->expr0, 0) : NULL;
                uint32_t receiver_id = receiver != NULL && receiver->type == AST_IDENTIFIER
                    ? ast_identifier_binding_syntax_id(receiver) : 0;
                fact = mir_routine_collection_ownership_fact(routine, receiver_id);
                if (pgy_collection_owned_local_fact_tracked(fact)
                    && name != NULL && strcmp(name, "ArrayDropOwnedStrings") == 0) {
                    failure_stage = "owned-drop-receipt";
                    failure_binding = receiver_id;
                    uint32_t effect = 0, issued_receiver = 0, issued_source = 0;
                    if (inst->kind != MIR_INST_STMT || ast_call_arg_count(inst->expr0) != 1
                        || ast_call_semantic_callee_decl_id(inst->expr0) != 0
                        || ast_call_semantic_callee_value_binding_id(inst->expr0) != 0
                        || !ast_call_semantic_collection_effect(inst->expr0,
                            &effect, &issued_receiver, &issued_source)
                        || effect != PGY_COLLECTION_EFFECT_DROP
                        || issued_receiver != receiver_id || issued_source != 0
                        || !mir_collection_receipt_shape_ready(inst, receiver_id)
                        || inst->collection_ownership_effect_kind != PGY_COLLECTION_EFFECT_DROP)
                        goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_DROP;
                    op.binding_syntax_id = receiver_id;
                    op.syntax_id = ast_node_stable_id(inst->expr0);
                } else if (pgy_collection_owned_local_fact_tracked(fact)
                    && name != NULL
                    && (strcmp(name, "ArrayPushOwnedString") == 0
                        || strcmp(name, "ArrayPush") == 0)
                    && inst->has_collection_ownership_receipt) {
                    failure_stage = "owned-string-push-receipt";
                    failure_binding = receiver_id;
                    uint32_t effect = 0, issued_receiver = 0,
                        issued_source = 0;
                    if (inst->kind != MIR_INST_STMT
                        || ast_call_arg_count(inst->expr0) != 2
                        || ast_call_semantic_callee_decl_id(inst->expr0) != 0
                        || ast_call_semantic_callee_value_binding_id(
                            inst->expr0) != 0
                        || !ast_call_semantic_collection_effect(inst->expr0,
                            &effect, &issued_receiver, &issued_source)
                        || effect != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                        || issued_receiver != receiver_id
                        || inst->collection_ownership_source_binding_id
                            != issued_source
                        || (strcmp(name, "ArrayPushOwnedString") == 0
                            ? issued_source != 0 : issued_source == 0)
                        || !mir_collection_receipt_shape_ready(
                            inst, receiver_id)
                        || inst->collection_ownership_effect_kind !=
                            PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH)
                        goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_OWNED_STRING_PUSH;
                    op.binding_syntax_id = receiver_id;
                    op.syntax_id = ast_node_stable_id(inst->expr0);
                } else if (pgy_collection_owned_local_fact_tracked(fact)
                    && name != NULL
                    && strcmp(name, "ArraySetOwnedString") == 0) {
                    failure_stage = "owned-string-set-receipt";
                    failure_binding = receiver_id;
                    uint32_t effect = 0, issued_receiver = 0,
                        issued_source = 0;
                    if (inst->kind != MIR_INST_STMT
                        || ast_call_arg_count(inst->expr0) != 3) {
                        failure_stage = "owned-string-set-statement";
                        goto done;
                    }
                    if (ast_call_semantic_callee_decl_id(inst->expr0) != 0
                        || ast_call_semantic_callee_value_binding_id(
                            inst->expr0) != 0) {
                        failure_stage = "owned-string-set-target";
                        goto done;
                    }
                    if (!ast_call_semantic_collection_effect(inst->expr0,
                            &effect, &issued_receiver, &issued_source)) {
                        failure_stage = "owned-string-set-missing-semantic-receipt";
                        goto done;
                    }
                    if (effect != PGY_COLLECTION_EFFECT_OWNED_STRING_SET) {
                        failure_stage = "owned-string-set-semantic-effect";
                        goto done;
                    }
                    if (issued_receiver != receiver_id) {
                        failure_stage = "owned-string-set-semantic-receiver";
                        goto done;
                    }
                    if (issued_source != 0) {
                        failure_stage = "owned-string-set-semantic-source";
                        goto done;
                    }
                    if (!mir_collection_receipt_shape_ready(inst, receiver_id)) {
                        failure_stage = "owned-string-set-shape";
                        goto done;
                    }
                    if (inst->collection_ownership_effect_kind !=
                            PGY_COLLECTION_EFFECT_OWNED_STRING_SET) {
                        failure_stage = "owned-string-set-mir-receipt";
                        goto done;
                    }
                    op.kind = PGY_COLLECTION_TRANSITION_OWNED_STRING_SET;
                    op.binding_syntax_id = receiver_id;
                    op.syntax_id = ast_node_stable_id(inst->expr0);
                } else if (pgy_collection_owned_local_fact_tracked(fact)
                    && name != NULL && strcmp(name, "ArrayPop") == 0
                    && inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_OWNED_STRING_POP) {
                    failure_stage = "owned-string-pop-receipt";
                    failure_binding = receiver_id;
                    uint32_t effect = 0, issued_receiver = 0,
                        issued_source = 0;
                    if (inst->kind != MIR_INST_STMT
                        || ast_call_arg_count(inst->expr0) != 1
                        || ast_call_semantic_callee_decl_id(inst->expr0) != 0
                        || ast_call_semantic_callee_value_binding_id(
                            inst->expr0) != 0
                        || !ast_call_semantic_collection_effect(inst->expr0,
                            &effect, &issued_receiver, &issued_source)
                        || effect != PGY_COLLECTION_EFFECT_OWNED_STRING_POP
                        || issued_receiver != receiver_id
                        || issued_source != 0
                        || !mir_collection_receipt_shape_ready(
                            inst, receiver_id))
                        goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_OWNED_STRING_POP;
                    op.binding_syntax_id = receiver_id;
                    op.syntax_id = ast_node_stable_id(inst->expr0);
                } else if (inst->has_collection_ownership_receipt
                    && pgy_collection_owned_local_fact_tracked(
                        mir_routine_collection_ownership_fact(routine,
                            inst->collection_ownership_receiver_binding_id)))
                {
                    failure_stage = "owned-orphan-receipt";
                    failure_binding =
                        inst->collection_ownership_receiver_binding_id;
                    goto done;
                }
            }
            if (op.kind != 0) operations[next_operation++] = op;
        }
        blocks[b].operation_count = next_operation - blocks[b].operation_start;
    }
    PgyCollectionTransitionView view = {
        .function_syntax_id = routine->source_syntax_id,
        .bindings = bindings,
        .binding_count = binding_count,
        .blocks = blocks,
        .block_count = routine->block_count,
        .predecessors = predecessors,
        .predecessor_count = predecessor_count,
        .operations = operations,
        .operation_count = next_operation,
        .entry_block = routine->entry_block,
        .retirements = retirements,
        .retirement_count = next_retirement
    };
    valid = pgy_collection_owned_local_transitions_validate(&view, error_message);
done:
    free(bindings); free(blocks); free(operations); free(retirements);
    free(predecessors);
    if (!valid && error_message != NULL && *error_message == NULL)
        return mir_collection_transition_error(routine, error_message, failure_binding, failure_stage);
    return valid;
}

static bool
mir_validate_collection_ownership_transitions_local(
    const MIRRoutine *routine,
    char **error_message)
{
    if (!mir_validate_loop_collection_cleanup(routine, error_message))
        return false;
    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_row = 0;
             inst_row < block->instruction_count;
             inst_row++) {
            const MIRInstruction *inst = &block->instructions[inst_row];
            const MIRCollectionOwnershipFact *fact;

            if (!inst->has_collection_ownership_receipt) {
                if (ast_call_semantic_collection_effect(inst->expr0, NULL, NULL, NULL))
                    return mir_collection_transition_error(routine, error_message, 0,
                        "missing-published-receipt");
                continue;
            }
            uint32_t issued_kind = 0, issued_receiver = 0, issued_source = 0;
            if (!ast_call_semantic_collection_effect(inst->expr0,
                    &issued_kind, &issued_receiver, &issued_source)
                || issued_kind != inst->collection_ownership_effect_kind
                || issued_receiver != inst->collection_ownership_receiver_binding_id
                || issued_source != inst->collection_ownership_source_binding_id)
                return mir_collection_transition_error(routine, error_message,
                    inst->collection_ownership_receiver_binding_id, "crosswired-published-receipt");
            fact = mir_routine_collection_ownership_fact(
                routine, inst->collection_ownership_receiver_binding_id);
            bool parameter_receipt = fact == NULL
                && mir_collection_parameter_receipt_ready(routine, inst);
            bool member_receipt = fact == NULL
                && mir_collection_member_receipt_ready(routine, inst);
            /* A MEMBER_MOVE fact alone is not an ownership grant.  The
             * semantic pass records each owned push as an exact call receipt,
             * proves its complete aggregate field path at finalization, and
             * only then seals the collection-effect snapshot.  The snapshot
             * replay above is therefore the existing SoT for this call; MIR
             * may consume it only for the same receiver and effect kind. */
            bool member_move_owned_push = fact != NULL
                && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE
                && issued_kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                && issued_receiver == fact->binding_syntax_id
                && mir_collection_receipt_shape_ready(
                    inst, fact->binding_syntax_id);
            /* Borrowed literal mutation is an effect receipt, not an owned
             * lifetime transition. It cannot authorize owned insertion/drop. */
            bool borrowed_mutation = false;
            if (fact != NULL && fact->origin == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL) {
                const MIRSourceLocalType *local =
                    mir_routine_source_local_type_fact_by_binding_syntax_id(
                        routine, fact->binding_syntax_id);
                uint32_t kind = 0, binding = 0, source = 0;
                borrowed_mutation = fact->function_syntax_id == routine->source_syntax_id
                    && fact->binding_syntax_id != 0 && fact->origin_syntax_id != 0
                    && fact->source_binding_syntax_id == 0
                    && fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS
                    && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                    && local != NULL && local->type_name != NULL
                    && strcmp(local->type_name, "Array<String>") == 0
                    && inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
                    && ast_call_semantic_collection_effect(inst->expr0, &kind, &binding, &source)
                    && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
                    && binding == fact->binding_syntax_id && source == 0;
            }
            /* Fact validation immediately preceding this pass has already
             * proved the exact identifier move and its one retired source.
             * Preserve a shallow borrowed-element receipt across that move;
             * it does not become an owned-local lifetime transition. */
            bool borrowed_move_mutation = false;
            if (fact != NULL && fact->origin == PGY_COLLECTION_ORIGIN_BINDING) {
                const MIRCollectionOwnershipFact *source_fact =
                    mir_routine_collection_ownership_fact(
                        routine, fact->source_binding_syntax_id);
                uint32_t kind = 0, binding = 0, source = 0;
                borrowed_move_mutation =
                    fact->function_syntax_id == routine->source_syntax_id
                    && fact->binding_syntax_id != 0
                    && fact->source_binding_syntax_id != 0
                    && fact->element_ownership
                        == PGY_STRING_ARRAY_BORROWED_ELEMENTS
                    && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                    && source_fact != NULL
                    && source_fact->origin
                        == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL
                    && source_fact->element_ownership
                        == PGY_STRING_ARRAY_BORROWED_ELEMENTS
                    && source_fact->disposition
                        == PGY_COLLECTION_DISPOSITION_RETIRED
                    && source_fact->source_binding_syntax_id == 0
                    && inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
                    && ast_call_semantic_collection_effect(
                        inst->expr0, &kind, &binding, &source)
                    && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
                    && binding == fact->binding_syntax_id && source == 0;
            }
            if ((!parameter_receipt && !member_receipt && fact == NULL)
                || (fact != NULL
                    && fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
                    && !pgy_collection_owned_local_fact_tracked(fact)
                    && !borrowed_mutation
                    && !borrowed_move_mutation
                    && !member_move_owned_push)
                || (fact != NULL
                    && !mir_collection_receipt_shape_ready(
                        inst, fact->binding_syntax_id))) {
                return mir_collection_transition_error(
                    routine, error_message,
                    inst->collection_ownership_receiver_binding_id,
                    "orphan-or-forged-receipt");
            }
        }
    }
    if (!mir_collection_validate_owned_local_transitions(routine, error_message))
        return false;
    for (size_t fact_row = 0;
         fact_row < routine->collection_ownership_fact_count;
         fact_row++) {
        const MIRCollectionOwnershipFact *fact =
            &routine->collection_ownership_facts[fact_row];
        MIRCollectionTransitionState *outputs;
        size_t iteration_limit;
        bool changed = true;

        if (fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL)
            continue;
        /* The operation, not the optional receipt, decides whether the state
         * machine is required. Otherwise deleting every receipt would make
         * its own absence invisible. */
        if (!mir_collection_binding_requires_transition_validation(
                routine, fact->binding_syntax_id))
            continue;
        if (routine->block_count == 0 || routine->blocks == NULL
            || routine->entry_block >= routine->block_count) {
            return mir_collection_transition_error(
                routine, error_message, fact->binding_syntax_id,
                "missing-cfg");
        }
        outputs = calloc(routine->block_count, sizeof(*outputs));
        if (outputs == NULL) {
            free(outputs);
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
        iteration_limit = routine->block_count * 8 + 1;
        for (size_t iteration = 0;
             changed && iteration < iteration_limit;
             iteration++) {
            changed = false;
            for (size_t block_id = 0;
                 block_id < routine->block_count;
                 block_id++) {
                const MIRBasicBlock *block = &routine->blocks[block_id];
                MIRCollectionTransitionState joined =
                    block_id == routine->entry_block
                        ? MIR_COLLECTION_STATE_UNBORN
                        : MIR_COLLECTION_STATE_BOTTOM;
                MIRCollectionTransitionState state;

                if (!block->is_reachable)
                    continue;
                for (size_t pred_row = 0;
                     pred_row < block->predecessor_count;
                     pred_row++) {
                    size_t predecessor = block->predecessors[pred_row];
                    if (predecessor >= routine->block_count) {
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            "predecessor");
                    }
                    if (routine->blocks[predecessor].is_reachable) {
                        MIRCollectionTransitionState incoming = outputs[predecessor];
                        if (incoming != MIR_COLLECTION_STATE_BOTTOM &&
                            mir_loop_collection_cleanup_edge(&routine->blocks[predecessor],
                                fact->binding_syntax_id, block_id))
                            incoming = MIR_COLLECTION_STATE_RETIRED;
                        joined = (MIRCollectionTransitionState)(
                            joined | incoming);
                    }
                }
                if (joined == MIR_COLLECTION_STATE_BOTTOM)
                    continue;
                state = joined;
                for (size_t inst_row = 0;
                     inst_row < block->instruction_count;
                     inst_row++) {
                    const MIRInstruction *inst =
                        &block->instructions[inst_row];
                    if (inst->kind == MIR_INST_DEF
                        && inst->binding_syntax_id == fact->binding_syntax_id) {
                        if (!mir_collection_empty_birth_ready(
                                routine, inst, fact)
                            || (state & ~(MIR_COLLECTION_STATE_UNBORN
                                          | MIR_COLLECTION_STATE_RETIRED)) != 0) {
                            free(outputs);
                            return mir_collection_empty_birth_error(
                                routine, error_message,
                                fact->binding_syntax_id, block_id, inst_row,
                                inst, fact, state);
                        }
                        state = MIR_COLLECTION_STATE_EMPTY;
                        continue;
                    }
                    bool relevant = mir_collection_relevant_call_for_binding(
                        inst, fact->binding_syntax_id);
                    if (inst->has_collection_ownership_receipt
                        && inst->collection_ownership_receiver_binding_id
                            != fact->binding_syntax_id)
                        continue;
                    if (!relevant
                        && !inst->has_collection_ownership_receipt)
                        continue;
                    if (!mir_collection_receipt_shape_ready(
                            inst, fact->binding_syntax_id)) {
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            relevant ? "missing-or-forged-receipt"
                                     : "cross-binding-receipt");
                    }
                    if (!mir_collection_transition_apply(inst, &state)) {
                        free(outputs);
                        return mir_collection_transition_apply_error(
                            routine, error_message, fact->binding_syntax_id,
                            block_id, inst_row, inst, state);
                    }
                }
                if (outputs[block_id] != state) {
                    outputs[block_id] = state;
                    changed = true;
                }
            }
        }
        if (changed) {
            free(outputs);
            return mir_collection_transition_error(
                routine, error_message, fact->binding_syntax_id,
                "non-convergent-cfg");
        }
        for (size_t block_id = 0;
             block_id < routine->block_count;
             block_id++) {
            if (routine->blocks[block_id].is_reachable
                && outputs[block_id] == MIR_COLLECTION_STATE_BOTTOM) {
                free(outputs);
                return mir_collection_transition_error(
                    routine, error_message, fact->binding_syntax_id,
                    "unresolved-reachable-block");
            }
        }
        free(outputs);
    }
    return true;
}

bool
mir_validate_collection_ownership_transitions_with_receipt(
    const MIRRoutine *routine,
    const MIRCollectionEffectEvidenceReceipt *receipt,
    char **error_message)
{
    if (routine == NULL
        || !mir_use_collection_effect_evidence_receipt(
               receipt, routine, error_message)) {
        return false;
    }
    return mir_validate_collection_ownership_transitions_local(
        routine, error_message);
}

bool
mir_validate_collection_ownership_transitions(const MIRRoutine *routine,
                                              char **error_message)
{
    if (routine == NULL)
        return false;
    if (!mir_validate_collection_effect_evidence(routine->program, routine,
                                                  error_message))
        return false;
    return mir_validate_collection_ownership_transitions_local(
        routine, error_message);
}
