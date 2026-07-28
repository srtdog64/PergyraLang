#include "dir_internal.h"
#include "hir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static void
dir_clear_error_message(DIRProgram *dir)
{
    if (dir == NULL)
        return;
    free(dir->error_message);
    dir->error_message = NULL;
}

static uint64_t
dir_domain_graph_anchor(uint32_t source_id, size_t node_count, size_t edge_count)
{
    uint64_t value = (uint64_t)source_id * UINT64_C(0x9e3779b97f4a7c15);
    value ^= (uint64_t)node_count + UINT64_C(0x517cc1b727220a95);
    value ^= ((uint64_t)edge_count << 32) | (uint64_t)edge_count;
    return value != 0 ? value : UINT64_C(1);
}

bool
dir_failf(DIRProgram *dir, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *message;

    if (dir == NULL || dir->error_message != NULL)
        return false;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return false;
    }

    message = malloc((size_t)length + 1);
    if (message == NULL) {
        va_end(args);
        return false;
    }

    vsnprintf(message, (size_t)length + 1, fmt, args);
    va_end(args);
    dir->error_message = message;
    return false;
}

static char *
dir_type_name_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

static char *
dir_render_type_name_dup(ASTNode *type_node)
{
    char *result = NULL;

    if (type_node == NULL)
        return NULL;

    switch (type_node->type) {
    case AST_TYPE: {
        const char *base_name = ast_type_name(type_node);
        if (base_name == NULL)
            return NULL;
        result = pergyra_strdup(base_name);
        if (result == NULL)
            return NULL;
        GenericParams *generic_args = ast_type_generic_args(type_node);
        size_t generic_count = ast_generic_param_count(generic_args);
        if (generic_count > 0) {
            char *next = dir_type_name_strdup_fmt("%s<", result);
            free(result);
            result = next;
            if (result == NULL)
                return NULL;
            for (size_t i = 0; i < generic_count; i++) {
                GenericParam *param = ast_generic_param_at(generic_args, i);
                char *arg_text = NULL;
                if (ast_generic_param_constraint(param) != NULL) {
                    arg_text = dir_render_type_name_dup(
                        ast_generic_param_constraint(param));
                } else if (ast_generic_param_name(param) != NULL) {
                    arg_text = pergyra_strdup(ast_generic_param_name(param));
                } else if (ast_generic_param_default_type(param) != NULL) {
                    arg_text = dir_render_type_name_dup(
                        ast_generic_param_default_type(param));
                }
                if (arg_text == NULL) {
                    free(result);
                    return NULL;
                }
                next = dir_type_name_strdup_fmt(
                    "%s%s%s",
                    result,
                    i > 0 ? ", " : "",
                    arg_text);
                free(arg_text);
                free(result);
                result = next;
                if (result == NULL)
                    return NULL;
            }
            next = dir_type_name_strdup_fmt("%s>", result);
            free(result);
            result = next;
        }
        return result;
    }
    case AST_CHANNEL_TYPE: {
        char *inner = dir_render_type_name_dup(
            ast_channel_type_element_type(type_node));
        if (inner != NULL)
            result = dir_type_name_strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }
    case AST_FUTURE_TYPE: {
        char *inner = dir_render_type_name_dup(
            ast_future_type_value_type(type_node));
        if (inner != NULL)
            result = dir_type_name_strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }
    default:
        break;
    }

    return NULL;
}

const char *
type_name(DIRProgram *dir, ASTNode *type_node)
{
    char *owned;

    if (dir == NULL || type_node == NULL)
        return NULL;

    owned = dir_render_type_name_dup(type_node);
    if (owned == NULL)
        return NULL;
    if (!dir_track_owned_name(dir, owned)) {
        free(owned);
        return NULL;
    }
    return owned;
}

bool
dir_domain_slot_is_projection(ASTNode *slot)
{
    return slot != NULL
        && slot->type == AST_DOMAIN_SLOT
        && !ast_domain_slot_is_subject(slot)
        && !ast_domain_slot_is_vessel(slot);
}

