render_type_name_cached_local(ASTNode *type_node)
{
    static char rendered[256];
    char *owned;

    if (type_node == NULL)
        return NULL;

    owned = render_type_name(type_node);
    if (owned == NULL)
        return NULL;
    snprintf(rendered, sizeof(rendered), "%s", owned);
    free(owned);
    return rendered;
}

static const char *
transpiler_decl_name_local(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return decl->data.class_decl.name;
    case AST_ENUM_DECL:
        return decl->data.enum_decl.name;
    case AST_ROLE_DECL:
        return decl->data.role_decl.name;
    case AST_PARTY_DECL:
        return decl->data.party_decl.name;
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.name;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.name;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.name;
    case AST_WORLD_DECL:
        return decl->data.world_decl.name;
    default:
        return NULL;
    }
}

static ASTNode *
transpiler_find_decl_in_inventory_local(TranspilerCtx *ctx,
                                        ASTNodeType decl_type,
                                        const char *name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    if (ctx->last_decl_lookup_result != NULL
        && ctx->last_decl_lookup_type == decl_type
        && !ctx->last_decl_lookup_active_only
        && ctx->last_decl_lookup_inventory == decls
        && ctx->last_decl_lookup_inventory_count == decl_count
        && strcmp(ctx->last_decl_lookup_name, name) == 0) {
        return ctx->last_decl_lookup_result;
    }
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        ASTNode *decl = decls[i];
        const char *decl_name = transpiler_decl_name_local(decl);
        if (decl != NULL && decl->type == decl_type
            && decl_name != NULL
            && strcmp(decl_name, name) == 0) {
            ctx->last_decl_lookup_type = decl_type;
            snprintf(ctx->last_decl_lookup_name,
                sizeof(ctx->last_decl_lookup_name), "%s", name);
            ctx->last_decl_lookup_inventory = decls;
            ctx->last_decl_lookup_inventory_count = decl_count;
            ctx->last_decl_lookup_result = decl;
            ctx->last_decl_lookup_active_only = false;
            return decl;
        }
    }

    return transpiler_find_named_decl_local(ctx, decl_type, name);
}

static ASTNode *
transpiler_find_decl_in_active_inventory_only_local(TranspilerCtx *ctx,
                                                    ASTNodeType decl_type,
                                                    const char *name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    if (ctx->last_decl_lookup_result != NULL
        && ctx->last_decl_lookup_type == decl_type
        && ctx->last_decl_lookup_active_only
        && ctx->last_decl_lookup_inventory == decls
        && ctx->last_decl_lookup_inventory_count == decl_count
        && strcmp(ctx->last_decl_lookup_name, name) == 0) {
        return ctx->last_decl_lookup_result;
    }
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        ASTNode *decl = decls[i];
        const char *decl_name = transpiler_decl_name_local(decl);
        if (decl != NULL && decl->type == decl_type
            && decl_name != NULL
            && strcmp(decl_name, name) == 0) {
            ctx->last_decl_lookup_type = decl_type;
            snprintf(ctx->last_decl_lookup_name,
                sizeof(ctx->last_decl_lookup_name), "%s", name);
            ctx->last_decl_lookup_inventory = decls;
            ctx->last_decl_lookup_inventory_count = decl_count;
            ctx->last_decl_lookup_result = decl;
            ctx->last_decl_lookup_active_only = true;
            return decl;
        }
    }

    return NULL;
}

