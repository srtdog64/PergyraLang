static Type *
ownership_let_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_resolve_or_fallback(ctx, type_ref);
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
