#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static bool
rir_collect_func_scope(RIRProgram *rir,
                       RIRScopeKind kind,
                       const char *owner_name,
                       ASTNode *func)
{
    RIRScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.id = rir->scope_count;
    scope.kind = kind;
    scope.owner_name = owner_name;
    scope.name = func->data.func_decl.name;
    scope.ast = func;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *param = func->data.func_decl.params[i];
        if (param == NULL)
            continue;
        if (!add_param_resource_fact(&scope, param->name, param->type, func))
            goto oom;
    }
    if (!rir_walk_node(&scope, func->data.func_decl.body)) {
oom:
        free(scope.facts);
        free(scope.ops);
        free(scope.state_summaries);
        return false;
    }
    if (!rir_normalize_scope_shared(&scope)) {
        free(scope.facts);
        free(scope.ops);
        free(scope.state_summaries);
        return false;
    }
    return append_scope(rir, scope);
}

static bool
rir_collect_zone_like_scope(RIRProgram *rir, ASTNode *node, RIRScopeKind kind, const char *name)
{
    RIRScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.id = rir->scope_count;
    scope.kind = kind;
    scope.name = name;
    scope.ast = node;

    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (node->type == AST_ZONE_DECL) {
        size_t slot_count = 0;
        ASTNode **slots = ast_zone_slots(node, &slot_count);
        for (size_t i = 0; i < slot_count; i++) {
            if (!add_domain_slot_fact(&scope, slots[i]))
                goto oom;
        }
        size_t layer_slot_count = 0;
        ASTNode **layer_slots = ast_zone_layer_slots(node, &layer_slot_count);
        for (size_t i = 0; i < layer_slot_count; i++) {
            ASTNode *slot = layer_slots[i];
            if (!add_named_resource_fact(&scope,
                                         ast_zone_layer_slot_name(slot),
                                         ast_zone_layer_slot_layer_type(slot),
                                         ast_zone_layer_slot_is_relation(slot)
                                             ? RIR_RESOURCE_RELATION_INSTANCE
                                             : RIR_RESOURCE_EFFECT_INSTANCE,
                                         RIR_STATE_DETACHED,
                                         slot))
                goto oom;
        }
        size_t authority_count = 0;
        ASTNode **authorities = ast_zone_authorities(node, &authority_count);
        for (size_t i = 0; i < authority_count; i++) {
            ASTNode *auth = authorities[i];
            if (!add_authority_fact(&scope, auth->data.zone_authority.subject_slot_name, NULL, auth))
                goto oom;
            for (size_t j = 0; j < auth->data.zone_authority.ability_count; j++) {
                ASTNode *ability_ref = auth->data.zone_authority.required_abilities[j];
                const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                    ? ability_ref->data.type.name : NULL;
                if (ability_name == NULL)
                    continue;
                if (!add_authority_fact(&scope,
                                        auth->data.zone_authority.subject_slot_name,
                                        ability_name,
                                        auth))
                    goto oom;
            }
        }

        refreshes = ast_zone_refreshes(node, &refresh_count);
        methods = ast_zone_methods(node, &method_count);

        size_t apply_count = 0;
        ASTNode **applies = ast_zone_applies(node, &apply_count);
        for (size_t i = 0; i < apply_count; i++) {
            ASTNode *apply = applies[i];
            if (!add_op(&scope, RIR_OP_ATTACH_EFFECT,
                        apply->data.zone_apply.effect_slot_name,
                        apply->data.zone_apply.target_slot_name,
                        apply->data.zone_apply.participant_slot_name,
                        apply))
                goto oom;
        }
        size_t link_count = 0;
        ASTNode **links = ast_zone_links(node, &link_count);
        for (size_t i = 0; i < link_count; i++) {
            ASTNode *link = links[i];
            if (!add_op(&scope, RIR_OP_LINK_RELATION,
                        link->data.zone_link.relation_slot_name,
                        link->data.zone_link.left_slot_name,
                        link->data.zone_link.right_slot_name,
                        link))
                goto oom;
        }
        size_t detach_count = 0;
        ASTNode **detaches = ast_zone_detaches(node, &detach_count);
        for (size_t i = 0; i < detach_count; i++) {
            ASTNode *detach = detaches[i];
            if (!add_op(&scope, RIR_OP_DETACH_EFFECT,
                        detach->data.zone_detach.effect_slot_name,
                        detach->data.zone_detach.target_slot_name,
                        detach->data.zone_detach.participant_slot_name,
                        detach))
                goto oom;
        }
        size_t unlink_count = 0;
        ASTNode **unlinks = ast_zone_unlinks(node, &unlink_count);
        for (size_t i = 0; i < unlink_count; i++) {
            ASTNode *unlink = unlinks[i];
            if (!add_op(&scope, RIR_OP_UNLINK_RELATION,
                        unlink->data.zone_unlink.relation_slot_name,
                        unlink->data.zone_unlink.left_slot_name,
                        unlink->data.zone_unlink.right_slot_name,
                        unlink))
                goto oom;
        }
    } else if (node->type == AST_RELATION_DECL) {
        size_t slot_count = 0;
        ASTNode **slots = ast_relation_slots(node, &slot_count);
        for (size_t i = 0; i < slot_count; i++) {
            if (!add_domain_slot_fact(&scope, slots[i]))
                goto oom;
        }
        refreshes = ast_relation_refreshes(node, &refresh_count);
        methods = ast_relation_methods(node, &method_count);
    } else if (node->type == AST_EFFECT_DECL) {
        size_t slot_count = 0;
        ASTNode **slots = ast_effect_slots(node, &slot_count);
        for (size_t i = 0; i < slot_count; i++) {
            if (!add_domain_slot_fact(&scope, slots[i]))
                goto oom;
        }
        refreshes = ast_effect_refreshes(node, &refresh_count);
        methods = ast_effect_methods(node, &method_count);
    } else if (node->type == AST_WORLD_DECL) {
        size_t zone_count = 0;
        ASTNode **zones = ast_world_zones(node, &zone_count);
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *slot = zones[i];
            if (!add_named_resource_fact(&scope,
                                         ast_world_zone_slot_name(slot),
                                         ast_world_zone_type_name(slot),
                                         RIR_RESOURCE_ZONE_HANDLE,
                                         RIR_STATE_OWNED,
                                         slot))
                goto oom;
        }
        methods = ast_world_methods(node, &method_count);
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        ASTNode *target_slot = rir_find_domain_slot_in_owner(
            node, refresh->data.zone_refresh.object_slot_name);
        bool tobject =
            refresh->data.zone_refresh.requires_dto
            || (refresh->data.zone_refresh.derive_target_kind
                && target_slot != NULL
                && ast_domain_slot_is_tobject(target_slot));

        if (!add_projection_fact(&scope,
                                 refresh->data.zone_refresh.object_slot_name,
                                 refresh->data.zone_refresh.source_slot_name,
                                 tobject ? "publish" : (refresh->data.zone_refresh.derive_target_kind ? "bind" : "refresh"),
                                 tobject ? RIR_STATE_PUBLISHED
                                     : (refresh->data.zone_refresh.derive_target_kind ? RIR_STATE_DIRTY : RIR_STATE_SYNCED),
                                 tobject ? RIR_RESOURCE_PROJECTION_TOBJECT : RIR_RESOURCE_PROJECTION_OBJECT,
                                 refresh))
            goto oom;
        if (!add_op(&scope,
                    tobject ? RIR_OP_PROJECT_PUBLISH : RIR_OP_PROJECT_REFRESH,
                    refresh->data.zone_refresh.object_slot_name,
                    refresh->data.zone_refresh.source_slot_name,
                    refresh->data.zone_refresh.participant_slot_name,
                    refresh))
            goto oom;
    }

    if (!rir_normalize_scope_shared(&scope))
        goto oom;

    if (!append_scope(rir, scope))
        goto oom_no_scope;

    for (size_t i = 0; i < method_count; i++) {
        if (!rir_collect_func_scope(rir, RIR_SCOPE_METHOD, name, methods[i]))
            return false;
    }
    return true;

