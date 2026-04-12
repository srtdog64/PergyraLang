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
current_field_type_name(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return NULL;

    if (ctx->current_class_name != NULL) {
        decl = find_class_decl(ctx, ctx->current_class_name);
        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
                ClassField *field = decl->data.class_decl.fields[i];
                if (field != NULL && field->name != NULL
                    && strcmp(field->name, field_name) == 0
                    && field->type != NULL && field->type->type == AST_TYPE) {
                    return render_type_name_cached_local(field->type);
                }
            }
        }
    }

    if (ctx->current_zone_name != NULL) {
        decl = find_zone_decl(ctx, ctx->current_zone_name);
        if (decl != NULL && decl->type == AST_ZONE_DECL) {
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
        }
    }

    if (ctx->current_relation_name != NULL) {
        decl = find_relation_decl(ctx, ctx->current_relation_name);
        if (decl != NULL && decl->type == AST_RELATION_DECL) {
            for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
                ASTNode *slot = decl->data.relation_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, field_name) == 0
                    && slot->data.domain_slot.type != NULL
                    && slot->data.domain_slot.type->type == AST_TYPE) {
                    return render_type_name_cached_local(slot->data.domain_slot.type);
                }
            }
        }
    }

    if (ctx->current_effect_name != NULL) {
        decl = find_effect_decl(ctx, ctx->current_effect_name);
        if (decl != NULL && decl->type == AST_EFFECT_DECL) {
            for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
                ASTNode *slot = decl->data.effect_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, field_name) == 0
                    && slot->data.domain_slot.type != NULL
                    && slot->data.domain_slot.type->type == AST_TYPE) {
                    return render_type_name_cached_local(slot->data.domain_slot.type);
                }
            }
        }
    }

    if (ctx->current_world_name != NULL) {
        decl = find_world_decl(ctx, ctx->current_world_name);
        if (decl != NULL && decl->type == AST_WORLD_DECL) {
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

static const char *
current_host_decl_name(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_relation_name != NULL)
        return ctx->current_relation_name;
    if (ctx->current_effect_name != NULL)
        return ctx->current_effect_name;
    if (ctx->current_zone_name != NULL)
        return ctx->current_zone_name;
    if (ctx->current_world_name != NULL)
        return ctx->current_world_name;
    return ctx->current_class_name;
}

static ASTNode *
current_host_method_decl(TranspilerCtx *ctx, const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || method_name == NULL)
        return NULL;

    if (ctx->current_relation_name != NULL) {
        decl = find_relation_decl(ctx, ctx->current_relation_name);
        if (decl != NULL) {
            methods = decl->data.relation_decl.methods;
            method_count = decl->data.relation_decl.method_count;
        }
    } else if (ctx->current_effect_name != NULL) {
        decl = find_effect_decl(ctx, ctx->current_effect_name);
        if (decl != NULL) {
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
        }
    } else if (ctx->current_zone_name != NULL) {
        decl = find_zone_decl(ctx, ctx->current_zone_name);
        if (decl != NULL) {
            methods = decl->data.zone_decl.methods;
            method_count = decl->data.zone_decl.method_count;
        }
    } else if (ctx->current_world_name != NULL) {
        decl = find_world_decl(ctx, ctx->current_world_name);
        if (decl != NULL) {
            methods = decl->data.world_decl.methods;
            method_count = decl->data.world_decl.method_count;
        }
    } else if (ctx->current_class_name != NULL) {
        decl = find_class_decl(ctx, ctx->current_class_name);
        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            methods = decl->data.class_decl.methods;
            method_count = decl->data.class_decl.method_count;
        }
    }

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

    decl = find_relation_decl(ctx, host_type_name);
    if (decl != NULL) {
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
    } else {
        decl = find_effect_decl(ctx, host_type_name);
        if (decl != NULL) {
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
        } else {
            decl = find_zone_decl(ctx, host_type_name);
            if (decl != NULL) {
                methods = decl->data.zone_decl.methods;
                method_count = decl->data.zone_decl.method_count;
            } else {
                decl = find_world_decl(ctx, host_type_name);
                if (decl != NULL) {
                    methods = decl->data.world_decl.methods;
                    method_count = decl->data.world_decl.method_count;
                } else {
                    decl = find_enum_decl(ctx, host_type_name);
                    if (decl != NULL && decl->type == AST_ENUM_DECL) {
                        methods = decl->data.enum_decl.methods;
                        method_count = decl->data.enum_decl.method_count;
                    } else {
                        decl = find_class_decl(ctx, host_type_name);
                        if (decl != NULL && decl->type == AST_CLASS_DECL) {
                            methods = decl->data.class_decl.methods;
                            method_count = decl->data.class_decl.method_count;
                        }
                    }
                }
            }
        }
    }

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
find_nominal_host_decl_local(TranspilerCtx *ctx, const char *host_type_name)
{
    ASTNode *decl;

    if (ctx == NULL || host_type_name == NULL)
        return NULL;

    decl = find_relation_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_effect_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_zone_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_world_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_subject_host_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_class_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    return NULL;
}

