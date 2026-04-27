#include "type_checker_internal.h"

void
semantic_type_resolution_precollect_program(ASTNode *program,
                                            SemanticContext *ctx)
{
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL)
            continue;

        semantic_type_resolution_register_top_level_decl(stmt, ctx);

        switch (stmt->type) {
        case AST_TYPE_ALIAS:
            if (stmt->data.type_alias.name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    stmt->data.type_alias.target_type,
                    ctx,
                    stmt,
                    stmt->data.type_alias.name,
                    "type-alias target lookup");
            }
            break;

        case AST_CLASS_DECL:
            semantic_type_resolution_precollect_class_inventory(stmt, ctx);
            break;

        case AST_FUNC_DECL:
            semantic_type_resolution_precollect_action_contract(
                stmt,
                ctx,
                stmt->data.func_decl.name);
            break;

        case AST_LET_DECL:
            semantic_type_resolution_collect_type_refs(
                stmt->data.let_decl.type,
                ctx,
                stmt,
                stmt->data.let_decl.name != NULL
                    ? stmt->data.let_decl.name : "<top-level-let>",
                "top-level let annotation lookup");
            break;

        case AST_EVENT_DECL:
            semantic_type_resolution_precollect_event_inventory(stmt, ctx);
            break;

        case AST_ENUM_DECL:
            semantic_type_resolution_precollect_enum_inventory(stmt, ctx);
            break;

        case AST_ABILITY_DECL:
            semantic_type_resolution_precollect_ability_inventory(stmt, ctx);
            break;

        case AST_ROLE_DECL:
            semantic_type_resolution_precollect_role_inventory(stmt, ctx);
            break;

        case AST_PARTY_DECL:
            semantic_type_resolution_precollect_party_inventory(stmt, ctx);
            break;

        case AST_ROSTER_DECL:
            semantic_type_resolution_precollect_roster_inventory(stmt, ctx);
            break;

        case AST_WORLD_DECL:
            semantic_type_resolution_precollect_world_inventory(stmt, ctx);
            break;

        case AST_INTENT_DECL:
            semantic_type_resolution_precollect_intent_inventory(stmt, ctx);
            break;

        case AST_ZONE_DECL:
            semantic_type_resolution_precollect_zone_inventory(stmt, ctx);
            break;

        case AST_RELATION_DECL:
            semantic_type_resolution_precollect_relation_inventory(stmt, ctx);
            break;

        case AST_EFFECT_DECL:
            semantic_type_resolution_precollect_effect_inventory(stmt, ctx);
            break;

        default:
            break;
        }
    }
}