static bool
dir_domain_runtime_text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static bool
dir_validate_hir_domain_runtime_fact_source(const HIRProgram *hir,
                                            char **error_message)
{
    if (!hir->has_domain_runtime_facts) {
        if (hir->domain_participant_role_facts != NULL
            || hir->domain_participant_role_fact_count != 0
            || hir->domain_projection_member_assignment_facts != NULL
            || hir->domain_projection_member_assignment_fact_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "DIR lowering found HIR domain runtime storage without its semantic snapshot marker");
            return false;
        }
        return true;
    }
    if ((hir->domain_participant_role_fact_count == 0
         && hir->domain_participant_role_facts != NULL)
        || (hir->domain_participant_role_fact_count != 0
            && hir->domain_participant_role_facts == NULL)
        || (hir->domain_projection_member_assignment_fact_count == 0
            && hir->domain_projection_member_assignment_facts != NULL)
        || (hir->domain_projection_member_assignment_fact_count != 0
            && hir->domain_projection_member_assignment_facts == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR lowering found incomplete HIR domain runtime fact storage");
        return false;
    }
    for (size_t i = 0; i < hir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &hir->domain_participant_role_facts[i];
        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != hir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->field_syntax_id == 0
            || (unsigned)fact->role
                > (unsigned)PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
            || !dir_domain_runtime_text_present(fact->owner_name)
            || !dir_domain_runtime_text_present(fact->field_name)
            || !dir_domain_runtime_text_present(fact->field_type_name)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "DIR lowering rejected an incomplete HIR domain participant-role fact");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainParticipantRoleFact *prior =
                &hir->domain_participant_role_facts[j];
            if ((prior->program_syntax_id == fact->program_syntax_id
                 && prior->owner_syntax_id == fact->owner_syntax_id
                 && prior->role == fact->role)
                || prior->field_syntax_id == fact->field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR lowering rejected duplicate HIR domain participant-role identity");
                return false;
            }
        }
    }
    for (size_t i = 0;
         i < hir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &hir->domain_projection_member_assignment_facts[i];
        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != hir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->directive_syntax_id == 0
            || fact->projection_slot_syntax_id == 0
            || fact->source_slot_syntax_id == 0
            || fact->target_decl_syntax_id == 0
            || fact->target_field_syntax_id == 0
            || fact->source_decl_syntax_id == 0
            || (unsigned)fact->operation
                > (unsigned)PGY_DOMAIN_PROJECTION_BIND
            || !dir_domain_runtime_text_present(fact->owner_name)
            || !dir_domain_runtime_text_present(fact->projection_slot_name)
            || !dir_domain_runtime_text_present(fact->source_slot_name)
            || !dir_domain_runtime_text_present(fact->target_field_name)
            || !dir_domain_runtime_text_present(
                fact->target_field_type_name)
            || !dir_domain_runtime_text_present(fact->source_path)
            || !dir_domain_runtime_text_present(fact->source_leaf_type_name)
            || fact->source_path_segment_count == 0
            || fact->source_path_segments == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "DIR lowering rejected an incomplete HIR domain projection assignment");
            return false;
        }
        for (size_t s = 0; s < fact->source_path_segment_count; s++) {
            const PgyDomainProjectionPathSegmentFact *segment =
                &fact->source_path_segments[s];
            if (segment->field_syntax_id == 0
                || !dir_domain_runtime_text_present(segment->field_name)
                || !dir_domain_runtime_text_present(
                    segment->field_type_name)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR lowering rejected an incomplete HIR domain projection source path");
                return false;
            }
        }
        if (strcmp(fact->source_path_segments[
                       fact->source_path_segment_count - 1]
                       .field_type_name,
                   fact->source_leaf_type_name) != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "DIR lowering rejected HIR domain projection leaf-type drift");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &hir->domain_projection_member_assignment_facts[j];
            if (prior->program_syntax_id == fact->program_syntax_id
                && prior->owner_syntax_id == fact->owner_syntax_id
                && prior->directive_syntax_id == fact->directive_syntax_id
                && prior->projection_slot_syntax_id
                    == fact->projection_slot_syntax_id
                && prior->target_field_syntax_id
                    == fact->target_field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR lowering rejected duplicate HIR domain projection member identity");
                return false;
            }
        }
    }
    return true;
}