static const char *
lookup_nominal_host_member_type_name(TranspilerCtx *ctx, const char *host_type_name,
                                     const char *member_name)
{
    ASTNode *decl;

    if (ctx == NULL || host_type_name == NULL || member_name == NULL)
        return NULL;

    decl = find_nominal_host_decl_local(ctx, host_type_name);
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
    ASTNode **abilities = NULL;
    size_t ability_count = 0;

    if (ctx == NULL || ability_name == NULL)
        return NULL;
    transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count);
    if (abilities == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *stmt = abilities[i];
        if (stmt != NULL && stmt->type == AST_ABILITY_DECL
            && stmt->data.ability_decl.name != NULL
            && strcmp(stmt->data.ability_decl.name, ability_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_event_decl(TranspilerCtx *ctx, const char *event_name)
{
    ASTNode **events = NULL;
    size_t event_count = 0;

    if (ctx == NULL || event_name == NULL)
        return NULL;
    transpiler_active_inventory(ctx, AST_EVENT_DECL, &events, &event_count);
    if (events == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < event_count; i++) {
        ASTNode *stmt = events[i];
        if (stmt != NULL && stmt->type == AST_EVENT_DECL
            && stmt->data.event_decl.name != NULL
            && strcmp(stmt->data.event_decl.name, event_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static bool
func_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_FUNC_DECL
        && node->data.func_decl.generic_params != NULL
        && node->data.func_decl.generic_params->count > 0;
}

static const char *
lookup_generic_binding(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
        if (strcmp(ctx->generic_bindings[i].name, name) == 0)
            return ctx->generic_bindings[i].concrete_type;
    }

    return NULL;
}

static bool
infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count);

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count);

static const char *
lookup_enum_variant_qualified_name(TranspilerCtx *ctx, const char *variant_name)
{
    static char qualified[128];
    ASTNode **types = NULL;
    size_t type_count = 0;

    if (ctx == NULL || variant_name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            const char *candidate = stmt->data.enum_decl.variants[j];
            if (candidate != NULL && strcmp(candidate, variant_name) == 0) {
                snprintf(qualified, sizeof(qualified), "%s_%s",
                    stmt->data.enum_decl.name, candidate);
                return qualified;
            }
        }
    }

    return NULL;
}

static const char *
operator_overload_suffix(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "add";
    case TOKEN_MINUS:         return "sub";
    case TOKEN_STAR:          return "mul";
    case TOKEN_SLASH:         return "div";
    case TOKEN_PERCENT:       return "mod";
    case TOKEN_EQUAL:         return "eq";
    case TOKEN_NOT_EQUAL:     return "ne";
    case TOKEN_LESS:          return "lt";
    case TOKEN_LESS_EQUAL:    return "le";
    case TOKEN_GREATER:       return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default:                  return NULL;
    }
}

static ASTNode *
find_role_operator_method_decl(TranspilerCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth);

static ASTNode *
find_operator_overload_decl(TranspilerCtx *ctx, const char *type_name, PgyTokenType op)
{
    const char *suffix = operator_overload_suffix(op);
    char fn_name[256];
    ASTNode **roles = NULL;
    size_t role_count = 0;

    if (ctx == NULL || type_name == NULL || suffix == NULL)
        return NULL;

    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, type_name);
    ASTNode *fn = find_function_decl(ctx, fn_name);
    if (fn != NULL)
        return fn;

    transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count);
    if (roles != NULL) {
        for (size_t i = 0; i < role_count; i++) {
            ASTNode *role = roles[i];
            if (role == NULL || role->type != AST_ROLE_DECL
                || role->data.role_decl.for_type == NULL
                || role->data.role_decl.for_type->type != AST_TYPE
                || role->data.role_decl.for_type->data.type.name == NULL
                || strcmp(role->data.role_decl.for_type->data.type.name, type_name) != 0) {
                continue;
            }
            if (find_role_operator_method_decl(ctx, role, op, 0) != NULL)
                return role;
        }
    }

    return NULL;
}

static bool
operator_method_name_matches(PgyTokenType op, const char *name)
{
    static const struct {
        PgyTokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }
    return false;
}

static ASTNode *
find_role_operator_method_decl(TranspilerCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *include_stmt = role->data.role_decl.includes[i];
        ASTNode *included_role = find_role_decl(ctx,
            include_stmt->data.include_stmt.role_name);
        ASTNode *method = find_role_operator_method_decl(ctx, included_role, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static bool
role_has_ability(ASTNode *role, const char *ability_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || ability_name == NULL)
        return false;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl->type == AST_IMPL_ABILITY
            && impl->data.impl_ability.ability_ref != NULL
            && impl->data.impl_ability.ability_ref->type == AST_TYPE
            && impl->data.impl_ability.ability_ref->data.type.name != NULL
            && strcmp(impl->data.impl_ability.ability_ref->data.type.name, ability_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
role_has_method(ASTNode *role, const char *method_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || method_name == NULL)
        return false;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, method_name) == 0) {
                return true;
            }
        }
    }

    return false;
}

/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

const char *
pergyra_primitive_to_c(const char *name)
{
    if (strcmp(name, "Int")    == 0) return "int32_t";
    if (strcmp(name, "Long")   == 0) return "int64_t";
    if (strcmp(name, "Float")  == 0) return "float";
    if (strcmp(name, "Double") == 0) return "double";
    if (strcmp(name, "Bool")   == 0) return "bool";
    if (strcmp(name, "String") == 0) return "char*";
    if (strcmp(name, "QubitSlot") == 0) return "int32_t";
    if (strcmp(name, "Void")   == 0) return "void";
    /* Single uppercase letter = generic type parameter → void* (type erasure) */
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
        return "void*";
    return name; /* user-defined type — pass through */
}

/*
 * slot_inner_type_name("Slot<Int>")       → "Int"
 * slot_inner_type_name("SecureSlot<Int>") → "Int"
 */
const char *
slot_inner_type_name(const char *slot_type_name)
{
    const char *open = strchr(slot_type_name, '<');
    if (open == NULL)
        return slot_type_name;
    open++; /* skip '<' */

    /* Trim trailing '>' — return a static buffer (fine for our uses) */
    static char buf[128];
    const char *close = strrchr(slot_type_name, '>');
    if (close == NULL || close <= open)
        return "Int";

    size_t len = (size_t)(close - open);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, open, len);
    buf[len] = '\0';
    return buf;
}

static const char *
channel_inner_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    const char *type_name = infer_expression_type_name(ctx, expr);
    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
        return slot_inner_type_name(type_name);
    return "Int";
}

