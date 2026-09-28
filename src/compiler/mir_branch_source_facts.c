#include "mir_branch_source_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

const MIRMatchBindingTypeFact *
mir_routine_match_binding_type_fact(const MIRRoutine *routine,
                                    uint32_t match_case_syntax_id,
                                    size_t binding_index)
{
    if (routine == NULL || match_case_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < routine->match_binding_type_fact_count; i++) {
        const MIRMatchBindingTypeFact *fact =
            &routine->match_binding_type_facts[i];
        if (fact->match_case_syntax_id == match_case_syntax_id
            && fact->binding_index == binding_index)
            return fact;
    }
    return NULL;
}

const MIRMatchBindingTypeFact *
mir_routine_match_binding_type_fact_by_binding_syntax_id(
    const MIRRoutine *routine,
    uint32_t binding_syntax_id)
{
    if (routine == NULL || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < routine->match_binding_type_fact_count; i++) {
        const MIRMatchBindingTypeFact *fact =
            &routine->match_binding_type_facts[i];
        if (fact->binding_syntax_id == binding_syntax_id)
            return fact;
    }
    return NULL;
}

bool
mir_copy_match_binding_type_facts(MIRRoutine *routine,
                                  const HIRRoutine *hir_routine,
                                  char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->match_binding_type_fact_count;
    if (count == 0)
        return true;
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->match_binding_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR match binding type facts have incomplete routine identity or storage");
        return false;
    }
    routine->match_binding_type_facts = calloc(
        count, sizeof(*routine->match_binding_type_facts));
    if (routine->match_binding_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->match_binding_type_fact_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRMatchBindingTypeFact *source =
            &hir_routine->match_binding_type_facts[i];
        MIRMatchBindingTypeFact *target =
            &routine->match_binding_type_facts[i];
        if (source->function_syntax_id != routine->source_syntax_id
            || source->match_case_syntax_id == 0
            || source->binding_syntax_id == 0
            || source->binding_count == 0
            || source->binding_index >= source->binding_count
            || source->binding_type_name == NULL
            || source->binding_type_name[0] == '\0'
            || mir_routine_match_binding_type_fact(
                routine, source->match_case_syntax_id,
                source->binding_index) != NULL
            || mir_routine_match_binding_type_fact_by_binding_syntax_id(
                routine, source->binding_syntax_id) != NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR match binding type facts have invalid or duplicate identity");
            goto fail;
        }
        *target = *source;
        target->binding_type_name = pergyra_strdup(source->binding_type_name);
        if (target->binding_type_name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
        routine->match_binding_type_fact_count++;
    }
    return true;

fail:
    mir_free_match_binding_type_facts(routine);
    return false;
}

void
mir_free_match_binding_type_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    for (size_t i = 0; i < routine->match_binding_type_fact_count; i++)
        free(routine->match_binding_type_facts[i].binding_type_name);
    free(routine->match_binding_type_facts);
    routine->match_binding_type_facts = NULL;
    routine->match_binding_type_fact_count = 0;
    routine->match_binding_type_fact_capacity = 0;
}

const MIRCollectionOwnershipFact *
mir_routine_collection_ownership_fact(const MIRRoutine *routine,
                                      uint32_t binding_syntax_id)
{
    if (routine == NULL || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++) {
        const MIRCollectionOwnershipFact *fact =
            &routine->collection_ownership_facts[i];
        if (fact->binding_syntax_id == binding_syntax_id)
            return fact;
    }
    return NULL;
}