oom:
    free(scope.facts);
    free(scope.ops);
    free(scope.state_summaries);
oom_no_scope:
    return false;
}

RIRProgram *
rir_lower(ASTNode *annotated_ast, char **error_message)
{
    RIRProgram *rir;
    if (error_message != NULL)
        *error_message = NULL;
    if (annotated_ast == NULL || annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR lowering requires AST_PROGRAM");
        return NULL;
    }

    g_rir_program_root = annotated_ast;
    rir = calloc(1, sizeof(RIRProgram));
    if (rir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }

    for (size_t i = 0; i < annotated_ast->data.program.count; i++) {
        ASTNode *node = annotated_ast->data.program.statements[i];
        bool ok = true;
        switch (node->type) {
            case AST_FUNC_DECL:
                ok = rir_collect_func_scope(rir,
                                            node->data.func_decl.is_action ? RIR_SCOPE_METHOD : RIR_SCOPE_FUNCTION,
                                            NULL,
                                            node);
                break;
            case AST_CLASS_DECL:
            {
                size_t method_count = 0;
                ASTNode **methods = ast_class_methods(node, &method_count);
                for (size_t j = 0; ok && j < method_count; j++) {
                    ok = rir_collect_func_scope(rir, RIR_SCOPE_METHOD,
                                                ast_class_name(node),
                                                methods != NULL ? methods[j] : NULL);
                }
                break;
            }
            case AST_ZONE_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_ZONE, ast_zone_name(node));
                break;
            case AST_RELATION_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_RELATION, ast_relation_name(node));
                break;
            case AST_EFFECT_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_EFFECT, ast_effect_name(node));
                break;
            case AST_WORLD_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_WORLD, ast_world_name(node));
                break;
            case AST_INTENT_DECL:
                ok = rir_collect_intent_scope(rir, node);
                break;
            default:
                break;
        }
        if (!ok) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            rir_destroy(rir);
            return NULL;
        }
    }

    return rir;
}