static void
sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size)
{
    size_t out = 0;
    bool last_was_underscore = false;

    if (buf_size == 0)
        return;

    if (type_name == NULL || *type_name == '\0') {
        snprintf(buf, buf_size, "Int");
        return;
    }

    for (size_t i = 0; type_name[i] != '\0' && out + 1 < buf_size; i++) {
        unsigned char ch = (unsigned char)type_name[i];
        if ((ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')) {
            buf[out++] = (char)ch;
            last_was_underscore = false;
        } else {
            if (out == 0 || last_was_underscore)
                continue;
            buf[out++] = '_';
            last_was_underscore = true;
        }
    }
    while (out > 0 && buf[out - 1] == '_')
        out--;
    if (out == 0) {
        snprintf(buf, buf_size, "Int");
        return;
    }
    if (buf[0] >= '0' && buf[0] <= '9') {
        if (out + 2 >= buf_size)
            out = buf_size > 3 ? buf_size - 3 : 0;
        if (out > 0)
            memmove(buf + 2, buf, out);
        buf[0] = 'T';
        buf[1] = '_';
        out += 2;
    }
    buf[out] = '\0';
}

static const char *
constructed_arg_name_at(const char *type_name, int arg_index)
{
    static char buf[128];
    const char *lt;
    int current = 0;
    int depth = 0;
    size_t out = 0;

    if (type_name == NULL || arg_index < 0)
        return "Int";

    lt = strchr(type_name, '<');
    if (lt == NULL)
        return "Int";
    lt++;

    for (const char *p = lt; *p != '\0'; p++) {
        char ch = *p;
        if (ch == '<') {
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth++;
            continue;
        }
        if (ch == '>') {
            if (depth == 0)
                break;
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth--;
            continue;
        }
        if (ch == ',' && depth == 0) {
            if (current == arg_index)
                break;
            current++;
            continue;
        }
        if (current == arg_index) {
            if (!(out == 0 && ch == ' ') && out + 1 < sizeof(buf))
                buf[out++] = ch;
        }
    }

    while (out > 0 && buf[out - 1] == ' ')
        out--;
    buf[out] = '\0';
    return out > 0 ? buf : "Int";
}

const char *
pergyra_type_to_c(const char *name)
{
    static char buf[128];
    char suffix[96];

    if (strcmp(name, "HashMap") == 0)
        return "PgyHashMap_Int";
    if (strcmp(name, "HashMapStr") == 0)
        return "PgyHashMap_String";
    if (strcmp(name, "List") == 0)
        return "PgyList_Int";
    if (strcmp(name, "ListStr") == 0)
        return "PgyList_String";
    if (strcmp(name, "Set") == 0)
        return "PgySet_String";
    if (strcmp(name, "Queue") == 0)
        return "PgyQueue_Int";
    if (strcmp(name, "Fsm") == 0)
        return "PgyFsm";
    if (strcmp(name, "Timer") == 0)
        return "PgyTimer";
    if (strcmp(name, "Cooldown") == 0)
        return "PgyCooldown";
    if (strcmp(name, "Allocator") == 0)
        return "PgyAllocator";
    if (strncmp(name, "List<", 5) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyList_%s", suffix);
        return buf;
    }
    if (strncmp(name, "HashMap<", 8) == 0) {
        sanitize_c_suffix(constructed_arg_name_at(name, 1), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyHashMap_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Queue<", 6) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyQueue_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Set<", 4) == 0) {
        const char *inner = slot_inner_type_name(name);
        if (inner != NULL && strcmp(inner, "String") == 0) {
            snprintf(buf, sizeof(buf), "PgySet_String");
        } else if (inner != NULL && strcmp(inner, "Int") == 0) {
            snprintf(buf, sizeof(buf), "PgySet_int");
        } else {
            sanitize_c_suffix(inner, suffix, sizeof(suffix));
            snprintf(buf, sizeof(buf), "PgySet_%s", suffix);
        }
        return buf;
    }
    if (strncmp(name, "RemoteFuture<", 13) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Future<", 7) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Channel<", 8) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyChannel_%s", inner);
        return buf;
    }
    if (strncmp(name, "Weak<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyWeak_%s", inner);
        return buf;
    }
    if (strncmp(name, "Rc<", 3) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyRc_%s", inner);
        return buf;
    }
    if (strncmp(name, "Box<Array<", 10) == 0) {
        static char buf[128];
        const char *inner = name + 10;
        const char *close = strstr(inner, ">>");
        size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
        if (len > 63) len = 63;
        char inner_buf[64];
        memcpy(inner_buf, inner, len);
        inner_buf[len] = '\0';
        snprintf(buf, sizeof(buf), "PgyBoxArray_%s", inner_buf);
        return buf;
    }
    if (strncmp(name, "Box<", 4) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyBox_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slice<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlice_%s", inner);
        return buf;
    }
    if (strncmp(name, "Array<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyArray_%s", inner);
        return buf;
    }
    if (strncmp(name, "SecureSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySecureSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "ReadView<", 9) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "WriteView<", 10) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "DeviceSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyDeviceSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slot<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Result<", 7) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyResult_%s", inner);
        return buf;
    }
    if (strncmp(name, "Option<", 7) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyOption_%s", inner);
        return buf;
    }
    return pergyra_primitive_to_c(name);
}

static void
append_type_name(CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    {
        const char *bound = NULL;
        if (g_type_render_ctx != NULL)
            bound = lookup_generic_binding(g_type_render_ctx, type_node->data.type.name);
        codebuf_write(buf, "%s", bound != NULL ? bound : type_node->data.type.name);
    }
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        codebuf_write(buf, "<");
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *param = type_node->data.type.generic_args->params[i];
            if (i > 0)
                codebuf_write(buf, ", ");
            if (param != NULL && param->constraint != NULL) {
                append_type_name(buf, param->constraint);
            } else if (param != NULL && param->name != NULL) {
                codebuf_write(buf, "%s", param->name);
            } else {
                codebuf_write(buf, "Int");
            }
        }
        codebuf_write(buf, ">");
    }
}

static bool resolve_slot_target(TranspilerCtx *ctx, ASTNode *slot_arg,
                                const char **inner_out, const char **slot_name_out,
                                bool *secure_out);

static char *
render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name(type_node->data.channel_type.element_type);
        char *result = strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(type_node->data.future_type.value_type);
        char *result = strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("Int");
    append_type_name(buf, type_node);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static char *
render_ability_ref_vtable_tag(ASTNode *ability_ref)
{
    char suffix[128];
    size_t len;
    char *rendered;

    if (ability_ref == NULL)
        return NULL;

    rendered = render_type_name(ability_ref);
    if (rendered == NULL)
        return NULL;
    sanitize_c_suffix(rendered, suffix, sizeof(suffix));
    len = strlen(suffix);
    while (len > 0 && suffix[len - 1] == '_')
        suffix[--len] = '\0';
    if (len == 0) {
        free(rendered);
        return NULL;
    }
    free(rendered);
    return pergyra_strdup(suffix);
}

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count)
{
    int saved_binding_count = ctx->generic_binding_count;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char *result;

    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++)
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];

    g_type_render_ctx = ctx;
    result = render_type_name(type_node);
    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    return result;
}