bool
mir_copy_collection_ownership_facts(MIRRoutine *routine,
                                    const HIRRoutine *hir_routine,
                                    char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->collection_ownership_fact_count;
    if (count == 0)
        return hir_routine->collection_ownership_facts == NULL;
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->collection_ownership_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR collection ownership facts have incomplete routine identity or storage");
        return false;
    }
    routine->collection_ownership_facts = calloc(
        count, sizeof(*routine->collection_ownership_facts));
    if (routine->collection_ownership_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->collection_ownership_fact_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRCollectionOwnershipFact *source =
            &hir_routine->collection_ownership_facts[i];
        if (source->function_syntax_id != routine->source_syntax_id
            || source->binding_syntax_id == 0
            || (unsigned)source->element_ownership
                > (unsigned)PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
            || (source->disposition != PGY_COLLECTION_DISPOSITION_LIVE
                && source->disposition
                    != PGY_COLLECTION_DISPOSITION_RETIRED)
            || (unsigned)source->origin
                > (unsigned)PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
            || (source->origin == PGY_COLLECTION_ORIGIN_BINDING
                && source->source_binding_syntax_id == 0)
            || (source->origin != PGY_COLLECTION_ORIGIN_BINDING
                && source->source_binding_syntax_id != 0)
            || mir_routine_collection_ownership_fact(
                routine, source->binding_syntax_id) != NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR collection ownership facts have invalid or duplicate identity");
            goto fail;
        }
        routine->collection_ownership_facts[
            routine->collection_ownership_fact_count++] = *source;
    }
    if (!mir_validate_collection_ownership_facts(routine, error_message))
        goto fail;
    return true;

fail:
    mir_free_collection_ownership_facts(routine);
    return false;
}

static const MIRSourceLocalType *
mir_collection_source_local(const MIRRoutine *routine,
                            uint32_t binding_syntax_id)
{
    if (routine == NULL || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        const MIRSourceLocalType *local = &routine->source_local_types[i];
        if (local->binding_syntax_id == binding_syntax_id)
            return local;
    }
    return NULL;
}

