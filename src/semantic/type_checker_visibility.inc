static bool
nominal_decl_matches_runtime_type(ASTNode *decl, Type *object_type)
{
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && object_type != NULL
        && object_type->kind == TYPE_KIND_CLASS
        && decl->data.class_decl.name != NULL
        && object_type->name != NULL
        && strcmp(decl->data.class_decl.name, object_type->name) == 0;
}

static bool
private_member_access_allowed(ASTNode *decl, Type *object_type, SemanticContext *ctx)
{
    ASTNode *host;

    if (!nominal_decl_matches_runtime_type(decl, object_type) || ctx == NULL)
        return false;

    host = current_host_decl(ctx);
    if (host == NULL || host->type != AST_CLASS_DECL)
        return false;

    if (host == decl)
        return true;

    return host->data.class_decl.name != NULL
        && decl->data.class_decl.name != NULL
        && strcmp(host->data.class_decl.name, decl->data.class_decl.name) == 0;
}

static bool
same_module_origin(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

static bool
cross_module_member_access(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || ctx == NULL)
        return false;
    if (ctx->current_module_path == NULL || decl->origin_path == NULL)
        return false;
    return !same_module_origin(ctx->current_module_path, decl->origin_path);
}

static bool
explicit_member_access_allowed(ASTNode *decl,
                               Type *object_type,
                               AccessModifier access,
                               bool has_explicit_access,
                               SemanticContext *ctx)
{
    if (!has_explicit_access)
        return true;
    if (access == ACCESS_PUBLIC)
        return true;
    if (access == ACCESS_PRIVATE)
        return private_member_access_allowed(decl, object_type, ctx);
    if (access == ACCESS_PROTECTED) {
        if (private_member_access_allowed(decl, object_type, ctx))
            return true;
        return !cross_module_member_access(decl, ctx);
    }
    return true;
}

static bool
explicit_type_reference_allowed(ASTNode *decl, const ASTNode *site, SemanticContext *ctx)
{
    if (decl == NULL || site == NULL || ctx == NULL)
        return true;
    if (site->origin_path == NULL || decl->origin_path == NULL)
        return true;
    if (same_module_origin(site->origin_path, decl->origin_path))
        return true;
    if (decl->type == AST_ABILITY_DECL) {
        if (!decl->data.ability_decl.has_explicit_access)
            return true;
        return decl->data.ability_decl.access == ACCESS_PUBLIC;
    }
    return decl->is_exported;
}
