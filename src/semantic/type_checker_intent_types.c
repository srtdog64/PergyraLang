#include "type_checker_internal.h"

Type *
intent_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;

    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

Type *
intent_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

Type *
intent_resolve_involves_type(ASTNode *involves, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
        return TYPE_UNKNOWN;
    type_ref = ast_intent_involves_subject_type(involves);
    return intent_resolve_type_ref(type_ref, ctx);
}

Type *
intent_resolve_value_type(ASTNode *value, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (value == NULL || value->type != AST_INTENT_VALUE)
        return TYPE_UNKNOWN;
    type_ref = ast_intent_value_type(value);
    return intent_resolve_type_ref(type_ref, ctx);
}

Type *
intent_resolve_step_where_type(ASTNode *step, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (step == NULL || step->type != AST_INTENT_STEP
        || ast_intent_step_where_type(step) == NULL) {
        return NULL;
    }
    type_ref = ast_intent_step_where_type(step);
    return intent_resolve_type_ref(type_ref, ctx);
}

ASTNode *
intent_find_zone_decl_for_type(Type *type, SemanticContext *ctx)
{
    Type *zone_type = intent_normalize_type(type);

    if (ctx == NULL || zone_type == TYPE_UNKNOWN || zone_type->name == NULL)
        return NULL;
    return semantic_find_zone_decl_by_name(ctx, zone_type->name);
}

ASTNode *
intent_resolve_step_where_zone_decl(ASTNode *step,
                                    SemanticContext *ctx,
                                    Type **type_out)
{
    Type *zone_type = intent_resolve_step_where_type(step, ctx);

    if (type_out != NULL)
        *type_out = zone_type;
    return intent_find_zone_decl_for_type(zone_type, ctx);
}

ASTNode *
intent_find_effect_decl_by_name(const char *effect_name, SemanticContext *ctx)
{
    if (ctx == NULL || effect_name == NULL)
        return NULL;
    return semantic_find_effect_decl_by_name(ctx, effect_name);
}

const char *
intent_step_where_source_label(const ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return "unknown step source";
    if (ast_intent_step_inherited_where_from_intent(step))
        return "the intent-level where default";
    if (ast_intent_step_inherited_where_from_action(step))
        return "the matching action contract";
    if (ast_intent_step_derived_where_from_transfer(step))
        return "the transfer target";
    if (ast_intent_step_derived_where_from_using(step))
        return "the using binding";
    return "the step-local where clause";
}