static void
ensure_collection_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                    const char *kind, const char *inner_type)
{
    char suffix[128];
    const char *ctype;

    if (ctx == NULL || dst == NULL || kind == NULL || inner_type == NULL)
        return;

    if ((strcmp(kind, "List") == 0 || strcmp(kind, "Queue") == 0)
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;
    if (strcmp(kind, "Map") == 0
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;
    if (strcmp(kind, "Set") == 0
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;  /* Int and String Set are pre-instantiated */

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    for (int i = 0; i < ctx->collection_spec_count; i++) {
        if (strcmp(ctx->collection_specs[i].kind, kind) == 0
            && strcmp(ctx->collection_specs[i].suffix, suffix) == 0) {
            return;
        }
    }

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS)
        return;

    ctype = pergyra_type_to_c(inner_type);
    if (ctype == NULL)
        return;

    snprintf(ctx->collection_specs[ctx->collection_spec_count].kind,
             sizeof(ctx->collection_specs[ctx->collection_spec_count].kind),
             "%s", kind);
    snprintf(ctx->collection_specs[ctx->collection_spec_count].suffix,
             sizeof(ctx->collection_specs[ctx->collection_spec_count].suffix),
             "%s", suffix);
    ctx->collection_spec_count++;

    if (strcmp(kind, "List") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_LIST_%s */\n"
            "PGY_LIST_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Queue") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_QUEUE_%s */\n"
            "PGY_QUEUE_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Map") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_MAP_%s */\n"
            "PGY_HASHMAP_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Set") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_SET_%s */\n"
            "PGY_SET_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    }
}

static void
ensure_collection_specialization(TranspilerCtx *ctx, const char *kind,
                                 const char *inner_type)
{
    ensure_collection_specialization_to(ctx, ctx->decls, kind, inner_type);
}

static void
ensure_type_specializations_from_ast_to(TranspilerCtx *ctx, CodeBuf *dst, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        if (type_node->data.event_handler_type.return_type != NULL)
            ensure_type_specializations_from_ast_to(ctx, dst,
                type_node->data.event_handler_type.return_type);
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                type_node->data.event_handler_type.param_types[i]);
        }
        return;
    }

    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return;

    {
        ASTNode *alias_decl = find_type_alias_decl(ctx, type_node->data.type.name);
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                alias_decl->data.type_alias.target_type);
            return;
        }
    }

    if (type_node->data.type.generic_args != NULL) {
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *arg = type_node->data.type.generic_args->params[i];
            if (arg != NULL && arg->constraint != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, arg->constraint);
        }
    }

    if (strcmp(type_node->data.type.name, "List") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner = render_type_name(type_node->data.type.generic_args->params[0]->constraint);
        ensure_collection_specialization_to(ctx, dst, "List", inner);
        free(inner);
        return;
    }

    if (strcmp(type_node->data.type.name, "Queue") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner = render_type_name(type_node->data.type.generic_args->params[0]->constraint);
        ensure_collection_specialization_to(ctx, dst, "Queue", inner);
        free(inner);
        return;
    }

    if (strcmp(type_node->data.type.name, "HashMap") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 1
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *value = render_type_name(type_node->data.type.generic_args->params[1]->constraint);
        ensure_collection_specialization_to(ctx, dst, "Map", value);
        free(value);
        return;
    }
}

static void
ensure_type_specializations_from_ast(TranspilerCtx *ctx, ASTNode *type_node)
{
    ensure_type_specializations_from_ast_to(ctx, ctx->decls, type_node);
}

static void
ensure_collection_specializations_from_stmt_to(TranspilerCtx *ctx, CodeBuf *dst, ASTNode *node)
{
    if (ctx == NULL || dst == NULL || node == NULL)
        return;

    switch (node->type) {
    case AST_FUNC_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst, node->data.func_decl.return_type);
        for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
            FuncParam *p = node->data.func_decl.params[i];
            if (p != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, p->type);
        }
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.func_decl.body);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.block.statements[i]);
        break;
    case AST_LET_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst, node->data.let_decl.type);
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            node->data.let_decl.initializer);
        break;
    case AST_LET_DESTRUCTURE:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            node->data.let_destructure.initializer);
        break;
    case AST_WITH_STMT:
        ensure_type_specializations_from_ast_to(ctx, dst, node->data.with_stmt.slot_type);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.with_stmt.body);
        break;
    case AST_IF_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.if_stmt.condition);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.if_stmt.then_branch);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.if_stmt.else_branch);
        break;
    case AST_WHILE_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.while_loop.condition);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.while_loop.body);
        break;
    case AST_FOR_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.range_start);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.range_end);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.iterable);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.body);
        break;
    case AST_MATCH_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.match_stmt.subject);
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.match_stmt.cases[i]);
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            node->data.match_stmt.default_body);
        break;
    case AST_MATCH_CASE:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.match_case.guard);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.match_case.body);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.async_block.statements[i]);
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.parallel.tasks[i]);
        break;
    default:
        break;
    }
}

static const char *
collection_runtime_suffix(const char *inner_type)
{
    static char suffix[128];

    if (inner_type == NULL)
        return "int";
    if (strcmp(inner_type, "Int") == 0)
        return "int";
    if (strcmp(inner_type, "String") == 0)
        return "string";

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    return suffix;
}

