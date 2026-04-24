Type *
create_overlay_nominal_type(const char *name)
{
    Type *type = calloc(1, sizeof(Type));
    if (type == NULL)
        return TYPE_UNKNOWN;
    type->kind = TYPE_KIND_CLASS;
    type->nominal_flavor = TYPE_NOMINAL_CLASS;
    type->name = pergyra_strdup(name);
    return type;
}

static size_t
overlay_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    switch (decl->type) {
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.party_count
            + decl->data.roster_decl.shared_count;
    case AST_WORLD_DECL:
        return decl->data.world_decl.roster_count
            + decl->data.world_decl.zone_count
            + decl->data.world_decl.shared_count;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.slot_count
            + decl->data.zone_decl.shared_count;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.slot_count
            + decl->data.relation_decl.shared_count;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.slot_count
            + decl->data.effect_decl.shared_count;
    default:
        return 0;
    }
}

static ASTNode *
overlay_field_decl_at(ASTNode *decl, size_t index, const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_ROSTER_DECL:
        if (index < decl->data.roster_decl.party_count)
            return NULL;
        index -= decl->data.roster_decl.party_count;
        if (index < decl->data.roster_decl.shared_count) {
            ASTNode *shared = decl->data.roster_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_WORLD_DECL:
        if (index < decl->data.world_decl.roster_count)
            return NULL;
        index -= decl->data.world_decl.roster_count;
        if (index < decl->data.world_decl.zone_count)
            return NULL;
        index -= decl->data.world_decl.zone_count;
        if (index < decl->data.world_decl.shared_count) {
            ASTNode *shared = decl->data.world_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_ZONE_DECL:
        if (index < decl->data.zone_decl.slot_count) {
            ASTNode *slot = decl->data.zone_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.zone_decl.slot_count;
        if (index < decl->data.zone_decl.shared_count) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_RELATION_DECL:
        if (index < decl->data.relation_decl.slot_count) {
            ASTNode *slot = decl->data.relation_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.relation_decl.slot_count;
        if (index < decl->data.relation_decl.shared_count) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_EFFECT_DECL:
        if (index < decl->data.effect_decl.slot_count) {
            ASTNode *slot = decl->data.effect_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.effect_decl.slot_count;
        if (index < decl->data.effect_decl.shared_count) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    default:
        break;
    }

    return NULL;
}

ASTNode *
current_host_decl(SemanticContext *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_nominal_decl != NULL)
        return ctx->current_nominal_decl;
    if (ctx->current_relation != NULL)
        return ctx->current_relation;
    if (ctx->current_effect != NULL)
        return ctx->current_effect;
    if (ctx->current_zone != NULL)
        return ctx->current_zone;
    if (ctx->current_world != NULL)
        return ctx->current_world;
    return NULL;
}

static Type *
current_host_field_type(SemanticContext *ctx, const char *field_name)
{
    ASTNode *decl = current_host_decl(ctx);
    if (decl == NULL || field_name == NULL)
        return NULL;

    if (decl->type == AST_CLASS_DECL) {
        size_t field_count = projection_source_field_count(decl);
        for (size_t i = 0; i < field_count; i++) {
            ClassField *field = projection_source_field_at(decl, i);
            if (field != NULL && field->name != NULL
                && strcmp(field->name, field_name) == 0) {
                return resolve_type_node(field->type, ctx);
            }
        }
        return NULL;
    }

    if (decl->type == AST_WORLD_DECL) {
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *slot = decl->data.world_decl.rosters[i];
            if (slot != NULL && slot->data.world_roster.slot_name != NULL
                && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                return resolve_named_type(slot->data.world_roster.roster_type,
                    ctx, slot);
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *slot = decl->data.world_decl.zones[i];
            if (slot != NULL && slot->data.world_zone.slot_name != NULL
                && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                return resolve_named_type(slot->data.world_zone.zone_type, ctx, slot);
            }
        }
    }

    for (size_t i = 0; i < overlay_field_count(decl); i++) {
        const char *name = NULL;
        ASTNode *type_node = overlay_field_decl_at(decl, i, &name);
        if (name != NULL && strcmp(name, field_name) == 0)
            return resolve_type_node(type_node, ctx);
    }

    return NULL;
}

static ASTNode *
current_host_method_decl(SemanticContext *ctx, const char *method_name)
{
    ASTNode *decl = current_host_decl(ctx);
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (decl == NULL || method_name == NULL)
        return NULL;

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
        return NULL;
    }

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static Type *
type_check_host_method_call(ASTNode *expr, ASTNode *method, SemanticContext *ctx)
{
    size_t implicit_self = 0;
    size_t expected;
    size_t provided;
    uint32_t declared_effects;

    if (expr == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return TYPE_UNKNOWN;

    declared_effects = declared_effects_from_function_node(method, ctx, NULL);
    semantic_record_effect(ctx, declared_effects);
    if (ctx->in_parallel && type_effect_mask_has(declared_effects, EFFECT_SECURE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit calling secure-effect method '%s'; serialize authority-bearing operations outside the parallel block",
            method->data.func_decl.name != NULL ? method->data.func_decl.name : "<method>");
        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    if (method->data.func_decl.param_count > 0
        && method->data.func_decl.params[0] != NULL
        && method->data.func_decl.params[0]->name != NULL
        && strcmp(method->data.func_decl.params[0]->name, "self") == 0
        && method->data.func_decl.params[0]->type == NULL) {
        implicit_self = 1;
    }

    expected = method->data.func_decl.param_count - implicit_self;
    provided = expr->data.call.arg_count;
    if (provided != expected) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu",
            method->data.func_decl.name != NULL ? method->data.func_decl.name : "<method>",
            (unsigned long long) expected, (unsigned long long) provided);
        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    for (size_t i = 0; i < provided; i++) {
        FuncParam *param = method->data.func_decl.params[i + implicit_self];
        Type *param_type = (param != NULL && param->type != NULL)
            ? resolve_type_node(param->type, ctx)
            : TYPE_UNKNOWN;
        Type *arg_type = type_check_expression(expr->data.call.arguments[i], ctx);
        if (param_type != NULL && arg_type != NULL
            && !type_is_assignable(arg_type, param_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_CALL_ARG_TYPE_MISMATCH, PGY_FIX_ALIGN_ARG_TYPE,
                expr->data.call.arguments[i],
                "Argument %llu for '%s' expects '%s', got '%s'",
                (unsigned long long) (i + 1),
                method->data.func_decl.name != NULL ? method->data.func_decl.name : "<method>",
                param_type->name != NULL ? param_type->name : "<type>",
                arg_type->name != NULL ? arg_type->name : "<type>");
        }
    }

    if (method->data.func_decl.return_type != NULL)
        return resolve_type_node(method->data.func_decl.return_type, ctx);
    return TYPE_VOID;
}

static ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return find_type_decl_by_name(program, name);
    case SYMBOL_ROSTER:
        return find_domain_decl_by_name(program, AST_ROSTER_DECL, name);
    case SYMBOL_WORLD:
        return find_domain_decl_by_name(program, AST_WORLD_DECL, name);
    case SYMBOL_ZONE:
        return find_domain_decl_by_name(program, AST_ZONE_DECL, name);
    case SYMBOL_RELATION:
        return find_domain_decl_by_name(program, AST_RELATION_DECL, name);
    case SYMBOL_EFFECT:
        return find_domain_decl_by_name(program, AST_EFFECT_DECL, name);
    default:
        return NULL;
    }
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_subject_host_slot_handle(const Type *type, SemanticContext *ctx)
{
    if (!type_is_owned_slot_handle(type) || ctx == NULL)
        return false;
    return type_is_subject_type(type->data.slot.inner_type, ctx);
}

static bool
type_is_class_object_type(const Type *type, SemanticContext *ctx)
{
    return type_is_subject_type(type, ctx);
}

static bool
type_is_nominal_host_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL
        || (type->kind != TYPE_KIND_CLASS && type->kind != TYPE_KIND_ENUM)
        || type->name == NULL || ctx == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl != NULL && decl->type == AST_ENUM_DECL)
        return true;
    if (decl != NULL && decl->type == AST_CLASS_DECL)
        return !decl->data.class_decl.is_struct
            || decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL
            || decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
    return false;
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL || type->kind != TYPE_KIND_CLASS
        || type->name == NULL || ctx == NULL)
        return false;

    if (type->nominal_flavor == TYPE_NOMINAL_SUBJECT)
        return true;

    decl = find_subject_host_decl_by_name(ctx->program_root, type->name);
    return decl_is_subject_host(decl);
}

