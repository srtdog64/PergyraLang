#include "type_checker_internal.h"

Type *
intent_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
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
    type_ref = involves->data.intent_involves.subject_type;
    return intent_resolve_type_ref(type_ref, ctx);
}

Type *
intent_resolve_value_type(ASTNode *value, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (value == NULL || value->type != AST_INTENT_VALUE)
        return TYPE_UNKNOWN;
    type_ref = value->data.intent_value.value_type;
    return intent_resolve_type_ref(type_ref, ctx);
}

Type *
intent_resolve_step_where_type(ASTNode *step, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL) {
        return NULL;
    }
    type_ref = step->data.intent_step.where_type;
    return intent_resolve_type_ref(type_ref, ctx);
}

const char *
intent_step_where_source_label(const ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return "unknown step source";
    if (step->data.intent_step.inherited_where_from_intent)
        return "the intent-level where default";
    if (step->data.intent_step.inherited_where_from_action)
        return "the matching action contract";
    if (step->data.intent_step.derived_where_from_transfer)
        return "the transfer target";
    if (step->data.intent_step.derived_where_from_using)
        return "the using binding";
    return "the step-local where clause";
}