static const char *
infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    static char inferred_call_type[128];

    if (expr == NULL)
        return "Int";

    switch (expr->type) {
    case AST_NUMBER:
        return "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_ARRAY_LITERAL: {
        const char *inner = "Int";
        if (expr->data.array_literal.count > 0)
            inner = infer_expression_type_name(ctx, expr->data.array_literal.elements[0]);
        static char buf[128];
        snprintf(buf, sizeof(buf), "Array<%s>", inner);
        return buf;
    }
    case AST_ARRAY_ACCESS: {
        const char *array_type = infer_expression_type_name(ctx, expr->data.array_access.array);
        if (strncmp(array_type, "Array<", 6) == 0 || strncmp(array_type, "Slice<", 6) == 0)
            return slot_inner_type_name(array_type);
        return "Int";
    }
    case AST_IDENTIFIER: {
        ASTNode *alias_expr = lookup_alias_expr(ctx, expr->data.identifier.name);
        if (alias_expr != NULL)
            return infer_expression_type_name(ctx, alias_expr);
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        type_name = current_field_type_name(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        {
            const char *enum_variant = lookup_enum_variant_qualified_name(ctx,
                expr->data.identifier.name);
            if (enum_variant != NULL) {
                size_t len = strcspn(enum_variant, "_");
                static char enum_name[128];
                if (len >= sizeof(enum_name))
                    len = sizeof(enum_name) - 1;
                memcpy(enum_name, enum_variant, len);
                enum_name[len] = '\0';
                return enum_name;
            }
        }
        return "Int";
    }
    case AST_CHANNEL_RECV: {
        ASTNode *channel = expr->data.channel_recv.channel;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                return slot_inner_type_name(type_name);
        }
        return "Int";
    }
    case AST_MEMBER_ACCESS: {
        const char *resolved = resolve_nominal_host_expr_type_name(ctx, expr);
        if (resolved != NULL)
            return resolved;
        if (expr->data.member.object != NULL && expr->data.member.name != NULL) {
            const char *obj_type = infer_expression_type_name(ctx, expr->data.member.object);
            if (obj_type != NULL) {
                ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                if (obj_decl != NULL) {
                    for (size_t fi = 0; fi < obj_decl->data.class_decl.field_count; fi++) {
                        ClassField *f = obj_decl->data.class_decl.fields[fi];
                        if (f != NULL && f->name != NULL && f->type != NULL
                            && strcmp(f->name, expr->data.member.name) == 0) {
                            char *ft = render_type_name(f->type);
                            if (ft != NULL) {
                                static char mbuf[128];
                                snprintf(mbuf, sizeof(mbuf), "%s", ft);
                                free(ft);
                                return mbuf;
                            }
                        }
                    }
                }
            }
        }
        return "Int";
    }
    case AST_BINARY: {
        PgyTokenType op = expr->data.binary.op.type;
        const char *left_type = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *right_type = infer_expression_type_name(ctx, expr->data.binary.right);
        if (op == TOKEN_PLUS) {
            if ((left_type != NULL && strcmp(left_type, "String") == 0)
                || (right_type != NULL && strcmp(right_type, "String") == 0)) {
                return "String";
            }
            {
                ASTNode *cursor = expr->data.binary.left;
                while (cursor != NULL && cursor->type == AST_BINARY
                       && cursor->data.binary.op.type == TOKEN_PLUS) {
                    cursor = cursor->data.binary.left;
                }
                if (cursor != NULL) {
                    const char *leaf_type = infer_expression_type_name(ctx, cursor);
                    if (leaf_type != NULL && strcmp(leaf_type, "String") == 0)
                        return "String";
                }
            }
            return "Int";
        }
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return "Bool";
        }
        return "Int";
    }
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.name != NULL) {
            ASTNode *receiver = expr->data.call.callee->data.member.object;
            const char *method_name = expr->data.call.callee->data.member.name;
            const char *receiver_type = infer_expression_type_name(ctx, receiver);
            if (receiver_type != NULL && method_name != NULL) {
                if ((strncmp(receiver_type, "Slot<", 5) == 0
                     || strncmp(receiver_type, "SecureSlot<", 11) == 0
                     || strncmp(receiver_type, "ReadView<", 9) == 0
                     || strncmp(receiver_type, "WriteView<", 10) == 0)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if ((strncmp(receiver_type, "DeviceSlot<", 11) == 0)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if (((strncmp(receiver_type, "Slot<", 5) == 0
                      || strncmp(receiver_type, "SecureSlot<", 11) == 0
                      || strncmp(receiver_type, "ReadView<", 9) == 0
                      || strncmp(receiver_type, "WriteView<", 10) == 0
                      || strncmp(receiver_type, "DeviceSlot<", 11) == 0))
                    && (strcmp(method_name, "Write") == 0
                        || strcmp(method_name, "Release") == 0)) {
                    return "Void";
                }
            }
            ASTNode *method_decl = NULL;
            receiver_type = resolve_nominal_host_expr_type_name(ctx, receiver);
            if (receiver_type != NULL) {
                method_decl = find_nominal_host_method_decl(ctx, receiver_type,
                    method_name);
            }
            if (method_decl != NULL && method_decl->type == AST_FUNC_DECL
                && method_decl->data.func_decl.return_type != NULL) {
                char *resolved = render_type_name(method_decl->data.func_decl.return_type);
                snprintf(inferred_call_type, sizeof(inferred_call_type), "%s", resolved);
                free(resolved);
                return inferred_call_type;
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *name = expr->data.call.callee->data.identifier.name;
            if (strcmp(name, "MapNew") == 0)
                return "HashMap";
            if (strcmp(name, "MapGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *map_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0)
                    return constructed_arg_name_at(map_type, 1);
                return "Int";
            }
            if (strcmp(name, "ToString") == 0)
                return "String";
            if (strcmp(name, "IntentLastTrace") == 0
                || strcmp(name, "IntentLastFailure") == 0
                || strcmp(name, "IntentLastName") == 0
                || strcmp(name, "IntentHistoryStepName") == 0
                || strcmp(name, "IntentHistoryStepZone") == 0
                || strcmp(name, "IntentHistoryStepPhase") == 0
                || strcmp(name, "IntentHistoryStepParticipant") == 0
                || strcmp(name, "IntentHistoryStepSlot") == 0
                || strcmp(name, "IntentHistoryStepFromZone") == 0
                || strcmp(name, "IntentHistoryStepFromSlot") == 0
                || strcmp(name, "IntentHistoryStepToZone") == 0
                || strcmp(name, "IntentHistoryStepToSlot") == 0
                || strcmp(name, "IntentActiveName") == 0
                || strcmp(name, "IntentActiveFailure") == 0
                || strcmp(name, "IntentActiveTrace") == 0)
                return "String";
            if (strcmp(name, "IntentLastHandle") == 0
                || strcmp(name, "IntentLastTraceId") == 0
                || strcmp(name, "IntentLastStepCount") == 0
                || strcmp(name, "IntentHistoryCount") == 0
                || strcmp(name, "IntentActiveCount") == 0
                || strcmp(name, "IntentActiveHandle") == 0
                || strcmp(name, "IntentActiveParentHandle") == 0
                || strcmp(name, "IntentActiveTraceId") == 0
                || strcmp(name, "IntentActivePriority") == 0
                || strcmp(name, "IntentActiveSubjectCount") == 0
                || strcmp(name, "IntentActiveStepCount") == 0)
                return "Int";
            if (strcmp(name, "ListNew") == 0)
                return "List";
            if (strcmp(name, "ListGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *list_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                    return slot_inner_type_name(list_type);
                return "Int";
            }
            if (strcmp(name, "ListSize") == 0)
                return "Int";
            if (strcmp(name, "SetNew") == 0)
                return "Set";
            if (strcmp(name, "QueueNew") == 0)
                return "Queue";
            if (strcmp(name, "FsmNew") == 0)
                return "Fsm";
            if (strcmp(name, "TimerNew") == 0)
                return "Timer";
            if (strcmp(name, "CooldownNew") == 0)
                return "Cooldown";
            if (strcmp(name, "ClaimQubit") == 0)
                return "QubitSlot";
            if (strcmp(name, "ClaimDeviceSlot") == 0)
                return "DeviceSlot<Int>";
            if (strcmp(name, "ViewRead") == 0 && expr->data.call.arg_count >= 1) {
                static char read_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                snprintf(read_view, sizeof(read_view), "ReadView<%s>",
                    slot_inner_type_name(slot_type));
                return read_view;
            }
            if (strcmp(name, "ViewWrite") == 0 && expr->data.call.arg_count >= 1) {
                static char write_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                snprintf(write_view, sizeof(write_view), "WriteView<%s>",
                    slot_inner_type_name(slot_type));
                return write_view;
            }
            if (strcmp(name, "Measure") == 0 || strcmp(name, "QubitState") == 0)
                return "Int";
            if (strcmp(name, "Read") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "Slot<", 5) == 0
                    || strncmp(slot_type, "SecureSlot<", 11) == 0
                    || strncmp(slot_type, "ReadView<", 9) == 0
                    || strncmp(slot_type, "WriteView<", 10) == 0
                    || strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    return slot_inner_type_name(slot_type);
                }
            }
            if ((strcmp(name, "Write") == 0 || strcmp(name, "Release") == 0)
                && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "Slot<", 5) == 0
                    || strncmp(slot_type, "SecureSlot<", 11) == 0
                    || strncmp(slot_type, "ReadView<", 9) == 0
                    || strncmp(slot_type, "WriteView<", 10) == 0
                    || strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    return "Void";
                }
            }
            if (strcmp(name, "DeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0)
                    return slot_inner_type_name(slot_type);
            }
            if (strcmp(name, "SubmitDeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    static char device_future[128];
                    snprintf(device_future, sizeof(device_future), "RemoteFuture<%s>",
                        slot_inner_type_name(slot_type));
                    return device_future;
                }
                return "RemoteFuture<Int>";
            }
            if (strcmp(name, "IsCollapsed") == 0
                || strcmp(name, "IntoClassical") == 0)
                return "Bool";
            if (strcmp(name, "H") == 0
                || strcmp(name, "ArrayPush") == 0
                || strcmp(name, "ArraySet") == 0
                || strcmp(name, "ArrayPop") == 0
                || strcmp(name, "ChannelClose") == 0)
                return "Void";
            if ((strcmp(name, "TryRecv") == 0
                 || strcmp(name, "RecvTimeout") == 0
                 || strcmp(name, "TrySendStatus") == 0
                 || strcmp(name, "SendTimeoutStatus") == 0)
                && expr->data.call.arg_count >= 1) {
                static char opt_buf[128];
                if (strcmp(name, "TrySendStatus") == 0
                    || strcmp(name, "SendTimeoutStatus") == 0) {
                    snprintf(opt_buf, sizeof(opt_buf), "Option<Bool>");
                } else {
                    const char *inner = channel_inner_type_name(ctx,
                        expr->data.call.arguments[0]);
                    snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                }
                return opt_buf;
            }
            if (strcmp(name, "TrySend") == 0
                || strcmp(name, "SendTimeout") == 0
                || strcmp(name, "Cancel") == 0
                || strcmp(name, "IsCancelled") == 0
                || strcmp(name, "IntentLastFailed") == 0
                || strcmp(name, "IntentHistoryStepOk") == 0
                || strcmp(name, "IntentActiveConcurrent") == 0
                || strcmp(name, "IntentActiveFailed") == 0
                || strcmp(name, "MapHas") == 0
                || strcmp(name, "HasProjection") == 0
                || strcmp(name, "HasLayer") == 0
                || strcmp(name, "HasState") == 0
                || strcmp(name, "HasZone") == 0
                || strcmp(name, "HasZoneProjection") == 0
                || strcmp(name, "HasZoneLayer") == 0
                || strcmp(name, "HasZoneState") == 0
                || strcmp(name, "ChannelFull") == 0
                || strcmp(name, "ChannelClosed") == 0
                || strcmp(name, "ChannelReady") == 0)
                return "Bool";
            if (strcmp(name, "ChannelLength") == 0
                || strcmp(name, "ChannelCapacity") == 0
                || strcmp(name, "ChannelSpace") == 0)
                return "Int";
            if (strcmp(name, "Some") == 0 && expr->data.call.arg_count == 1) {
                static char opt_buf[128];
                const char *inner = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                return opt_buf;
            }
            if (strcmp(name, "None") == 0)
                return "Option<Int>";
            if ((strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0)
                && expr->data.call.arg_count == 1)
                return "Bool";
            if (strcmp(name, "UnwrapOption") == 0 && expr->data.call.arg_count == 1) {
                const char *opt_type = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                if (strncmp(opt_type, "Option<", 7) == 0)
                    return slot_inner_type_name(opt_type);
            }
            if (strcmp(name, "ToTObject") == 0 && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER
                && expr->data.call.arguments[0]->data.identifier.name != NULL) {
                return expr->data.call.arguments[0]->data.identifier.name;
            }
            if (strcmp(name, "ToObject") == 0 && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER
                && expr->data.call.arguments[0]->data.identifier.name != NULL) {
                return expr->data.call.arguments[0]->data.identifier.name;
            }
            if (find_class_decl(ctx, name) != NULL
                || find_subject_host_decl(ctx, name) != NULL
                || find_relation_decl(ctx, name) != NULL
                || find_effect_decl(ctx, name) != NULL
                || find_zone_decl(ctx, name) != NULL
                || find_world_decl(ctx, name) != NULL) {
                return name;
            }
            if (find_intent_decl(ctx, name) != NULL)
                return "Bool";

            {
                ASTNode *host_method = current_host_method_decl(ctx, name);
                if (host_method != NULL
                    && host_method->type == AST_FUNC_DECL
                    && host_method->data.func_decl.return_type != NULL) {
                    char *resolved = render_type_name(host_method->data.func_decl.return_type);
                    snprintf(inferred_call_type, sizeof(inferred_call_type), "%s", resolved);
                    free(resolved);
                    return inferred_call_type;
                }
            }

            {
                ASTNode *decl = find_function_decl(ctx, name);
                if (decl != NULL && decl->data.func_decl.return_type != NULL) {
                    char *resolved = NULL;
                    if (func_has_generic_params(decl)) {
                        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
                        size_t binding_count = 0;
                        if (infer_generic_call_bindings(ctx, decl, expr, bindings, &binding_count))
                            resolved = render_type_name_with_bindings(ctx,
                                decl->data.func_decl.return_type, bindings, binding_count);
                    }
                    if (resolved == NULL)
                        resolved = render_type_name(decl->data.func_decl.return_type);
                    snprintf(inferred_call_type, sizeof(inferred_call_type), "%s", resolved);
                    free(resolved);
                    return inferred_call_type;
                }
            }
        }
        return "Int";
    case AST_UNARY:
        if (expr->data.unary.op.type == TOKEN_NOT)
            return "Bool";
        return infer_expression_type_name(ctx, expr->data.unary.operand);
    default:
        return "Int";
    }
}