static void
transpiler_decl_methods_local(ASTNode *decl, ASTNode ***methods_out,
                              size_t *method_count_out)
{
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (decl != NULL) {
        switch (decl->type) {
        case AST_CLASS_DECL:
            methods = decl->data.class_decl.methods;
            method_count = decl->data.class_decl.method_count;
            break;
        case AST_ENUM_DECL:
            methods = decl->data.enum_decl.methods;
            method_count = decl->data.enum_decl.method_count;
            break;
        case AST_RELATION_DECL:
            methods = decl->data.relation_decl.methods;
            method_count = decl->data.relation_decl.method_count;
            break;
        case AST_EFFECT_DECL:
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
            break;
        case AST_ZONE_DECL:
            methods = decl->data.zone_decl.methods;
            method_count = decl->data.zone_decl.method_count;
            break;
        case AST_WORLD_DECL:
            methods = decl->data.world_decl.methods;
            method_count = decl->data.world_decl.method_count;
            break;
        default:
            break;
        }
    }

    if (methods_out != NULL)
        *methods_out = methods;
    if (method_count_out != NULL)
        *method_count_out = method_count;
}

static ASTNode *
transpiler_find_host_decl_from_owner_local(TranspilerCtx *ctx,
                                           const char *owner_name,
                                           ASTNodeType owner_ast_type)
{
    ASTNode *current_host_decl;
    ASTNode *role_decl;

    if (ctx == NULL || owner_name == NULL)
        return NULL;

    current_host_decl = transpiler_current_host_decl_local(ctx);
    if (current_host_decl != NULL
        && current_host_decl->type == owner_ast_type) {
        const char *current_name = transpiler_decl_name_local(current_host_decl);
        if (current_name != NULL && strcmp(current_name, owner_name) == 0)
            return current_host_decl;
    }

    switch (owner_ast_type) {
    case AST_ZONE_DECL:
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_ZONE_DECL, owner_name);
    case AST_RELATION_DECL:
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_RELATION_DECL, owner_name);
    case AST_EFFECT_DECL:
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_EFFECT_DECL, owner_name);
    case AST_WORLD_DECL:
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_WORLD_DECL, owner_name);
    case AST_ROLE_DECL:
        role_decl = transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_ROLE_DECL, owner_name);
        if (role_decl != NULL
            && role_decl->type == AST_ROLE_DECL
            && role_decl->data.role_decl.for_type != NULL
            && role_decl->data.role_decl.for_type->type == AST_TYPE
            && role_decl->data.role_decl.for_type->data.type.name != NULL) {
            return transpiler_find_decl_in_active_inventory_only_local(
                ctx, AST_CLASS_DECL,
                role_decl->data.role_decl.for_type->data.type.name);
        }
        return NULL;
    case AST_ENUM_DECL:
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_ENUM_DECL, owner_name);
    case AST_CLASS_DECL:
    default:
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_CLASS_DECL, owner_name);
    }
}

static const char *
transpiler_role_subject_name_local(TranspilerCtx *ctx, const char *role_name)
{
    ASTNode *role_decl;

    if (ctx == NULL || role_name == NULL)
        return NULL;

    role_decl = transpiler_find_decl_in_active_inventory_only_local(
        ctx, AST_ROLE_DECL, role_name);
    if (role_decl == NULL
        || role_decl->type != AST_ROLE_DECL
        || role_decl->data.role_decl.for_type == NULL
        || role_decl->data.role_decl.for_type->type != AST_TYPE
        || role_decl->data.role_decl.for_type->data.type.name == NULL) {
        return NULL;
    }

    return role_decl->data.role_decl.for_type->data.type.name;
}

static void
transpiler_bind_current_host_decl_local(TranspilerCtx *ctx, ASTNode *decl)
{
    if (ctx == NULL)
        return;

    ctx->current_host_decl = decl;
}

static ASTNode *
transpiler_current_host_decl_local(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->current_host_decl;
}

