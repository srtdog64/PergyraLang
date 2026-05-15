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
    scope.name = ast_declaration_name(func);
    scope.ast = func;
    for (size_t i = 0; i < ast_func_param_count(func); i++) {
        FuncParam *param = ast_func_param(func, i);
        if (param == NULL)
            continue;
        if (!add_param_resource_fact(&scope, param->name, param->type, func))
            goto oom;
    }
    if (!rir_walk_node(&scope, ast_func_body(func))) {
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
            const char *subject_slot =
                ast_zone_authority_subject_slot_name(auth);
            if (!add_authority_fact(&scope, subject_slot, NULL, auth))
                goto oom;
            for (size_t j = 0; j < ast_zone_authority_ability_count(auth); j++) {
                ASTNode *ability_ref = ast_zone_authority_required_ability(auth, j);
                const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                    ? ast_type_name(ability_ref) : NULL;
                if (ability_name == NULL)
                    continue;
                if (!add_authority_fact(&scope,
                                        subject_slot,
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
                        ast_zone_effect_slot_name(apply),
                        ast_zone_effect_target_slot_name(apply),
                        ast_zone_directive_participant_slot_name(apply),
                        apply))
                goto oom;
        }
        size_t link_count = 0;
        ASTNode **links = ast_zone_links(node, &link_count);
        for (size_t i = 0; i < link_count; i++) {
            ASTNode *link = links[i];
            if (!add_op(&scope, RIR_OP_LINK_RELATION,
                        ast_zone_relation_slot_name(link),
                        ast_zone_relation_left_slot_name(link),
                        ast_zone_relation_right_slot_name(link),
                        link))
                goto oom;
        }
        size_t detach_count = 0;
        ASTNode **detaches = ast_zone_detaches(node, &detach_count);
        for (size_t i = 0; i < detach_count; i++) {
            ASTNode *detach = detaches[i];
            if (!add_op(&scope, RIR_OP_DETACH_EFFECT,
                        ast_zone_effect_slot_name(detach),
                        ast_zone_effect_target_slot_name(detach),
                        ast_zone_directive_participant_slot_name(detach),
                        detach))
                goto oom;
        }
        size_t unlink_count = 0;
        ASTNode **unlinks = ast_zone_unlinks(node, &unlink_count);
        for (size_t i = 0; i < unlink_count; i++) {
            ASTNode *unlink = unlinks[i];
            if (!add_op(&scope, RIR_OP_UNLINK_RELATION,
                        ast_zone_relation_slot_name(unlink),
                        ast_zone_relation_left_slot_name(unlink),
                        ast_zone_relation_right_slot_name(unlink),
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
            node, ast_zone_refresh_object_slot_name(refresh));
        bool tobject =
            ast_zone_refresh_requires_dto(refresh)
            || (ast_zone_refresh_derives_target_kind(refresh)
                && target_slot != NULL
                && ast_domain_slot_is_tobject(target_slot));

        if (!add_projection_fact(&scope,
                                 ast_zone_refresh_object_slot_name(refresh),
                                 ast_zone_refresh_source_slot_name(refresh),
                                 tobject ? "publish" : (ast_zone_refresh_derives_target_kind(refresh) ? "bind" : "refresh"),
                                 tobject ? RIR_STATE_PUBLISHED
                                     : (ast_zone_refresh_derives_target_kind(refresh) ? RIR_STATE_DIRTY : RIR_STATE_SYNCED),
                                 tobject ? RIR_RESOURCE_PROJECTION_TOBJECT : RIR_RESOURCE_PROJECTION_OBJECT,
                                 refresh))
            goto oom;
        if (!add_op(&scope,
                    tobject ? RIR_OP_PROJECT_PUBLISH : RIR_OP_PROJECT_REFRESH,
                    ast_zone_refresh_object_slot_name(refresh),
                    ast_zone_refresh_source_slot_name(refresh),
                    ast_zone_refresh_participant_slot_name(refresh),
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

    for (size_t i = 0; i < ast_program_statement_count(annotated_ast); i++) {
        ASTNode *node = ast_program_statement(annotated_ast, i);
        bool ok = true;
        switch (node->type) {
            case AST_FUNC_DECL:
                ok = rir_collect_func_scope(rir,
                                            ast_func_is_action(node) ? RIR_SCOPE_METHOD : RIR_SCOPE_FUNCTION,
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
