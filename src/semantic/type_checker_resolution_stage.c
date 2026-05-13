#include "type_checker_internal.h"

void
semantic_stage_top_level_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode *saved_nominal;
    ASTNode *saved_relation;
    ASTNode *saved_effect;
    ASTNode *saved_party;
    ASTNode *saved_roster;
    ASTNode *saved_zone;
    ASTNode *saved_world;

    if (decl == NULL || ctx == NULL)
        return;

    saved_nominal = ctx->current_nominal_decl;
    saved_relation = ctx->current_relation;
    saved_effect = ctx->current_effect;
    saved_party = ctx->current_party;
    saved_roster = ctx->current_roster;
    saved_zone = ctx->current_zone;
    saved_world = ctx->current_world;

    switch (decl->type) {
    case AST_TYPE_ALIAS:
        semantic_stage_type_alias_decl(decl, ctx);
        break;

    case AST_CLASS_DECL:
        ctx->current_nominal_decl = decl;
        semantic_stage_class_decl(decl, ctx);
        break;

    case AST_FUNC_DECL:
        semantic_stage_function_signature(decl, ctx, decl->data.func_decl.name);
        break;

    case AST_EVENT_DECL:
        semantic_stage_event_signature(decl, ctx);
        break;

    case AST_ENUM_DECL:
        semantic_stage_enum_decl(decl, ctx);
        break;

    case AST_ABILITY_DECL:
        semantic_stage_ability_decl(decl, ctx);
        break;

    case AST_ROLE_DECL:
        semantic_stage_role_decl(decl, ctx);
        break;

    case AST_PARTY_DECL:
        ctx->current_party = decl;
        semantic_stage_party_decl(decl, ctx);
        break;

    case AST_ROSTER_DECL:
        ctx->current_roster = decl;
        semantic_stage_roster_decl(decl, ctx);
        break;

    case AST_WORLD_DECL:
        semantic_stage_world_decl(decl, ctx);
        break;

    case AST_INTENT_DECL:
        semantic_stage_intent_decl(decl, ctx);
        break;

    case AST_RELATION_DECL:
        semantic_stage_relation_decl(decl, ctx);
        break;

    case AST_EFFECT_DECL:
        semantic_stage_effect_decl(decl, ctx);
        break;

    case AST_ZONE_DECL:
        semantic_stage_zone_decl(decl, ctx);
        break;

    default:
        break;
    }

    ctx->current_nominal_decl = saved_nominal;
    ctx->current_relation = saved_relation;
    ctx->current_effect = saved_effect;
    ctx->current_party = saved_party;
    ctx->current_roster = saved_roster;
    ctx->current_zone = saved_zone;
    ctx->current_world = saved_world;
}