static ASTNode *
transpiler_find_nominal_host_decl_local(TranspilerCtx *ctx,
                                        const char *host_type_name)
{
    ASTNode *decl = NULL;

    if (ctx == NULL || host_type_name == NULL)
        return NULL;

    if (ctx->last_nominal_host_decl != NULL
        && ctx->last_nominal_host_mir == ctx->mir
        && strcmp(ctx->last_nominal_host_name, host_type_name) == 0) {
        return ctx->last_nominal_host_decl;
    }

    decl = transpiler_find_decl_in_inventory_local(ctx, AST_RELATION_DECL, host_type_name);
    if (decl != NULL)
        goto cache_and_return;
    decl = transpiler_find_decl_in_inventory_local(ctx, AST_EFFECT_DECL, host_type_name);
    if (decl != NULL)
        goto cache_and_return;
    decl = transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL, host_type_name);
    if (decl != NULL)
        goto cache_and_return;
    decl = transpiler_find_decl_in_inventory_local(ctx, AST_WORLD_DECL, host_type_name);
    if (decl != NULL)
        goto cache_and_return;
    decl = transpiler_find_decl_in_inventory_local(ctx, AST_ENUM_DECL, host_type_name);
    if (decl != NULL)
        goto cache_and_return;
    decl = transpiler_find_decl_in_inventory_local(ctx, AST_CLASS_DECL, host_type_name);
    if (decl == NULL)
        return NULL;

cache_and_return:
    snprintf(ctx->last_nominal_host_name,
        sizeof(ctx->last_nominal_host_name), "%s", host_type_name);
    ctx->last_nominal_host_mir = ctx->mir;
    ctx->last_nominal_host_decl = decl;
    return decl;
}

static const char *
current_field_type_name(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL) {
        if (decl->type == AST_CLASS_DECL) {
            for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
                ClassField *field = decl->data.class_decl.fields[i];
                if (field != NULL && field->name != NULL
                    && strcmp(field->name, field_name) == 0
                    && field->type != NULL && field->type->type == AST_TYPE) {
                    return render_type_name_cached_local(field->type);
                }
            }
        } else if (decl->type == AST_ZONE_DECL) {
            ASTNode *slot = find_zone_domain_slot(decl, field_name);
            if (slot != NULL && slot->data.domain_slot.type != NULL
                && slot->data.domain_slot.type->type == AST_TYPE) {
                return render_type_name_cached_local(slot->data.domain_slot.type);
            }
            for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
                ASTNode *layer = decl->data.zone_decl.layer_slots[i];
                if (layer != NULL && layer->type == AST_ZONE_LAYER_SLOT
                    && layer->data.zone_layer_slot.slot_name != NULL
                    && strcmp(layer->data.zone_layer_slot.slot_name, field_name) == 0) {
                    return layer->data.zone_layer_slot.layer_type;
                }
            }
            for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
                ASTNode *shared = decl->data.zone_decl.shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, field_name) == 0
                    && shared->data.party_shared.type != NULL
                    && shared->data.party_shared.type->type == AST_TYPE) {
                    return render_type_name_cached_local(shared->data.party_shared.type);
                }
            }
        } else if (decl->type == AST_RELATION_DECL) {
            for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
                ASTNode *slot = decl->data.relation_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, field_name) == 0
                    && slot->data.domain_slot.type != NULL
                    && slot->data.domain_slot.type->type == AST_TYPE) {
                    return render_type_name_cached_local(slot->data.domain_slot.type);
                }
            }
        } else if (decl->type == AST_EFFECT_DECL) {
            for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
                ASTNode *slot = decl->data.effect_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, field_name) == 0
                    && slot->data.domain_slot.type != NULL
                    && slot->data.domain_slot.type->type == AST_TYPE) {
                    return render_type_name_cached_local(slot->data.domain_slot.type);
                }
            }
        } else if (decl->type == AST_WORLD_DECL) {
            for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
                ASTNode *slot = decl->data.world_decl.rosters[i];
                if (slot != NULL && slot->data.world_roster.slot_name != NULL
                    && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                    return slot->data.world_roster.roster_type;
                }
            }
            for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
                ASTNode *slot = decl->data.world_decl.zones[i];
                if (slot != NULL && slot->data.world_zone.slot_name != NULL
                    && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                    return slot->data.world_zone.zone_type;
                }
            }
            for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
                ASTNode *shared = decl->data.world_decl.shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, field_name) == 0
                    && shared->data.party_shared.type != NULL
                    && shared->data.party_shared.type->type == AST_TYPE) {
                    return render_type_name_cached_local(shared->data.party_shared.type);
                }
            }
        }
    }

    return NULL;
}