static char *
dir_domain_runtime_strdup(const char *source)
{
    return source != NULL ? pergyra_strdup(source) : NULL;
}

static bool
dir_copy_hir_domain_runtime_facts(DIRProgram *dir,
                                  const HIRProgram *hir,
                                  char **error_message)
{
    if (dir == NULL || hir == NULL
        || !dir_validate_hir_domain_runtime_fact_source(
            hir, error_message)) {
        return false;
    }
    dir->has_domain_runtime_facts = hir->has_domain_runtime_facts;
    if (!hir->has_domain_runtime_facts)
        return true;

    if (hir->domain_participant_role_fact_count != 0) {
        dir->domain_participant_role_facts = calloc(
            hir->domain_participant_role_fact_count,
            sizeof(*dir->domain_participant_role_facts));
        if (dir->domain_participant_role_facts == NULL)
            goto out_of_memory;
        dir->domain_participant_role_fact_count =
            hir->domain_participant_role_fact_count;
        for (size_t i = 0;
             i < hir->domain_participant_role_fact_count; i++) {
            const PgyDomainParticipantRoleFact *source =
                &hir->domain_participant_role_facts[i];
            PgyDomainParticipantRoleFact *copy =
                &dir->domain_participant_role_facts[i];
            *copy = *source;
            copy->owner_name = dir_domain_runtime_strdup(source->owner_name);
            copy->field_name = dir_domain_runtime_strdup(source->field_name);
            copy->field_type_name = dir_domain_runtime_strdup(
                source->field_type_name);
            if (copy->owner_name == NULL || copy->field_name == NULL
                || copy->field_type_name == NULL) {
                goto out_of_memory;
            }
        }
    }

    if (hir->domain_projection_member_assignment_fact_count != 0) {
        dir->domain_projection_member_assignment_facts = calloc(
            hir->domain_projection_member_assignment_fact_count,
            sizeof(*dir->domain_projection_member_assignment_facts));
        if (dir->domain_projection_member_assignment_facts == NULL)
            goto out_of_memory;
        dir->domain_projection_member_assignment_fact_count =
            hir->domain_projection_member_assignment_fact_count;
        for (size_t i = 0;
             i < hir->domain_projection_member_assignment_fact_count; i++) {
            const PgyDomainProjectionMemberAssignmentFact *source =
                &hir->domain_projection_member_assignment_facts[i];
            PgyDomainProjectionMemberAssignmentFact *copy =
                &dir->domain_projection_member_assignment_facts[i];
            *copy = *source;
            copy->owner_name = dir_domain_runtime_strdup(source->owner_name);
            copy->projection_slot_name = dir_domain_runtime_strdup(
                source->projection_slot_name);
            copy->source_slot_name = dir_domain_runtime_strdup(
                source->source_slot_name);
            copy->target_field_name = dir_domain_runtime_strdup(
                source->target_field_name);
            copy->target_field_type_name = dir_domain_runtime_strdup(
                source->target_field_type_name);
            copy->source_path = dir_domain_runtime_strdup(source->source_path);
            copy->source_leaf_type_name = dir_domain_runtime_strdup(
                source->source_leaf_type_name);
            copy->source_path_segments = calloc(
                source->source_path_segment_count,
                sizeof(*copy->source_path_segments));
            if (copy->owner_name == NULL
                || copy->projection_slot_name == NULL
                || copy->source_slot_name == NULL
                || copy->target_field_name == NULL
                || copy->target_field_type_name == NULL
                || copy->source_path == NULL
                || copy->source_leaf_type_name == NULL
                || copy->source_path_segments == NULL) {
                goto out_of_memory;
            }
            for (size_t s = 0; s < source->source_path_segment_count; s++) {
                copy->source_path_segments[s] =
                    source->source_path_segments[s];
                copy->source_path_segments[s].field_name =
                    dir_domain_runtime_strdup(
                        source->source_path_segments[s].field_name);
                copy->source_path_segments[s].field_type_name =
                    dir_domain_runtime_strdup(
                        source->source_path_segments[s].field_type_name);
                if (copy->source_path_segments[s].field_name == NULL
                    || copy->source_path_segments[s].field_type_name == NULL) {
                    goto out_of_memory;
                }
            }
        }
    }
    return true;

out_of_memory:
    if (error_message != NULL && *error_message == NULL)
        *error_message = pergyra_strdup(
            "Out of memory while copying HIR domain runtime facts into DIR");
    return false;
}

