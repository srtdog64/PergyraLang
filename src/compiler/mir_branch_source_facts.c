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
                > (unsigned)PGY_COLLECTION_ORIGIN_BINDING
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
                > (unsigned)PGY_COLLECTION_ORIGIN_BINDING) {
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