static const char *
find_action_binding_type_name(ASTNode *func, ASTNode *enclosing_nominal,
                              SemanticContext *ctx, const char *binding_name)
{
    if (func == NULL || func->type != AST_FUNC_DECL || ctx == NULL
        || binding_name == NULL) {
        return NULL;
    }

    if (strcmp(binding_name, "self") == 0) {
        return enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && enclosing_nominal->data.class_decl.name != NULL
                ? enclosing_nominal->data.class_decl.name
                : NULL;
    }

    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *param = func->data.func_decl.params[i];
        Type *param_type;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, binding_name) != 0) {
            continue;
        }
        if (param->type == NULL)
            return NULL;
        param_type = resolve_type_node(param->type, ctx);
        if (param_type == NULL || !type_is_subject_type(param_type, ctx))
            return NULL;
        return param_type->name;
    }

    return NULL;
}

bool
domain_has_subject_slot_type(ASTNode **slots, size_t slot_count,
                             SemanticContext *ctx, const char *type_name)
{
    if (slots == NULL || ctx == NULL || type_name == NULL)
        return false;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.type == NULL) {
            continue;
        }
        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
type_is_value_nominal_boundary_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;
    TypeNominalFlavor flavor;

    if (type == NULL || ctx == NULL || ctx->program_root == NULL)
        return false;
    if (type_is_subject_type(type, ctx))
        return false;
    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_ENUM:
        return true;
    case TYPE_KIND_TUPLE:
        return true;
    case TYPE_KIND_CONSTRUCTED:
        return !type_is_movable_resource_handle(type);
    case TYPE_KIND_CLASS:
        break;
    default:
        return false;
    }
    if (type->name == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return false;

    flavor = nominal_flavor_from_decl(decl);
    return flavor == TYPE_NOMINAL_CLASS
        || flavor == TYPE_NOMINAL_STRUCT
        || flavor == TYPE_NOMINAL_OBJECT
        || flavor == TYPE_NOMINAL_TOBJECT
        || flavor == TYPE_NOMINAL_VESSEL;
}