bool
mir_validate_collection_ownership_facts(const MIRRoutine *routine,
                                        char **error_message)
{
    if (routine == NULL)
        return false;
    if ((routine->collection_ownership_fact_count == 0)
        != (routine->collection_ownership_facts == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR collection ownership row storage is inconsistent");
        return false;
    }
    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++) {
        const MIRCollectionOwnershipFact *fact =
            &routine->collection_ownership_facts[i];
        const MIRSourceLocalType *target =
            mir_collection_source_local(routine, fact->binding_syntax_id);
        const MIRCollectionOwnershipFact *source_fact = NULL;
        const MIRSourceLocalType *source_local = NULL;
        bool origin_consistent = false;

        if (fact->function_syntax_id != routine->source_syntax_id
            || fact->binding_syntax_id == 0
            || fact->origin_syntax_id == 0
            || target == NULL || target->type_name == NULL
            || strcmp(target->type_name, "Array<String>") != 0
            || (unsigned)fact->element_ownership
                > (unsigned)PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
            || (fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
                && fact->disposition != PGY_COLLECTION_DISPOSITION_RETIRED)
            || (fact->disposition == PGY_COLLECTION_DISPOSITION_RETIRED
                && fact->element_ownership
                    != PGY_STRING_ARRAY_OWNED_ELEMENTS
                && fact->element_ownership
                    != PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
            || (unsigned)fact->origin
                > (unsigned)PGY_COLLECTION_ORIGIN_EMPTY_LITERAL) {
            goto invalid;
        }
        for (size_t prior = 0; prior < i; prior++) {
            if (routine->collection_ownership_facts[prior].binding_syntax_id
                == fact->binding_syntax_id)
                goto invalid;
            if (routine->collection_ownership_facts[prior].binding_syntax_id
                == fact->source_binding_syntax_id)
                source_fact = &routine->collection_ownership_facts[prior];
        }
        if (fact->source_binding_syntax_id != 0)
            source_local = mir_collection_source_local(
                routine, fact->source_binding_syntax_id);

        switch (fact->origin) {
            case PGY_COLLECTION_ORIGIN_UNKNOWN:
                origin_consistent =
                    (fact->element_ownership
                         == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN)
                    && fact->source_binding_syntax_id == 0
                    && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE;
                break;
            case PGY_COLLECTION_ORIGIN_BORROWED_LITERAL:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_BORROWED_ELEMENTS
                    && fact->source_binding_syntax_id == 0
                    && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE;
                break;
            case PGY_COLLECTION_ORIGIN_MAP_KEYS:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
                    && fact->source_binding_syntax_id == 0;
                break;
            case PGY_COLLECTION_ORIGIN_BINDING:
                origin_consistent =
                    fact->source_binding_syntax_id != 0
                    && fact->source_binding_syntax_id
                        != fact->binding_syntax_id
                    && source_fact != NULL && source_local != NULL
                    && source_local->type_name != NULL
                    && strcmp(source_local->type_name,
                              "Array<String>") == 0
                    && source_fact->element_ownership
                        == fact->element_ownership
                    && source_fact->disposition
                        == PGY_COLLECTION_DISPOSITION_LIVE
                    && fact->disposition
                        == PGY_COLLECTION_DISPOSITION_LIVE;
                break;
            case PGY_COLLECTION_ORIGIN_EMPTY_LITERAL:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
                    && fact->source_binding_syntax_id == 0
                    && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE;
                break;
        }
        if (!origin_consistent)
            goto invalid;
        continue;

invalid:
        if (error_message != NULL) {
            char detail[512];
            snprintf(detail, sizeof(detail),
                     "MIR collection ownership facts have invalid identity, type, or provenance "
                     "(row=%zu function=%u routine=%u binding=%u origin_syntax=%u "
                     "source=%u ownership=%u disposition=%u origin=%u target_type=%s "
                     "source_local=%s source_fact=%s)",
                     i, fact->function_syntax_id, routine->source_syntax_id,
                     fact->binding_syntax_id, fact->origin_syntax_id,
                     fact->source_binding_syntax_id,
                     (unsigned)fact->element_ownership,
                     (unsigned)fact->disposition, (unsigned)fact->origin,
                     target != NULL && target->type_name != NULL
                         ? target->type_name : "<missing>",
                     source_local != NULL && source_local->type_name != NULL
                         ? source_local->type_name : "<missing>",
                     source_fact != NULL ? "present" : "missing");
            *error_message = pergyra_strdup(detail);
        }
        return false;
    }
    return true;
}

typedef enum
{
    MIR_COLLECTION_STATE_BOTTOM = 0,
    MIR_COLLECTION_STATE_EMPTY,
    MIR_COLLECTION_STATE_BORROWED,
    MIR_COLLECTION_STATE_OWNED,
    MIR_COLLECTION_STATE_RETIRED,
    MIR_COLLECTION_STATE_CONFLICT
} MIRCollectionTransitionState;

static MIRCollectionTransitionState
mir_collection_state_join(MIRCollectionTransitionState left,
                          MIRCollectionTransitionState right)
{
    if (left == MIR_COLLECTION_STATE_BOTTOM)
        return right;
    if (right == MIR_COLLECTION_STATE_BOTTOM || left == right)
        return left;
    if ((left == MIR_COLLECTION_STATE_EMPTY
         && right == MIR_COLLECTION_STATE_BORROWED)
        || (left == MIR_COLLECTION_STATE_BORROWED
            && right == MIR_COLLECTION_STATE_EMPTY))
        return MIR_COLLECTION_STATE_BORROWED;
    if ((left == MIR_COLLECTION_STATE_EMPTY
         && right == MIR_COLLECTION_STATE_OWNED)
        || (left == MIR_COLLECTION_STATE_OWNED
            && right == MIR_COLLECTION_STATE_EMPTY))
        return MIR_COLLECTION_STATE_OWNED;
    return MIR_COLLECTION_STATE_CONFLICT;
}

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
mir_collection_receipt_shape_ready(const MIRInstruction *inst,
                                   uint32_t binding_syntax_id)
{
    const char *name;
    ASTNode *receiver;
    uint32_t receiver_id;
    bool expected = false;

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

    switch ((PgyCollectionOwnershipEffectKind)
                inst->collection_ownership_effect_kind) {
    case PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH:
        expected = strcmp(name, "ArrayPushOwnedString") == 0;
        break;
    case PGY_COLLECTION_EFFECT_SHALLOW_MUTATION:
        expected = strcmp(name, "ArrayPush") == 0
            || strcmp(name, "ArraySet") == 0
            || strcmp(name, "ArrayPop") == 0;
        break;
    case PGY_COLLECTION_EFFECT_DROP:
        expected = strcmp(name, "ArrayDropOwnedStrings") == 0;
        break;
    default:
        expected = false;
        break;
    }
    return inst->has_collection_ownership_receipt && expected
        && inst->collection_ownership_receiver_binding_id
            == binding_syntax_id
        && inst->collection_ownership_source_binding_id == 0;
}

static bool
mir_collection_relevant_call_for_binding(const MIRInstruction *inst,
                                         uint32_t binding_syntax_id)
{
    const char *name = mir_collection_call_name(inst);
    ASTNode *receiver;

    if (name == NULL
        || (strcmp(name, "ArrayPushOwnedString") != 0
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
                        == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION)) {
                return true;
            }
        }
    }
    return false;
}

