#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

#define type_name rir_type_name
#define expr_name rir_expr_name
#define call_name rir_call_name
#define rollback_policy_name rir_rollback_policy_name

static ASTNode *
find_top_level_zone_named(ASTNode *program, const char *zone_name)
{
    if (program == NULL
        || program->type != AST_PROGRAM
        || zone_name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *node = ast_program_statement(program, i);
        if (node != NULL
            && node->type == AST_ZONE_DECL
            && ast_zone_name(node) != NULL
            && strcmp(ast_zone_name(node), zone_name) == 0) {
            return node;
        }
    }
    return NULL;
}

static const char *
unique_effect_slot_for_type(ASTNode *program_root,
                            const char *zone_name,
                            const char *effect_type_name)
{
    ASTNode *zone = find_top_level_zone_named(program_root, zone_name);
    const char *slot_name = NULL;
    if (zone == NULL || effect_type_name == NULL)
        return NULL;
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot == NULL
            || slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_is_relation(slot)
            || ast_zone_layer_slot_layer_type(slot) == NULL
            || strcmp(ast_zone_layer_slot_layer_type(slot), effect_type_name) != 0) {
            continue;
        }
        if (slot_name != NULL)
            return NULL;
        slot_name = ast_zone_layer_slot_name(slot);
    }
    return slot_name;
}

bool
rir_collect_intent_scope(RIRProgram *rir, ASTNode *node)
{
    RIRScope scope;
    ASTNode **involves_nodes;
    ASTNode **steps;
    size_t involve_count;
    size_t step_count;
    const char *intent_name;
    IntentRollbackPolicy rollback_policy;
    memset(&scope, 0, sizeof(scope));
    intent_name = ast_intent_decl_name(node);
    rollback_policy = ast_intent_decl_rollback_policy(node);
    involves_nodes = ast_intent_decl_involves(node, &involve_count);
    steps = ast_intent_decl_steps(node, &step_count);
    scope.id = rir->scope_count;
    scope.kind = RIR_SCOPE_INTENT;
    scope.name = intent_name;
    scope.ast = node;
    scope.program_root = rir->program_root;

    if (!add_intent_policy_fact(&scope,
                                "concurrency",
                                ast_intent_decl_is_concurrent(node) ? "concurrent" : "exclusive",
                                node))
        goto oom;
    if (!add_intent_policy_fact(&scope,
                                "rollback",
                                rollback_policy_name(rollback_policy),
                                node))
        goto oom;

    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        if (!add_param_resource_fact(&scope,
                                     ast_intent_involves_alias(involves),
                                     ast_intent_involves_subject_type(involves),
                                     involves))
            goto oom;
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        if (ast_intent_step_using_expr(step) != NULL) {
            if (!add_op(&scope,
                        RIR_OP_READ,
                        expr_name(ast_intent_step_using_expr(step)),
                        ast_intent_step_name(step),
                        ast_intent_step_where_type(step) != NULL
                            ? type_name(ast_intent_step_where_type(step)) : NULL,
                        step))
                goto oom;
        }
        if (ast_intent_step_transfer_from_alias(step) != NULL) {
            if (!add_op(&scope,
                        RIR_OP_MOVE,
                        ast_intent_step_transfer_from_alias(step),
                        ast_intent_step_transfer_to_alias(step),
                        ast_intent_step_name(step),
                        step))
                goto oom;
        }
        if (ast_intent_step_transfer_to_alias(step) != NULL) {
            if (!add_op(&scope,
                        RIR_OP_CLAIM,
                        ast_intent_step_transfer_to_alias(step),
                        ast_intent_step_transfer_from_alias(step),
                        ast_intent_step_name(step),
                        step))
                goto oom;
        }
        for (size_t j = 0; j < ast_intent_step_required_ability_count(step); j++) {
            ASTNode *ability_ref = ast_intent_step_required_abilities(step, NULL)[j];
            const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                ? ast_type_name(ability_ref) : NULL;
            if (ability_name == NULL)
                continue;
            if (!add_authority_fact(&scope, ast_intent_step_name(step),
                                    ability_name,
                                    step))
                goto oom;
        }
        if (ast_intent_step_causes_effect(step) != NULL) {
            const char *where_name = ast_intent_step_where_type(step) != NULL
                ? type_name(ast_intent_step_where_type(step)) : NULL;
            const char *effect_slot_name =
                unique_effect_slot_for_type(rir->program_root,
                                            where_name,
                                            ast_intent_step_causes_effect(step));
            const char *effect_anchor = effect_slot_name != NULL
                ? effect_slot_name : ast_intent_step_causes_effect(step);
            if (!add_named_resource_fact(&scope,
                                         effect_anchor,
                                         ast_intent_step_causes_effect(step),
                                         RIR_RESOURCE_EFFECT_INSTANCE,
                                         RIR_STATE_DETACHED,
                                         step))
                goto oom;
            if (!add_op(&scope,
                        RIR_OP_ATTACH_EFFECT,
                        effect_anchor,
                        ast_intent_step_name(step),
                        where_name,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < ast_intent_step_authorized_by_count(step); j++) {
            if (!add_op(&scope, RIR_OP_AUTHORIZE,
                        ast_intent_step_authorized_by(step, NULL)[j],
                        ast_intent_step_name(step),
                        ast_intent_step_where_type(step) != NULL
                            ? type_name(ast_intent_step_where_type(step)) : NULL,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < ast_intent_step_on_expr_count(step); j++) {
            if (!rir_walk_node(&scope, ast_intent_step_on_exprs(step, NULL)[j]))
                goto oom;
        }
        for (size_t j = 0; j < ast_intent_step_compensate_expr_count(step); j++) {
            const char *comp_name = expr_name(ast_intent_step_compensate_exprs(step, NULL)[j]);
            if (comp_name == NULL
                && ast_intent_step_compensate_exprs(step, NULL)[j] != NULL
                && ast_intent_step_compensate_exprs(step, NULL)[j]->type == AST_CALL) {
                comp_name = call_name(ast_intent_step_compensate_exprs(step, NULL)[j]);
            }
            if (!add_op(&scope, RIR_OP_COMPENSATE_INTENT_STEP,
                        ast_intent_step_name(step),
                        comp_name,
                        NULL,
                        ast_intent_step_compensate_exprs(step, NULL)[j]))
                goto oom;
            if (!rir_walk_node(&scope, ast_intent_step_compensate_exprs(step, NULL)[j]))
                goto oom;
        }
    }

    if (!add_op(&scope, RIR_OP_ABORT_INTENT,
                intent_name,
                rollback_policy_name(rollback_policy),
                NULL,
                node))
        goto oom;
    if (!add_op(&scope, RIR_OP_COMMIT_INTENT, intent_name, NULL, NULL, node))
        goto oom;

    if (!rir_normalize_scope_shared(&scope))
        goto oom;

    return append_scope(rir, scope);

oom:
    free(scope.facts);
    free(scope.ops);
    free(scope.state_summaries);
    return false;
}
