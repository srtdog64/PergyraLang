#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

#define type_name rir_type_name
#define expr_name rir_expr_name
#define call_name rir_call_name
#define rollback_policy_name rir_rollback_policy_name

static ASTNode *
find_top_level_zone_named(const char *zone_name)
{
    if (g_rir_program_root == NULL
        || g_rir_program_root->type != AST_PROGRAM
        || zone_name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < g_rir_program_root->data.program.count; i++) {
        ASTNode *node = g_rir_program_root->data.program.statements[i];
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
unique_effect_slot_for_type(const char *zone_name, const char *effect_type_name)
{
    ASTNode *zone = find_top_level_zone_named(zone_name);
    const char *slot_name = NULL;
    if (zone == NULL || effect_type_name == NULL)
        return NULL;
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot == NULL
            || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.is_relation
            || slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(slot->data.zone_layer_slot.layer_type, effect_type_name) != 0) {
            continue;
        }
        if (slot_name != NULL)
            return NULL;
        slot_name = slot->data.zone_layer_slot.slot_name;
    }
    return slot_name;
}

bool
rir_collect_intent_scope(RIRProgram *rir, ASTNode *node)
{
    RIRScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.id = rir->scope_count;
    scope.kind = RIR_SCOPE_INTENT;
    scope.name = node->data.intent_decl.name;
    scope.ast = node;

    if (!add_intent_policy_fact(&scope,
                                "concurrency",
                                node->data.intent_decl.is_concurrent ? "concurrent" : "exclusive",
                                node))
        goto oom;
    if (!add_intent_policy_fact(&scope,
                                "rollback",
                                rollback_policy_name(node->data.intent_decl.rollback_policy),
                                node))
        goto oom;

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        if (!add_param_resource_fact(&scope,
                                     involves->data.intent_involves.alias,
                                     involves->data.intent_involves.subject_type,
                                     involves))
            goto oom;
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        if (step->data.intent_step.using_expr != NULL) {
            if (!add_op(&scope,
                        RIR_OP_READ,
                        expr_name(step->data.intent_step.using_expr),
                        step->data.intent_step.name,
                        step->data.intent_step.where_type != NULL
                            ? type_name(step->data.intent_step.where_type) : NULL,
                        step))
                goto oom;
        }
        if (step->data.intent_step.transfer_from_alias != NULL) {
            if (!add_op(&scope,
                        RIR_OP_MOVE,
                        step->data.intent_step.transfer_from_alias,
                        step->data.intent_step.transfer_to_alias,
                        step->data.intent_step.name,
                        step))
                goto oom;
        }
        if (step->data.intent_step.transfer_to_alias != NULL) {
            if (!add_op(&scope,
                        RIR_OP_CLAIM,
                        step->data.intent_step.transfer_to_alias,
                        step->data.intent_step.transfer_from_alias,
                        step->data.intent_step.name,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.required_ability_count; j++) {
            ASTNode *ability_ref = step->data.intent_step.required_abilities[j];
            const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                ? ability_ref->data.type.name : NULL;
            if (ability_name == NULL)
                continue;
            if (!add_authority_fact(&scope, step->data.intent_step.name,
                                    ability_name,
                                    step))
                goto oom;
        }
        if (step->data.intent_step.causes_effect != NULL) {
            const char *where_name = step->data.intent_step.where_type != NULL
                ? type_name(step->data.intent_step.where_type) : NULL;
            const char *effect_slot_name =
                unique_effect_slot_for_type(where_name, step->data.intent_step.causes_effect);
            const char *effect_anchor = effect_slot_name != NULL
                ? effect_slot_name : step->data.intent_step.causes_effect;
            if (!add_named_resource_fact(&scope,
                                         effect_anchor,
                                         step->data.intent_step.causes_effect,
                                         RIR_RESOURCE_EFFECT_INSTANCE,
                                         RIR_STATE_DETACHED,
                                         step))
                goto oom;
            if (!add_op(&scope,
                        RIR_OP_ATTACH_EFFECT,
                        effect_anchor,
                        step->data.intent_step.name,
                        where_name,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.authorized_by_count; j++) {
            if (!add_op(&scope, RIR_OP_AUTHORIZE,
                        step->data.intent_step.authorized_by[j],
                        step->data.intent_step.name,
                        step->data.intent_step.where_type != NULL
                            ? type_name(step->data.intent_step.where_type) : NULL,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (!rir_walk_node(&scope, step->data.intent_step.on_exprs[j]))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            const char *comp_name = expr_name(step->data.intent_step.compensate_exprs[j]);
            if (comp_name == NULL
                && step->data.intent_step.compensate_exprs[j] != NULL
                && step->data.intent_step.compensate_exprs[j]->type == AST_CALL) {
                comp_name = call_name(step->data.intent_step.compensate_exprs[j]);
            }
            if (!add_op(&scope, RIR_OP_COMPENSATE_INTENT_STEP,
                        step->data.intent_step.name,
                        comp_name,
                        NULL,
                        step->data.intent_step.compensate_exprs[j]))
                goto oom;
            if (!rir_walk_node(&scope, step->data.intent_step.compensate_exprs[j]))
                goto oom;
        }
    }

    if (!add_op(&scope, RIR_OP_ABORT_INTENT,
                node->data.intent_decl.name,
                rollback_policy_name(node->data.intent_decl.rollback_policy),
                NULL,
                node))
        goto oom;
    if (!add_op(&scope, RIR_OP_COMMIT_INTENT, node->data.intent_decl.name, NULL, NULL, node))
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
