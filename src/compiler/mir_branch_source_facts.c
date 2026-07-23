#include "mir_branch_source_facts.h"

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
            || source->binding_count == 0
            || source->binding_index >= source->binding_count
            || source->binding_type_name == NULL
            || source->binding_type_name[0] == '\0'
            || mir_routine_match_binding_type_fact(
                routine, source->match_case_syntax_id,
                source->binding_index) != NULL) {
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