static bool
mir_collection_transition_apply(const MIRInstruction *inst,
                                MIRCollectionTransitionState *state)
{
    if (inst == NULL || state == NULL
        || !inst->has_collection_ownership_receipt)
        return false;
    switch ((PgyCollectionOwnershipEffectKind)
                inst->collection_ownership_effect_kind) {
    case PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH:
        if (*state != MIR_COLLECTION_STATE_EMPTY
            && *state != MIR_COLLECTION_STATE_OWNED)
            return false;
        *state = MIR_COLLECTION_STATE_OWNED;
        return true;
    case PGY_COLLECTION_EFFECT_SHALLOW_MUTATION:
        if (*state != MIR_COLLECTION_STATE_EMPTY
            && *state != MIR_COLLECTION_STATE_BORROWED)
            return false;
        *state = MIR_COLLECTION_STATE_BORROWED;
        return true;
    case PGY_COLLECTION_EFFECT_DROP:
        if (*state != MIR_COLLECTION_STATE_EMPTY
            && *state != MIR_COLLECTION_STATE_OWNED)
            return false;
        *state = MIR_COLLECTION_STATE_RETIRED;
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
        const MIRSourceLocalType *local = mir_collection_source_local(
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

bool
mir_validate_collection_ownership_transitions(const MIRRoutine *routine,
                                              char **error_message)
{
    if (routine == NULL)
        return false;
    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_row = 0;
             inst_row < block->instruction_count;
             inst_row++) {
            const MIRInstruction *inst = &block->instructions[inst_row];
            const MIRCollectionOwnershipFact *fact;

            if (!inst->has_collection_ownership_receipt)
                continue;
            fact = mir_routine_collection_ownership_fact(
                routine, inst->collection_ownership_receiver_binding_id);
            if (fact == NULL
                || fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
                || inst->collection_ownership_source_binding_id != 0
                || !mir_collection_receipt_shape_ready(
                    inst, fact->binding_syntax_id)) {
                return mir_collection_transition_error(
                    routine, error_message,
                    inst->collection_ownership_receiver_binding_id,
                    "orphan-or-forged-receipt");
            }
        }
    }
    for (size_t fact_row = 0;
         fact_row < routine->collection_ownership_fact_count;
         fact_row++) {
        const MIRCollectionOwnershipFact *fact =
            &routine->collection_ownership_facts[fact_row];
        MIRCollectionTransitionState *inputs;
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
        inputs = calloc(routine->block_count, sizeof(*inputs));
        outputs = calloc(routine->block_count, sizeof(*outputs));
        if (inputs == NULL || outputs == NULL) {
            free(inputs);
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
                        ? MIR_COLLECTION_STATE_EMPTY
                        : MIR_COLLECTION_STATE_BOTTOM;
                MIRCollectionTransitionState state;

                if (!block->is_reachable)
                    continue;
                for (size_t pred_row = 0;
                     pred_row < block->predecessor_count;
                     pred_row++) {
                    size_t predecessor = block->predecessors[pred_row];
                    if (predecessor >= routine->block_count) {
                        free(inputs);
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            "predecessor");
                    }
                    if (routine->blocks[predecessor].is_reachable) {
                        joined = mir_collection_state_join(
                            joined, outputs[predecessor]);
                    }
                }
                if (joined == MIR_COLLECTION_STATE_BOTTOM)
                    continue;
                if (joined == MIR_COLLECTION_STATE_CONFLICT) {
                    free(inputs);
                    free(outputs);
                    return mir_collection_transition_error(
                        routine, error_message, fact->binding_syntax_id,
                        "branch-join");
                }
                state = joined;
                for (size_t inst_row = 0;
                     inst_row < block->instruction_count;
                     inst_row++) {
                    const MIRInstruction *inst =
                        &block->instructions[inst_row];
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
                        free(inputs);
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            relevant ? "missing-or-forged-receipt"
                                     : "cross-binding-receipt");
                    }
                    if (!mir_collection_transition_apply(inst, &state)) {
                        free(inputs);
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            "invalid-state-transition");
                    }
                }
                if (inputs[block_id] != joined
                    || outputs[block_id] != state) {
                    inputs[block_id] = joined;
                    outputs[block_id] = state;
                    changed = true;
                }
            }
        }
        if (changed) {
            free(inputs);
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
                free(inputs);
                free(outputs);
                return mir_collection_transition_error(
                    routine, error_message, fact->binding_syntax_id,
                    "unresolved-reachable-block");
            }
        }
        free(inputs);
        free(outputs);
    }
    return true;
}