static char *
infer_spawn_return_type_name(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && target->data.call.callee != NULL
        && target->data.call.callee->type == AST_IDENTIFIER) {
        call = target;
        function_name = target->data.call.callee->data.identifier.name;
    } else if (target->type == AST_IDENTIFIER) {
        function_name = target->data.identifier.name;
    } else if (target->type == AST_FUNC_DECL) {
        if (target->data.func_decl.return_type != NULL)
            return render_type_name(target->data.func_decl.return_type);
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && decl->data.func_decl.return_type != NULL) {
        if (call != NULL && func_has_generic_params(decl)) {
            GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
            size_t binding_count = 0;
            if (infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
                return render_type_name_with_bindings(ctx, decl->data.func_decl.return_type,
                    bindings, binding_count);
        }
        return render_type_name(decl->data.func_decl.return_type);
    }

    return pergyra_strdup("Void");
}

static bool
is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL) return false;
    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
    }
    return false;
}

static const char *
lookup_future_inner_type(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Void";

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Future<", 7) == 0)
            return slot_inner_type_name(type_name);
        if (type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0)
            return slot_inner_type_name(type_name);
    }

    if (expr->type == AST_SPAWN_EXPR) {
        char *inner = infer_spawn_return_type_name(ctx, expr);
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s", inner);
        free(inner);
        return buf;
    }

    return "Void";
}