static bool
zone_has_authority_for_subject_type(ASTNode *zone, SemanticContext *ctx,
                                    const char *type_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || ctx == NULL
        || type_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone,
            authority->data.zone_authority.subject_slot_name);
        if (slot == NULL || slot->data.domain_slot.type == NULL)
            continue;
        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

bool
zone_has_effect_layer_type(ASTNode *zone, const char *effect_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || effect_name == NULL)
        return false;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.layer_type == NULL
            || slot->data.zone_layer_slot.is_relation) {
            continue;
        }
        if (strcmp(slot->data.zone_layer_slot.layer_type, effect_name) == 0)
            return true;
    }

    return false;
}

static bool
expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *decl;

    if (expr == NULL || expr->type != AST_CALL
        || expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER
        || expr->data.call.callee->data.identifier.name == NULL
        || ctx == NULL) {
        return false;
    }

    decl = find_type_decl_by_name(ctx->program_root,
        expr->data.call.callee->data.identifier.name);
    if (decl != NULL)
        return !decl->data.class_decl.is_struct;
    return false;
}

static bool
expr_is_qubit_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimQubit") == 0;
}

static bool
expr_is_device_slot_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimDeviceSlot") == 0;
}

static bool
expr_is_movable_resource_transfer_source(const ASTNode *expr)
{
    if (expr == NULL)
        return false;

    switch (expr->type) {
    case AST_IDENTIFIER:
    case AST_CALL:
    case AST_CHANNEL_RECV:
    case AST_AWAIT_EXPR:
        return true;
    default:
        return false;
    }
}

static bool
slot_transfer_compatible(const Type *from, const Type *to)
{
    if (from == NULL || to == NULL)
        return false;
    if (from->kind != TYPE_KIND_SLOT || to->kind != TYPE_KIND_SLOT)
        return false;
    if (from->data.slot.access_mode != SLOT_ACCESS_MOVE_TOKEN)
        return false;
    if (to->data.slot.access_mode != SLOT_ACCESS_OWNED)
        return false;
    if (from->data.slot.is_secure != to->data.slot.is_secure)
        return false;
    return type_is_assignable(from->data.slot.inner_type, to->data.slot.inner_type)
        && type_is_assignable(to->data.slot.inner_type, from->data.slot.inner_type);
}

static bool
expr_is_movable_resource_boundary(const ASTNode *expr)
{
    return expr != NULL
        && (expr->type == AST_CHANNEL_RECV
            || expr->type == AST_AWAIT_EXPR);
}

static bool
expr_is_static_member_access(const ASTNode *expr)
{
    if (expr == NULL || expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL) {
        return false;
    }

    if (expr->data.member.object->type == AST_IDENTIFIER) {
        const char *name = expr->data.member.object->data.identifier.name;
        return name != NULL && name[0] >= 'A' && name[0] <= 'Z';
    }

    return expr_is_static_member_access(expr->data.member.object);
}

static char *