static ASTNode *
current_host_method_decl(TranspilerCtx *ctx, const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || method_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    transpiler_decl_methods_local(decl, &methods, &method_count);

    for (size_t i = 0; methods != NULL && i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static ASTNode *
find_nominal_host_method_decl(TranspilerCtx *ctx, const char *host_type_name,
                              const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    if (ctx->last_nominal_method_decl != NULL
        && ctx->last_nominal_method_mir == ctx->mir
        && strcmp(ctx->last_nominal_method_host_name, host_type_name) == 0
        && strcmp(ctx->last_nominal_method_name, method_name) == 0) {
        return ctx->last_nominal_method_decl;
    }

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    transpiler_decl_methods_local(decl, &methods, &method_count);

    for (size_t i = 0; methods != NULL && i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            snprintf(ctx->last_nominal_method_host_name,
                sizeof(ctx->last_nominal_method_host_name), "%s", host_type_name);
            snprintf(ctx->last_nominal_method_name,
                sizeof(ctx->last_nominal_method_name), "%s", method_name);
            ctx->last_nominal_method_mir = ctx->mir;
            ctx->last_nominal_method_decl = method;
            return method;
        }
    }

    return NULL;
}

static const char *
lookup_nominal_host_member_type_name(TranspilerCtx *ctx, const char *host_type_name,
                                     const char *member_name)
{
    ASTNode *decl;

    if (ctx == NULL || host_type_name == NULL || member_name == NULL)
        return NULL;

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            if (field != NULL && field->name != NULL
                && strcmp(field->name, member_name) == 0
                && field->type != NULL && field->type->type == AST_TYPE) {
                return render_type_name_cached_local(field->type);
            }
        }
        break;
    case AST_ZONE_DECL: {
        ASTNode *slot = find_zone_domain_slot(decl, member_name);
        if (slot != NULL && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE) {
            return render_type_name_cached_local(slot->data.domain_slot.type);
        }
        for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
            ASTNode *layer = decl->data.zone_decl.layer_slots[i];
            if (layer != NULL && layer->type == AST_ZONE_LAYER_SLOT
                && layer->data.zone_layer_slot.slot_name != NULL
                && strcmp(layer->data.zone_layer_slot.slot_name, member_name) == 0) {
                return layer->data.zone_layer_slot.layer_type;
            }
        }
        for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return render_type_name_cached_local(shared->data.party_shared.type);
            }
        }
        break;
    }
    case AST_RELATION_DECL:
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, member_name) == 0
                && slot->data.domain_slot.type != NULL
                && slot->data.domain_slot.type->type == AST_TYPE) {
                return render_type_name_cached_local(slot->data.domain_slot.type);
            }
        }
        for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return render_type_name_cached_local(shared->data.party_shared.type);
            }
        }
        break;
    case AST_EFFECT_DECL:
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, member_name) == 0
                && slot->data.domain_slot.type != NULL
                && slot->data.domain_slot.type->type == AST_TYPE) {
                return render_type_name_cached_local(slot->data.domain_slot.type);
            }
        }
        for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return render_type_name_cached_local(shared->data.party_shared.type);
            }
        }
        break;
    case AST_WORLD_DECL:
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *slot = decl->data.world_decl.rosters[i];
            if (slot != NULL && slot->data.world_roster.slot_name != NULL
                && strcmp(slot->data.world_roster.slot_name, member_name) == 0) {
                return slot->data.world_roster.roster_type;
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *slot = decl->data.world_decl.zones[i];
            if (slot != NULL && slot->data.world_zone.slot_name != NULL
                && strcmp(slot->data.world_zone.slot_name, member_name) == 0) {
                return slot->data.world_zone.zone_type;
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
            ASTNode *shared = decl->data.world_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return render_type_name_cached_local(shared->data.party_shared.type);
            }
        }
        break;
    default:
        break;
    }

    return NULL;
}