static const char *
pergyra_ast_type_to_c(ASTNode *type_node)
{
    static char mapped[128];
    if (type_node == NULL)
        return "void";

    if (type_node->type == AST_EVENT_HANDLER_TYPE)
        return "void *";

    char *type_name = render_type_name(type_node);
    snprintf(mapped, sizeof(mapped), "%s", pergyra_type_to_c(type_name));
    free(type_name);
    return mapped;
}

static char *
pergyra_ast_typed_declarator(ASTNode *type_node, const char *name)
{
    if (type_node == NULL)
        return strdup_fmt("void %s", name != NULL ? name : "value");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        char *ret_type = pergyra_strdup("void");
        CodeBuf *params = codebuf_create();
        char *result;

        if (type_node->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                type_node->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (params == NULL) {
            result = strdup_fmt("%s (*%s)(void)", ret_type,
                name != NULL ? name : "value");
            free(ret_type);
            return result;
        }

        if (type_node->data.event_handler_type.param_count == 0) {
            codebuf_write(params, "void");
        } else {
            for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(params, ", ");
                codebuf_write(params, "%s",
                    pergyra_ast_type_to_c(
                        type_node->data.event_handler_type.param_types[i]));
            }
        }

        result = strdup_fmt("%s (*%s)(%s)", ret_type,
            name != NULL ? name : "value", params->data);
        codebuf_destroy(params);
        free(ret_type);
        return result;
    }

    return strdup_fmt("%s %s", pergyra_ast_type_to_c(type_node),
        name != NULL ? name : "value");
}

static char *
pergyra_func_pointer_declarator_from_decl(ASTNode *func_decl, const char *name)
{
    CodeBuf *params = NULL;
    char *ret_type = NULL;
    char *result = NULL;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return strdup_fmt("void (*%s)(void)", name != NULL ? name : "value");

    params = codebuf_create();
    ret_type = pergyra_strdup("void");
    if (func_decl->data.func_decl.return_type != NULL) {
        const char *rendered = pergyra_ast_type_to_c(func_decl->data.func_decl.return_type);
        free(ret_type);
        ret_type = pergyra_strdup(rendered);
    }

    if (params == NULL) {
        result = strdup_fmt("%s (*%s)(void)", ret_type,
            name != NULL ? name : "value");
        free(ret_type);
        return result;
    }

    if (func_decl->data.func_decl.param_count == 0) {
        codebuf_write(params, "void");
    } else {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *p = func_decl->data.func_decl.params[i];
            if (i > 0)
                codebuf_write(params, ", ");
            codebuf_write(params, "%s",
                p != NULL && p->type != NULL
                    ? pergyra_ast_type_to_c(p->type)
                    : "int32_t");
        }
    }

    result = strdup_fmt("%s (*%s)(%s)", ret_type,
        name != NULL ? name : "value", params->data);
    codebuf_destroy(params);
    free(ret_type);
    return result;
}

static char *
pergyra_func_signature_declarator(ASTNode *return_type, const char *name,
                                  const char *params_sig)
{
    const char *fn_name = name != NULL ? name : "value";
    const char *sig = (params_sig != NULL && params_sig[0] != '\0')
        ? params_sig : "void";

    if (return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE) {
        CodeBuf *handler_params = codebuf_create();
        char *ret_type = pergyra_strdup("void");
        char *result;

        if (return_type->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                return_type->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (handler_params == NULL) {
            result = strdup_fmt("%s (*%s(%s))(void)", ret_type, fn_name, sig);
            free(ret_type);
            return result;
        }

        if (return_type->data.event_handler_type.param_count == 0) {
            codebuf_write(handler_params, "void");
        } else {
            for (size_t i = 0; i < return_type->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(handler_params, ", ");
                codebuf_write(handler_params, "%s",
                    pergyra_ast_type_to_c(
                        return_type->data.event_handler_type.param_types[i]));
            }
        }

        result = strdup_fmt("%s (*%s(%s))(%s)", ret_type, fn_name, sig,
            handler_params->data);
        codebuf_destroy(handler_params);
        free(ret_type);
        return result;
    }

    return strdup_fmt("%s %s(%s)",
        pergyra_ast_type_to_c(return_type), fn_name, sig);
}

static bool
transpiler_can_forward_declare_type_early(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return true;

    const char *name = type_node->data.type.name;
    if (strcmp(name, "Int") == 0
        || strcmp(name, "Float") == 0
        || strcmp(name, "Bool") == 0
        || strcmp(name, "String") == 0
        || strcmp(name, "Char") == 0
        || strcmp(name, "Byte") == 0
        || strcmp(name, "Void") == 0
        || strcmp(name, "Qubit") == 0)
        return true;

    if (strcmp(name, "Result") == 0
        || strcmp(name, "Option") == 0
        || strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strcmp(name, "RemoteFuture") == 0
        || strcmp(name, "Array") == 0
        || strcmp(name, "Slice") == 0
        || strcmp(name, "Channel") == 0
        || strcmp(name, "Box") == 0
        || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0
        || strcmp(name, "Future") == 0)
        return true;

    return find_class_decl(ctx, name) != NULL;
}

static bool
transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_early(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_early(ctx, p->type))
            return false;
    }
    return true;
}

