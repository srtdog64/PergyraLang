#include "mir_branch_source_facts.h"
#include "mir_branch_source_facts_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "mir_decl_headers.h"
#include "collection_owned_local_transition_owner.h"
#include "../semantic/builtin_kind.h"
#include "mir_json_expression_graph_materialize.h"

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
                > (unsigned)PGY_COLLECTION_ORIGIN_CLONE
            || (source->origin == PGY_COLLECTION_ORIGIN_BINDING
                && source->source_binding_syntax_id == 0)
            || (source->origin != PGY_COLLECTION_ORIGIN_BINDING
                && source->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
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
    /* HIR already sealed the carried rows.  Exact binding-move evidence uses
     * MIR def/use identity and therefore cannot be checked until instruction
     * population and SSA rename finish.  mir_validate() owns that final
     * fail-closed check before any artifact is emitted. */
    return true;

fail:
    mir_free_collection_ownership_facts(routine);
    return false;
}

const char *
mir_collection_binding_type_name(const MIRRoutine *routine,
                                 uint32_t binding_syntax_id)
{
    const MIRSourceLocalType *local =
        mir_routine_source_local_type_fact_by_binding_syntax_id(
            routine, binding_syntax_id);
    const char *parameter_type = NULL;
    size_t parameter_matches = 0;

    if (routine == NULL || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        if (param != NULL
            && ast_func_param_stable_id(param) == binding_syntax_id) {
            parameter_matches++;
            parameter_type = mir_routine_param_type_name(routine, i);
        }
    }
    if (local != NULL)
        return parameter_matches == 0 ? local->type_name : NULL;
    return parameter_matches == 1 ? parameter_type : NULL;
}

static bool
mir_collection_member_source_matches(const MIRRoutine *routine,
                                     uint32_t source_binding_syntax_id,
                                     uint32_t field_syntax_id)
{
    const char *source_type = mir_collection_binding_type_name(
        routine, source_binding_syntax_id);
    const MIRDeclHeader *header = NULL;
    size_t base_length;

    if (routine == NULL || routine->program == NULL || source_type == NULL
        || source_type[0] == '\0' || field_syntax_id == 0) {
        return false;
    }
    base_length = strcspn(source_type, "<");
    for (size_t i = 0; i < routine->program->decl_header_count; i++) {
        const MIRDeclHeader *candidate = &routine->program->decl_headers[i];
        const char *name = mir_decl_header_name(candidate);
        if (name != NULL && strlen(name) == base_length
            && strncmp(name, source_type, base_length) == 0) {
            if (header != NULL)
                return false;
            header = candidate;
        }
    }
    if (header == NULL)
        return false;
    for (size_t i = 0; i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        const char *field_type = mir_decl_field_type_name(field);
        if (mir_decl_field_source_syntax_id(field) == field_syntax_id
            && field_type != NULL
            && strcmp(field_type, "Array<String>") == 0) {
            return true;
        }
    }
    return false;
}

bool
mir_collection_binding_move_source_value_matches(
    const MIRRoutine *routine,
    const MIRInstruction *destination,
    uint32_t source_binding_syntax_id)
{
    if (routine == NULL || destination == NULL
        || destination->use_count != 1 || destination->uses == NULL
        || destination->uses[0] == NULL)
        return false;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *candidate = &block->instructions[i];
            if (candidate == destination)
                return false;
            if (candidate->binding_syntax_id == source_binding_syntax_id
                && candidate->result_name != NULL
                && strcmp(candidate->result_name,
                          destination->uses[0]) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool
mir_collection_binding_move_matches(const MIRRoutine *routine,
                                    uint32_t destination_binding_syntax_id,
                                    uint32_t source_binding_syntax_id)
{
    size_t matched = 0;

    if (routine == NULL || destination_binding_syntax_id == 0
        || source_binding_syntax_id == 0
        || destination_binding_syntax_id == source_binding_syntax_id)
        return false;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *instruction = &block->instructions[i];
            if (instruction->binding_syntax_id
                    != destination_binding_syntax_id)
                continue;
            if (instruction->kind != MIR_INST_DEF
                || instruction->source_stable_id != destination_binding_syntax_id
                || instruction->expr0 == NULL
                || instruction->expr0->type != AST_IDENTIFIER
                || !mir_collection_binding_move_source_value_matches(
                    routine, instruction, source_binding_syntax_id)) {
                return false;
            }
            matched++;
        }
    }
    return matched == 1;
}

static bool
mir_collection_retired_source_has_destination(
    const MIRRoutine *routine,
    const MIRCollectionOwnershipFact *source)
{
    size_t matched = 0;

    if (routine == NULL || source == NULL)
        return false;
    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++) {
        const MIRCollectionOwnershipFact *candidate =
            &routine->collection_ownership_facts[i];
        if (candidate->origin == PGY_COLLECTION_ORIGIN_BINDING
            && candidate->source_binding_syntax_id
                == source->binding_syntax_id
            && candidate->element_ownership == source->element_ownership)
            matched++;
    }
    return matched == 1;
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
            mir_routine_source_local_type_fact_by_binding_syntax_id(
                routine, fact->binding_syntax_id);
        const MIRCollectionOwnershipFact *source_fact = NULL;
        const char *source_type = NULL;
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
                && fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
                && fact->element_ownership
                    != PGY_STRING_ARRAY_OWNED_ELEMENTS
                && fact->element_ownership
                    != PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
                && !mir_collection_retired_source_has_destination(
                    routine, fact))
            || (unsigned)fact->origin
                > (unsigned)PGY_COLLECTION_ORIGIN_CLONE) {
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
            source_type = mir_collection_binding_type_name(
                routine, fact->source_binding_syntax_id);

        switch (fact->origin) {
            case PGY_COLLECTION_ORIGIN_UNKNOWN:
                origin_consistent =
                    (fact->element_ownership
                         == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN)
                    && fact->source_binding_syntax_id == 0;
                break;
            case PGY_COLLECTION_ORIGIN_BORROWED_LITERAL:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_BORROWED_ELEMENTS
                    && fact->source_binding_syntax_id == 0;
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
                    && source_fact != NULL && source_type != NULL
                    && strcmp(source_type, "Array<String>") == 0
                    && source_fact->element_ownership
                        == fact->element_ownership
                    && source_fact->disposition
                        == PGY_COLLECTION_DISPOSITION_RETIRED
                    && mir_collection_binding_move_matches(
                        routine, fact->binding_syntax_id,
                        fact->source_binding_syntax_id);
                break;
            case PGY_COLLECTION_ORIGIN_EMPTY_LITERAL:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
                    && fact->source_binding_syntax_id == 0
                    && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE;
                break;
            case PGY_COLLECTION_ORIGIN_MEMBER_MOVE:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
                    && fact->source_binding_syntax_id != 0
                    && fact->source_binding_syntax_id
                        != fact->binding_syntax_id
                    && mir_collection_member_source_matches(
                        routine, fact->source_binding_syntax_id,
                        fact->origin_syntax_id)
                    && (fact->disposition
                            == PGY_COLLECTION_DISPOSITION_LIVE
                        || fact->disposition
                            == PGY_COLLECTION_DISPOSITION_RETIRED);
                break;
            case PGY_COLLECTION_ORIGIN_CLONE:
                origin_consistent =
                    fact->element_ownership
                        == PGY_STRING_ARRAY_OWNED_ELEMENTS
                    && fact->source_binding_syntax_id == 0;
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
                      source_type != NULL ? source_type : "<missing>",
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
