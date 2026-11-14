#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
tc_zone_tail_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void
semantic_type_resolution_precollect_zone_state_authority_inventory(
    ASTNode *zone_decl,
    SemanticContext *ctx)
{
    ASTNode **states;
    ASTNode **maintained_states;
    ASTNode **authorities;
    ASTNode **methods;
    size_t state_count;
    size_t maintained_state_count;
    size_t authority_count;
    size_t method_count;

    states = ast_zone_states(zone_decl, &state_count);
    maintained_states = ast_zone_maintained_states(zone_decl,
                                                   &maintained_state_count);
    authorities = ast_zone_authorities(zone_decl, &authority_count);
    methods = ast_zone_methods(zone_decl, &method_count);

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        char *state_label;

        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;

        state_label = semantic_type_resolution_zone_state_label(
            zone_decl,
            ast_zone_state_name(state));
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        if (ast_zone_state_layer_slot_name(state) != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                ast_zone_state_layer_slot_name(state));
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    layer_label,
                    "zone state layer lookup");
                free(layer_label);
            }
        }

        if (ast_zone_state_left_or_target_slot_name(state) != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                ast_zone_state_left_or_target_slot_name(state));
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    target_label,
                    "zone state target-slot lookup");
                free(target_label);
            }
        }

        if (ast_zone_state_is_relation(state)
            && ast_zone_state_right_slot_name(state) != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                ast_zone_state_right_slot_name(state));
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    right_label,
                    "zone state right-slot lookup");
                free(right_label);
            }
        }
        free(state_label);
    }

    for (size_t i = 0; i < maintained_state_count; i++) {
        ASTNode *maintain = maintained_states[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;

        consumer_label = tc_zone_tail_strdup_fmt(
            "zone %s.maintain-state.%s",
            ast_zone_name(zone_decl) != NULL
                ? ast_zone_name(zone_decl) : "<zone>",
            ast_zone_directive_state_name(maintain) != NULL
                ? ast_zone_directive_state_name(maintain)
                : "<state>");
        if (consumer_label == NULL)
            continue;

        if (ast_zone_directive_state_name(maintain) != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                ast_zone_directive_state_name(maintain));
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone maintain-state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *authority = authorities[i];
        const char *subject_slot =
            ast_zone_authority_subject_slot_name(authority);
        char *consumer_name;

        if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
            continue;

        consumer_name = tc_zone_tail_strdup_fmt(
            "zone %s.%s",
            ast_zone_name(zone_decl) != NULL
                ? ast_zone_name(zone_decl) : "<zone>",
            subject_slot != NULL ? subject_slot : "<authority>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            ast_zone_authority_required_abilities(authority, NULL),
            ast_zone_authority_ability_count(authority),
            ctx,
            authority,
            consumer_name,
            "zone authority ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            ast_zone_name(zone_decl));
    }
}