static bool
transpiler_can_forward_declare_type_after_zones(TranspilerCtx *ctx, ASTNode *type_node)
{
    const char *name = NULL;

    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return true;
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL)
        return false;

    name = type_node->data.type.name;
    if (find_world_decl(ctx, name) != NULL)
        return false;
    return transpiler_has_known_nominal_type(ctx, name);
}

static bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_after_zones(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type))
            return false;
    }
    return true;
}

static void
append_mangled_type_name(CodeBuf *buf, const char *type_name)
{
    bool wrote = false;

    for (const unsigned char *p = (const unsigned char *)type_name; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9')) {
            codebuf_write(buf, "%c", *p);
            wrote = true;
        } else if (wrote && buf->len > 0 && buf->data[buf->len - 1] != '_') {
            codebuf_write(buf, "_");
        }
    }

    if (!wrote)
        codebuf_write(buf, "Type");
}

static int
find_generic_param_index(ASTNode *decl, const char *name)
{
    if (!func_has_generic_params(decl) || name == NULL)
        return -1;

    for (size_t i = 0; i < decl->data.func_decl.generic_params->count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL && strcmp(param->name, name) == 0)
            return (int)i;
    }

    return -1;
}

static bool
infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count)
{
    if (!func_has_generic_params(decl)
        || call == NULL
        || call->type != AST_CALL
        || bindings == NULL
        || binding_count == NULL) {
        return false;
    }

    size_t generic_count = decl->data.func_decl.generic_params->count;
    memset(bindings, 0, sizeof(GenericBindingEntry) * generic_count);

    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL) {
            strncpy(bindings[i].name, param->name, sizeof(bindings[i].name) - 1);
            bindings[i].name[sizeof(bindings[i].name) - 1] = '\0';
        }
    }

    for (size_t i = 0; i < decl->data.func_decl.param_count && i < call->data.call.arg_count; i++) {
        FuncParam *param = decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE)
            continue;
        if (param->type->data.type.generic_args != NULL)
            continue;

        int generic_index = find_generic_param_index(decl, param->type->data.type.name);
        if (generic_index < 0)
            continue;

        const char *arg_type = infer_expression_type_name(ctx, call->data.call.arguments[i]);
        if (arg_type == NULL)
            continue;

        if (bindings[generic_index].concrete_type[0] != '\0'
            && strcmp(bindings[generic_index].concrete_type, arg_type) != 0) {
            return false;
        }

        strncpy(bindings[generic_index].concrete_type, arg_type,
            sizeof(bindings[generic_index].concrete_type) - 1);
        bindings[generic_index].concrete_type[sizeof(bindings[generic_index].concrete_type) - 1] = '\0';
    }

    for (size_t i = 0; i < generic_count; i++) {
        if (bindings[i].name[0] == '\0' || bindings[i].concrete_type[0] == '\0')
            return false;
    }

    *binding_count = generic_count;
    return true;
}

static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_hosted_method_forward_decl_named(const char *host_name, ASTNode *method,
                                      bool pointer_self, CodeBuf *buf,
                                      TranspilerCtx *ctx)
{
    if (host_name == NULL || method == NULL || buf == NULL || ctx == NULL
        || method->type != AST_FUNC_DECL)
        return;

    const char *method_name = method->data.func_decl.name;
    const char *ret_type = "void";
    if (method_name == NULL)
        return;
    ensure_type_specializations_from_ast(ctx, method->data.func_decl.return_type);
    if (method->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

    codebuf_write(buf, "\n%s\n%s_%s(%s%s",
                  ret_type, host_name, method_name, host_name,
                  pointer_self ? " *self" : " self");

    for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
        FuncParam *p = method->data.func_decl.params[j];
        if (p == NULL || p->name == NULL)
            continue;
        if (strcmp(p->name, "self") == 0)
            continue;

        const char *pt = NULL;
        char surface_desc[256];
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        snprintf(surface_desc, sizeof(surface_desc),
            "hosted method parameter '%s.%s(%s)'",
            host_name != NULL ? host_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)",
            p->name != NULL ? p->name : "(anonymous)");
        pt = transpiler_require_ast_c_type(ctx, p->type, surface_desc);
        if (pt == NULL)
            return;
        {
            char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
            bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
            if (subj_param)
                codebuf_write(buf, ", %s *%s", pt, p->name);
            else
                codebuf_write(buf, ", %s %s", pt, p->name);
            free(ptn);
        }
    }
    codebuf_write(buf, ");\n");
}

static const char *
ensure_generic_specialization(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (!infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
        return NULL;

    CodeBuf *name_buf = codebuf_create();
    if (name_buf == NULL)
        return NULL;

    codebuf_write(name_buf, "%s", decl->data.func_decl.name);
    for (size_t i = 0; i < binding_count; i++) {
        codebuf_write(name_buf, "_");
        append_mangled_type_name(name_buf, bindings[i].concrete_type);
    }

    for (int i = 0; i < ctx->generic_specialization_count; i++) {
        GenericSpecializationEntry *entry = &ctx->generic_specializations[i];
        if (entry->func_decl == decl
            && strcmp(entry->specialized_name, name_buf->data) == 0) {
            codebuf_destroy(name_buf);
            return entry->specialized_name;
        }
    }

    if (ctx->generic_specialization_count >= MAX_GENERIC_SPECIALIZATIONS) {
        codebuf_destroy(name_buf);
        return NULL;
    }

    GenericSpecializationEntry *entry =
        &ctx->generic_specializations[ctx->generic_specialization_count++];
    memset(entry, 0, sizeof(*entry));
    entry->func_decl = decl;
    strncpy(entry->specialized_name, name_buf->data,
        sizeof(entry->specialized_name) - 1);
    entry->specialized_name[sizeof(entry->specialized_name) - 1] = '\0';
    entry->emitting = true;
    codebuf_destroy(name_buf);

    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    emit_func_forward_decl_named(decl, entry->specialized_name, ctx->decls, ctx);
    emit_func_decl_named(decl, entry->specialized_name, ctx->helpers, ctx);

    ctx->generic_binding_count = saved_binding_count;
    entry->emitting = false;
    return entry->specialized_name;
}

