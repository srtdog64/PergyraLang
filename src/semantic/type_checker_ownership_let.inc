static Type *
ownership_let_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;
    if (type_ref == NULL)
        return NULL;
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_ref);
    if (resolved != NULL)
        return resolved;
    return resolve_type_node(type_ref, ctx);
}

bool
type_check_let_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;

    #include "type_checker_ownership_let_claim.inc"
    #include "type_checker_ownership_let_infer.inc"
    #include "type_checker_ownership_let_boundary.inc"
    #include "type_checker_ownership_let_slot.inc"
    #include "type_checker_ownership_let_value.inc"

    return !ctx->has_error;
}