void
mir_free_collection_ownership_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    free(routine->collection_ownership_facts);
    routine->collection_ownership_facts = NULL;
    routine->collection_ownership_fact_count = 0;
    routine->collection_ownership_fact_capacity = 0;
}

MIRBranchShape
mir_branch_shape_from_ast(const ASTNode *node)
{
    if (node == NULL)
        return MIR_BRANCH_EXPR;
    if (node->type == AST_FOR_LOOP)
        return ast_for_iterable(node) != NULL ? MIR_BRANCH_FOR_IN
                                              : MIR_BRANCH_FOR_RANGE;
    if (node->type == AST_MATCH_CASE)
        return MIR_BRANCH_MATCH_CASE;
    if (node->type == AST_BLOCK)
        return MIR_BRANCH_SELECT_DISPATCH;
    return MIR_BRANCH_EXPR;
}

ASTNode *
mir_select_case_channel(ASTNode *node)
{
    ASTNode *first = node != NULL && node->type == AST_BLOCK
        && ast_block_statement_count(node) > 0
            ? ast_block_statement(node, 0)
            : NULL;
    ASTNode *value = first != NULL && first->type == AST_ASSIGNMENT
        ? ast_assignment_value(first) : first;
    return value != NULL && value->type == AST_CHANNEL_RECV
        ? ast_channel_recv_channel(value) : NULL;
}

bool
mir_capture_match_case_facts(MIRRoutine *routine, MIRInstruction *inst,
                             ASTNode *case_node, ASTNode *subject_node)
{
    size_t binding_count;
    uint32_t match_case_id;

    if (inst == NULL)
        return false;
    inst->expr0 = subject_node;
    inst->match_case_pattern = ast_match_case_pattern(case_node);
    inst->match_case_patterns =
        ast_match_case_patterns(case_node, &inst->match_case_pattern_count);
    inst->match_case_guard = ast_match_case_guard(case_node);
    inst->match_subject_family =
        ast_match_case_semantic_subject_family(case_node);
    binding_count = mir_instruction_match_binding_count(inst);
    if (binding_count == 0)
        return true;
    if (routine == NULL || case_node == NULL)
        return false;
    match_case_id = ast_node_stable_id(case_node);
    if (match_case_id == 0)
        return false;
    inst->match_binding_type_names = calloc(
        binding_count, sizeof(*inst->match_binding_type_names));
    if (inst->match_binding_type_names == NULL)
        return false;
    for (size_t i = 0; i < binding_count; i++) {
        const MIRMatchBindingTypeFact *fact =
            mir_routine_match_binding_type_fact(routine, match_case_id, i);
        if (fact == NULL || fact->binding_count != binding_count
            || fact->binding_type_name == NULL
            || fact->binding_type_name[0] == '\0') {
            free((void *)inst->match_binding_type_names);
            inst->match_binding_type_names = NULL;
            return false;
        }
        inst->match_binding_type_names[i] = fact->binding_type_name;
        inst->match_binding_type_count++;
    }
    return true;
}
