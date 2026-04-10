static const char *
ability_ref_name(ASTNode *ability_ref)
{
    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return NULL;
    return ability_ref->data.type.name;
}

static ASTNode *
resolve_required_ability_decl(ASTNode *ability_ref,
                              const ASTNode *site,
                              SemanticContext *ctx,
                              const char *owner_label,
                              const char *owner_name)
{
    const char *ability = ability_ref_name(ability_ref);
    ASTNode *ability_decl;

    resolve_type_node(ability_ref, ctx);
    if (ability == NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires a valid ability type reference",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>");
        return NULL;
    }

    ability_decl = find_ability_decl_by_name(ctx->program_root, ability);
    if (ability_decl == NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires unknown ability '%s'",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            ability);
        return NULL;
    }

    if (ability_ref->type == AST_TYPE
        && ability_ref->data.type.generic_args != NULL
        && ability_ref->data.type.generic_args->count > 0) {
        semantic_error(ctx, site,
            "Ability '%s' does not accept generic type arguments in requires clauses",
            ability);
        return NULL;
    }

    return ability_decl;
}

static void
validate_ability_require_fields(ASTNode *node, SemanticContext *ctx)
{
    const char *name;

    if (node == NULL || node->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    name = node->data.ability_decl.name;

    for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
        ASTNode *req = node->data.ability_decl.require_fields[i];
        if (req == NULL || req->data.require_field.name == NULL
            || req->data.require_field.type == NULL) {
            semantic_error(ctx, node,
                "Ability '%s' has an invalid require field declaration",
                name != NULL ? name : "<ability>");
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            ASTNode *prev = node->data.ability_decl.require_fields[j];
            if (prev != NULL && prev->data.require_field.name != NULL
                && strcmp(prev->data.require_field.name,
                          req->data.require_field.name) == 0) {
                semantic_error(ctx, req,
                    "Ability '%s' declares duplicate require field '%s'",
                    name != NULL ? name : "<ability>",
                    req->data.require_field.name);
                break;
            }
        }
        resolve_type_node(req->data.require_field.type, ctx);
        if (req->data.require_field.type != NULL
            && req->data.require_field.type->type == AST_TYPE
            && req->data.require_field.type->data.type.name != NULL) {
            ASTNode *type_decl = find_type_decl_by_name(
                ctx->program_root, req->data.require_field.type->data.type.name);
            if (type_decl != NULL
                && !explicit_type_reference_allowed(type_decl, node, ctx)) {
                semantic_error(ctx, req,
                    "Ability '%s' cannot require field '%s' with non-exported type '%s' from another module",
                    name != NULL ? name : "<ability>",
                    req->data.require_field.name,
                    req->data.require_field.type->data.type.name);
            }
        }
    }
}

static void
validate_action_required_abilities(ASTNode *node,
                                   ASTNode *enclosing_nominal,
                                   SemanticContext *ctx)
{
    const char *name;
    const char *subject_name = NULL;

    if (node == NULL || node->type != AST_FUNC_DECL || ctx == NULL)
        return;

    name = node->data.func_decl.name;

    if (enclosing_nominal == NULL
        || enclosing_nominal->type != AST_CLASS_DECL
        || enclosing_nominal->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        semantic_error(ctx, node,
            "action '%s' is only supported inside subject declarations",
            name != NULL ? name : "<anonymous>");
    } else {
        subject_name = enclosing_nominal->data.class_decl.name;
    }

    for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++) {
        ASTNode *ability_ref = node->data.func_decl.required_abilities[i];
        const char *ability = ability_ref_name(ability_ref);
        ASTNode *ability_decl = resolve_required_ability_decl(
            ability_ref, node, ctx, "action", name);
        if (ability_decl == NULL)
            continue;
        if (ability_decl->type == AST_ABILITY_DECL
            && ability_decl->origin_path != NULL
            && node->origin_path != NULL
            && strcmp(ability_decl->origin_path, node->origin_path) != 0
            && ability_decl->data.ability_decl.has_explicit_access
            && ability_decl->data.ability_decl.access != ACCESS_PUBLIC) {
            semantic_error(ctx, node,
                "action '%s' cannot require non-exported ability '%s' from another module",
                name != NULL ? name : "<anonymous>",
                ability != NULL ? ability : "<ability>");
            continue;
        }
        if (subject_name != NULL
            && !subject_type_has_ability(ctx->program_root, subject_name, ability)) {
            semantic_error(ctx, node,
                "action '%s' requires ability '%s' but subject '%s' does not implement it",
                name != NULL ? name : "<anonymous>",
                ability != NULL ? ability : "<ability>",
                subject_name);
        }
    }
}

static void
validate_stdlib_use_decl(ASTNode *stmt, SemanticContext *ctx)
{
    if (stmt == NULL || stmt->type != AST_USE_DECL || ctx == NULL
        || stmt->data.use_decl.module_name == NULL) {
        return;
    }

    if (!semantic_is_known_stdlib_use_module(stmt->data.use_decl.module_name)) {
        semantic_error(ctx, stmt,
            "Unknown stdlib use '%s'; expected one of datetime, http, page, spray, storage",
            stmt->data.use_decl.module_name);
        return;
    }

    if (ctx->program_root == NULL || ctx->program_root->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < ctx->program_root->data.program.count; i++) {
        ASTNode *prev = ctx->program_root->data.program.statements[i];
        if (prev == stmt)
            break;
        if (prev != NULL && prev->type == AST_USE_DECL
            && prev->data.use_decl.module_name != NULL
            && strcmp(prev->data.use_decl.module_name,
                      stmt->data.use_decl.module_name) == 0) {
            semantic_warning(ctx, stmt,
                "Duplicate stdlib use '%s'; resolver will merge it once",
                stmt->data.use_decl.module_name);
            break;
        }
    }
}