static DIRProgram *
dir_lower_with_resource_flow_facts_internal(
    ASTNode *annotated_ast,
    const PgyResourceFlowFact *facts,
    size_t fact_count,
    const HIRProgram *hir,
    char **error_message)
{
    DIRProgram *dir;

    if (error_message != NULL)
        *error_message = NULL;
    if (annotated_ast == NULL || annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("DIR lowering requires AST_PROGRAM root");
        return NULL;
    }
    if (fact_count != 0 && facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR lowering requires a ResourceFlowUniverse fact array when fact_count is nonzero");
        return NULL;
    }

    dir = calloc(1, sizeof(DIRProgram));
    if (dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }
    dir->source_program_syntax_id = annotated_ast->stable_id;
    if (dir->source_program_syntax_id == 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR lowering requires an anchored source program identity");
        dir_destroy(dir);
        return NULL;
    }
    if (hir != NULL
        && hir->source_program_syntax_id != dir->source_program_syntax_id) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR lowering rejected HIR domain facts from a different source program");
        dir_destroy(dir);
        return NULL;
    }
    if (hir != NULL
        && !dir_copy_hir_domain_runtime_facts(dir, hir, error_message)) {
        dir_destroy(dir);
        return NULL;
    }

    dir->has_resource_flow_facts = facts != NULL || fact_count != 0;
    if (fact_count != 0) {
        dir->resource_flow_facts = calloc(
            fact_count, sizeof(*dir->resource_flow_facts));
        if (dir->resource_flow_facts == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("Out of memory");
            dir_destroy(dir);
            return NULL;
        }
        dir->resource_flow_fact_count = fact_count;
        for (size_t i = 0; i < fact_count; i++) {
            dir->resource_flow_facts[i] = facts[i];
            dir->resource_flow_facts[i].name = NULL;
            if (facts[i].name != NULL) {
                dir->resource_flow_facts[i].name =
                    pergyra_strdup(facts[i].name);
                if (dir->resource_flow_facts[i].name == NULL) {
                    if (error_message != NULL)
                        *error_message = pergyra_strdup("Out of memory");
                    dir_destroy(dir);
                    return NULL;
                }
            }
        }
    }

    bool collected = hir != NULL
        ? dir_collect_nodes_from_hir(dir, hir)
        : dir_collect_nodes(dir, annotated_ast);
    bool edges_collected = hir != NULL
        ? dir_collect_edges_and_intents_from_hir(dir, annotated_ast, hir)
        : dir_collect_edges_and_intents(dir, annotated_ast);
    if (!collected || !edges_collected) {
        if (error_message != NULL) {
            if (dir->error_message != NULL) {
                *error_message = dir->error_message;
                dir->error_message = NULL;
            } else {
                *error_message = pergyra_strdup("Out of memory");
            }
        }
        dir_clear_error_message(dir);
        dir_destroy(dir);
        return NULL;
    }

    dir->domain_graph_id = dir_domain_graph_anchor(
        dir->source_program_syntax_id, dir->node_count, dir->edge_count);

    dir_clear_error_message(dir);
    return dir;
}

