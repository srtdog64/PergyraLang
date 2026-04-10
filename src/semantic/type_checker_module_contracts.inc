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
        const char *ability = node->data.func_decl.required_abilities[i];
        ASTNode *ability_decl = find_ability_decl_by_name(ctx->program_root, ability);
        if (ability_decl == NULL) {
            semantic_error(ctx, node,
                "action '%s' requires unknown ability '%s'",
                name != NULL ? name : "<anonymous>",
                ability != NULL ? ability : "<ability>");
            continue;
        }
        if (!explicit_type_reference_allowed(ability_decl, node, ctx)) {
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
validate_program_stdlib_use_decls(ASTNode *program, SemanticContext *ctx)
{
    if (program == NULL || program->type != AST_PROGRAM || ctx == NULL)
        return;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_USE_DECL
            || stmt->data.use_decl.module_name == NULL) {
            continue;
        }
        if (!semantic_is_known_stdlib_use_module(stmt->data.use_decl.module_name)) {
            semantic_error(ctx, stmt,
                "Unknown stdlib use '%s'; expected one of datetime, http, page, spray, storage",
                stmt->data.use_decl.module_name);
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            ASTNode *prev = program->data.program.statements[j];
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
}