static const char *
resolve_nominal_host_expr_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    if (ctx == NULL || expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        ASTNode *alias_expr = lookup_alias_expr(ctx, expr->data.identifier.name);
        if (alias_expr != NULL)
            return resolve_nominal_host_expr_type_name(ctx, alias_expr);
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        return current_field_type_name(ctx, expr->data.identifier.name);
    }

    if (expr->type == AST_MEMBER_ACCESS
        && expr->data.member.object != NULL
        && expr->data.member.name != NULL) {
        /* self.field */
        if (expr->data.member.object->type == AST_IDENTIFIER
            && strcmp(expr->data.member.object->data.identifier.name, "self") == 0) {
            return current_field_type_name(ctx, expr->data.member.name);
        }
        {
            const char *obj_type = resolve_nominal_host_expr_type_name(ctx,
                expr->data.member.object);
            const char *member_type = lookup_nominal_host_member_type_name(ctx,
                obj_type, expr->data.member.name);
            if (member_type != NULL)
                return member_type;
        }
    }

    return NULL;
}

static bool
world_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return false;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return true;
        }
    }

    return false;
}

static ASTNode *
find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
resolve_world_zone_decl(TranspilerCtx *ctx, ASTNode *world_decl, const char *slot_name)
{
    ASTNode *zone_slot = find_world_zone_slot_decl(world_decl, slot_name);
    if (ctx == NULL || zone_slot == NULL || zone_slot->data.world_zone.zone_type == NULL)
        return NULL;
    return find_zone_decl(ctx, zone_slot->data.world_zone.zone_type);
}

static char *
emit_builtin_to_dto(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    ASTNode *target_decl;
    ASTNode *source_decl;
    const char *source_type_name;
    char *source_expr;
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (call->data.call.arg_count != 2)
        return pergyra_strdup("/* ToTObject: invalid args */");

    target_arg = call->data.call.arguments[0];
    source_arg = call->data.call.arguments[1];
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL) {
        return pergyra_strdup("/* ToTObject: need target tobject type */");
    }
    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER) {
        return pergyra_strdup("/* ToTObject: need named subject source */");
    }

    target_decl = find_class_decl(ctx, target_arg->data.identifier.name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct)
        return pergyra_strdup("/* ToTObject: target must be tobject/struct */");

    source_type_name = infer_expression_type_name(ctx, source_arg);
    source_decl = find_class_decl(ctx, source_type_name);
    if (source_decl == NULL)
        return pergyra_strdup("/* ToTObject: source subject type not found */");

    source_expr = emit_expression(source_arg, ctx);
    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_arg->data.identifier.name);

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, target_field->name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
        free(source_path);
    }

    codebuf_write(buf, " }");
    free(source_expr);
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static char *
emit_projection_literal(TranspilerCtx *ctx, ASTNode *target_decl, ASTNode *source_decl,
                        ASTNode *refresh, const char *target_type_name,
                        const char *source_expr)
{
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (target_decl == NULL || source_decl == NULL
        || target_type_name == NULL || source_expr == NULL) {
        return pergyra_strdup("0");
    }

    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_type_name);

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        const char *source_field_name = NULL;
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_field_name = target_field->name;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
            for (size_t j = 0; j < refresh->data.zone_refresh.field_map_count; j++) {
                const char *mapped_target =
                    refresh->data.zone_refresh.mapped_target_fields[j];
                const char *mapped_source =
                    refresh->data.zone_refresh.mapped_source_fields[j];
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, target_field->name) == 0) {
                    source_field_name = mapped_source;
                    break;
                }
            }
        }

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, source_field_name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
        free(source_path);
    }

    codebuf_write(buf, " }");
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static ASTNode *
find_ability_decl(TranspilerCtx *ctx, const char *ability_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ABILITY_DECL, ability_name);
}