DIRProgram *
dir_lower_with_resource_flow_facts(ASTNode *annotated_ast,
                                   const PgyResourceFlowFact *facts,
                                   size_t fact_count,
                                   char **error_message)
{
    return dir_lower_with_resource_flow_facts_internal(
        annotated_ast, facts, fact_count, NULL, error_message);
}

DIRProgram *
dir_lower_with_hir_resource_flow_facts(ASTNode *annotated_ast,
                                       const HIRProgram *hir,
                                       char **error_message)
{
    PgyResourceFlowFact *facts = NULL;
    size_t fact_count = 0;
    size_t fact_index = 0;
    DIRProgram *dir;

    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR lowering requires the HIR-owned ResourceFlowUniverse snapshot");
        return NULL;
    }
    for (size_t i = 0; i < hir->routine_count; i++)
        fact_count += hir->routines[i].resource_flow_symbol_count;
    if (fact_count != 0) {
        facts = calloc(fact_count, sizeof(*facts));
        if (facts == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("Out of memory");
            return NULL;
        }
        for (size_t i = 0; i < hir->routine_count; i++) {
            const HIRRoutine *routine = &hir->routines[i];
            for (size_t j = 0;
                 j < routine->resource_flow_symbol_count;
                 j++) {
                const HIRResourceFlowSymbol *symbol =
                    &routine->resource_flow_symbols[j];
                facts[fact_index].function_syntax_id =
                    routine->source_syntax_id;
                facts[fact_index].stable_index = symbol->stable_index;
                facts[fact_index].declaration_syntax_id =
                    symbol->declaration_syntax_id;
                facts[fact_index].line = symbol->line;
                facts[fact_index].column = symbol->column;
                facts[fact_index].symbol_kind = symbol->symbol_kind;
                facts[fact_index].is_parameter = symbol->is_parameter;
                facts[fact_index].parameter_index = symbol->parameter_index;
                facts[fact_index].name = (char *)symbol->name;
                fact_index++;
            }
        }
    }
    dir = dir_lower_with_resource_flow_facts_internal(
        annotated_ast, facts, fact_count, hir, error_message);
    free(facts);
    return dir;
}

DIRProgram *
dir_lower(ASTNode *annotated_ast, char **error_message)
{
    return dir_lower_with_resource_flow_facts(
        annotated_ast, NULL, 0, error_message);
}

void
dir_destroy(DIRProgram *dir)
{
    if (dir == NULL)
        return;
    if (dir->intents != NULL) {
        for (size_t i = 0; i < dir->intent_count; i++) {
            free(dir->intents[i].participants);
            for (size_t j = 0; j < dir->intents[i].step_count; j++) {
                free((void *)dir->intents[i].steps[j].who_names);
                free((void *)dir->intents[i].steps[j].required_abilities);
                free((void *)dir->intents[i].steps[j].authorized_by);
            }
            free(dir->intents[i].steps);
        }
    }
    if (dir->owned_names != NULL) {
        for (size_t i = 0; i < dir->owned_name_count; i++)
            free(dir->owned_names[i]);
    }
    free(dir->nodes);
    free(dir->edges);
    free(dir->domain_topology_rows);
    free(dir->intents);
    pgy_resource_flow_facts_destroy(
        dir->resource_flow_facts, dir->resource_flow_fact_count);
    pgy_domain_participant_role_facts_destroy(
        dir->domain_participant_role_facts,
        dir->domain_participant_role_fact_count);
    pgy_domain_projection_member_assignment_facts_destroy(
        dir->domain_projection_member_assignment_facts,
        dir->domain_projection_member_assignment_fact_count);
    free(dir->owned_names);
    free(dir->error_message);
    free(dir);
}
