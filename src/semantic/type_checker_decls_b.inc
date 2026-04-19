#include "type_checker_decls_intent_world.inc"

count_subject_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t subject_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.is_subject) {
            subject_count++;
        }
    }
    return subject_count;
}

static size_t
count_object_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t object_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && !slot->data.domain_slot.is_subject) {
            object_count++;
        }
    }
    return object_count;
}

static size_t
count_bindable_domain_slots(ASTNode **slots, size_t slot_count,
                            ASTNode **refreshes, size_t refresh_count)
{
    size_t bindable_count = 0;
    (void)refreshes;
    (void)refresh_count;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }
        bindable_count++;
    }

    return bindable_count;
}

static ASTNode *
find_nth_bindable_domain_slot(ASTNode **slots, size_t slot_count,
                              ASTNode **refreshes, size_t refresh_count,
                              size_t ordinal)
{
    size_t seen = 0;
    (void)refreshes;
    (void)refresh_count;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }

        if (seen == ordinal)
            return slot;
        seen++;
    }

    return NULL;
}

static bool
role_decl_has_ability(ASTNode *role, ASTNode *program,
                      ASTNode *ability_ref, int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL
        || ability_ref == NULL || depth > 16) {
        return false;
    }

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl != NULL
            && impl->type == AST_IMPL_ABILITY
            && impl->data.impl_ability.ability_ref != NULL
            && ability_ref_matches(program,
                impl->data.impl_ability.ability_ref, ability_ref)) {
            return true;
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = find_role_decl_in_program(program,
            inc != NULL ? inc->data.include_stmt.role_name : NULL);
        if (role_decl_has_ability(included, program, ability_ref, depth + 1))
            return true;
    }

    return false;
}

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, type_name) != 0) {
            continue;
        }
        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

static ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, type_name) != 0) {
            continue;
        }
        for (size_t j = 0; j < stmt->data.role_decl.impl_count; j++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[j];
            if (impl != NULL
                && impl->type == AST_IMPL_ABILITY
                && impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL
                && strcmp(impl->data.impl_ability.ability_ref->data.type.name, ability_name) == 0) {
                return impl->data.impl_ability.ability_ref;
            }
        }
    }

    return NULL;
}

static bool
any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

static ASTNode *
any_subject_role_find_base_ability_impl(ASTNode *program, const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        for (size_t j = 0; j < stmt->data.role_decl.impl_count; j++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[j];
            ASTNode *ability_ref;

            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            ability_ref = impl->data.impl_ability.ability_ref;
            if (ability_ref == NULL || ability_ref->type != AST_TYPE
                || ability_ref->data.type.name == NULL) {
                continue;
            }
            if (strcmp(ability_ref->data.type.name, ability_name) == 0)
                return ability_ref;
        }
    }

    return NULL;
}

static ASTNode *
find_domain_slot_local(ASTNode **slots, size_t slot_count, const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    return find_domain_slot_local(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, slot_name);
}

static ASTNode *
find_domain_decl_by_name(ASTNode *program,
                         ASTNodeType decl_type,
                         const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != decl_type)
            continue;

        const char *decl_name = NULL;
        if (decl_type == AST_RELATION_DECL)
            decl_name = stmt->data.relation_decl.name;
        else if (decl_type == AST_EFFECT_DECL)
            decl_name = stmt->data.effect_decl.name;
        else if (decl_type == AST_ZONE_DECL)
            decl_name = stmt->data.zone_decl.name;
        else if (decl_type == AST_WORLD_DECL)
            decl_name = stmt->data.world_decl.name;
        else if (decl_type == AST_ROSTER_DECL)
            decl_name = stmt->data.roster_decl.name;

        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
find_zone_effect_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && !slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_relation_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_authority(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        if (authority != NULL
            && authority->type == AST_ZONE_AUTHORITY
            && authority->data.zone_authority.subject_slot_name != NULL
            && strcmp(authority->data.zone_authority.subject_slot_name, slot_name) == 0) {
            return authority;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_state(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static bool
resolve_zone_effect_state(ASTNode *zone,
                          ASTNode *site,
                          const char *state_name,
                          SemanticContext *ctx,
                          const char *action_name,
                          const char **effect_slot_name,
                          const char **target_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (effect_slot_name != NULL)
        *effect_slot_name = NULL;
    if (target_slot_name != NULL)
        *target_slot_name = NULL;

    if (state == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (state->data.zone_state.is_relation) {
        semantic_error(ctx, site,
            "Zone %s state '%s' is a relation state and cannot be used as an effect state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (effect_slot_name != NULL)
        *effect_slot_name = state->data.zone_state.layer_slot_name;
    if (target_slot_name != NULL)
        *target_slot_name = state->data.zone_state.left_or_target_slot_name;
    return true;
}

static bool
resolve_zone_relation_state(ASTNode *zone,
                            ASTNode *site,
                            const char *state_name,
                            SemanticContext *ctx,
                            const char *action_name,
                            const char **relation_slot_name,
                            const char **left_slot_name,
                            const char **right_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (relation_slot_name != NULL)
        *relation_slot_name = NULL;
    if (left_slot_name != NULL)
        *left_slot_name = NULL;
    if (right_slot_name != NULL)
        *right_slot_name = NULL;

    if (state == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (!state->data.zone_state.is_relation) {
        semantic_error(ctx, site,
            "Zone %s state '%s' is an effect state and cannot be used as a relation state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (relation_slot_name != NULL)
        *relation_slot_name = state->data.zone_state.layer_slot_name;
    if (left_slot_name != NULL)
        *left_slot_name = state->data.zone_state.left_or_target_slot_name;
    if (right_slot_name != NULL)
        *right_slot_name = state->data.zone_state.right_slot_name;
    return true;
}

static bool
type_check_zone_participant_authority(ASTNode *zone,
                                ASTNode *site,
                                const char *participant_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *participant_slot;
    ASTNode *authority;

    if (zone == NULL || ctx == NULL || participant_slot_name == NULL)
        return false;

    participant_slot = find_zone_domain_slot(zone, participant_slot_name);
    if (participant_slot == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown authority subject slot '%s'",
            action_name, participant_slot_name);
        return true;
    }

    if (!participant_slot->data.domain_slot.is_subject) {
        semantic_error(ctx, site,
            "Zone %s authority '%s' must be a subject slot",
            action_name, participant_slot_name);
        return true;
    }

    authority = find_zone_authority(zone, participant_slot_name);
    if (zone->data.zone_decl.authority_count > 0 && authority == NULL) {
        semantic_error(ctx, site,
            "Zone %s participant '%s' is not declared in zone authority set",
            action_name, participant_slot_name);
        return true;
    }

    return true;
}

static ASTNode *
find_named_class_decl_local(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL
            || stmt->data.class_decl.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.class_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee;

    if (expr == NULL || ctx == NULL || expr->type != AST_CALL)
        return false;

    callee = expr->data.call.callee;
    if (callee == NULL)
        return false;

    if (callee->type == AST_IDENTIFIER && callee->data.identifier.name != NULL) {
        Symbol *sym = scope_lookup(ctx->scope, callee->data.identifier.name);
        return sym != NULL
            && sym->type != NULL
            && sym->type->kind == TYPE_KIND_FUNCTION
            && type_effect_mask_requires_authority(type_function_effects(sym->type));
    }

    if (callee->type == AST_MEMBER_ACCESS
        && callee->data.member.object != NULL
        && callee->data.member.name != NULL) {
        Type *object_type = type_check_expression(callee->data.member.object, ctx);
        ASTNode *class_decl;

        if (object_type == NULL || object_type->name == NULL)
            return false;

        class_decl = find_type_decl_by_name(ctx->program_root, object_type->name);
        if (class_decl == NULL || class_decl->type != AST_CLASS_DECL)
            return false;

        for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
            ASTNode *method = class_decl->data.class_decl.methods[i];
            if (method == NULL || method->type != AST_FUNC_DECL
                || method->data.func_decl.name == NULL) {
                continue;
            }
            if (strcmp(method->data.func_decl.name, callee->data.member.name) == 0) {
                uint32_t method_effects =
                    declared_effects_from_function_node(method, ctx, NULL);
                if (type_effect_mask_requires_authority(method_effects))
                    return true;
                if (method->data.func_decl.is_action
                    && (method->data.func_decl.within_zone != NULL
                        || method->data.func_decl.causes_effect != NULL)) {
                    return true;
                }
                return false;
            }
        }
    }

    return false;
}

static bool
type_check_projection_contract(ASTNode **slots,
                               size_t slot_count,
                               const char *owner_label,
                               ASTNode *site,
                               const char *object_slot_name,
                               const char *source_slot_name,
                               SemanticContext *ctx,
                               const char *action_name)
{
    ASTNode *object_slot;
    ASTNode *source_slot;
    bool requires_dto;
    Type *target_type;
    Type *source_type;
    ASTNode *target_decl;
    ASTNode *source_decl;

    object_slot = find_domain_slot_local(slots, slot_count, object_slot_name);
    source_slot = find_domain_slot_local(slots, slot_count, source_slot_name);
    if (object_slot == NULL || source_slot == NULL || ctx == NULL)
        return false;

    if (object_slot->data.domain_slot.is_subject
        || object_slot->data.domain_slot.is_vessel) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' is not a projection slot.\n"
            "Reason:\n"
            "- source slot '%s' is driving this %s path\n"
            "- %s only writes into object/tobject projection targets\n"
            "- subject/vessel slots are ownership anchors, not projection sinks\n"
            "- current owner surface is '%s'\n"
            "Fix:\n"
            "- choose an object slot for local projection sync\n"
            "- or choose a tobject slot for boundary publication",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            action_name,
            owner_label);
        return true;
    }
    requires_dto =
        site != NULL && site->type == AST_ZONE_REFRESH
        && (site->data.zone_refresh.derive_target_kind
            ? object_slot->data.domain_slot.is_tobject
            : site->data.zone_refresh.requires_dto);
    if (requires_dto && !object_slot->data.domain_slot.is_tobject) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' must be a tobject slot.\n"
            "Reason:\n"
            "- source slot '%s' is driving this %s path\n"
            "- publish writes a boundary transfer snapshot\n"
            "- object slots are local projections and cannot be published as transfer targets\n"
            "- current owner surface is '%s'\n"
            "Fix:\n"
            "- change '%s' to a tobject slot\n"
            "- or use refresh/bind into an object slot instead",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            owner_label,
            object_slot_name != NULL ? object_slot_name : "<slot>");
        return true;
    }
    target_type = resolve_type_node(object_slot->data.domain_slot.type, ctx);
    source_type = resolve_type_node(source_slot->data.domain_slot.type, ctx);
    if (target_type == NULL || source_type == NULL
        || target_type == TYPE_UNKNOWN || source_type == TYPE_UNKNOWN) {
        return true;
    }

    target_decl = find_named_class_decl_local(ctx->program_root, target_type->name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' does not use a projection declaration.\n"
            "Reason:\n"
            "- source slot '%s' is driving this %s path\n"
            "- projection sync requires an object/tobject nominal target\n"
            "- '%s' is not an object/tobject declaration\n"
            "- target slot '%s' is declared on %s '%s'\n"
            "Fix:\n"
            "- change the target slot type to an object or tobject declaration\n"
            "- or move this data through a plain assignment path instead",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            target_type->name != NULL ? target_type->name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_label);
        return true;
    }
    if (site != NULL && site->type == AST_ZONE_REFRESH) {
        NominalDeclKind expected_kind =
            requires_dto ? NOMINAL_DECL_TOBJECT : NOMINAL_DECL_OBJECT;
        const char *expected_label =
            requires_dto ? "tobject" : "object";

        if (target_decl->data.class_decl.nominal_kind != expected_kind) {
            semantic_error(ctx, site,
                "%s %s target slot '%s' uses the wrong projection kind.\n"
                "Reason:\n"
                "- source slot '%s' is driving this %s path\n"
                "- %s requires target slot '%s' to use %s declaration\n"
                "- actual target type '%s' uses a different projection kind\n"
                "- target projection declaration currently comes from '%s'\n"
                "Fix:\n"
                "- change slot '%s' to use %s declaration\n"
                "- or switch to the matching projection operation",
                owner_label, action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                action_name,
                action_name,
                object_slot_name != NULL ? object_slot_name : "<slot>",
                expected_label,
                target_type->name != NULL ? target_type->name : "<unknown>",
                target_type->name != NULL ? target_type->name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                expected_label);
            return true;
        }
    }

    if (source_slot->data.domain_slot.is_tobject) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' cannot be a tobject slot.\n"
            "Reason:\n"
            "- tobject is already a boundary snapshot\n"
            "- projection sync must read from a subject/object source, not from a transfer snapshot\n"
            "- source slot '%s' currently resolves to tobject '%s'\n"
            "Fix:\n"
            "- use the original subject/object slot as the source\n"
            "- or materialize a new object/tobject from the original source instead",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>");
        return true;
    }

    source_decl = find_subject_host_decl_by_name(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl))
        source_decl = find_named_class_decl_local(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl)) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' is not a valid projection source.\n"
            "Reason:\n"
            "- projection sync reads fields from a subject/object declaration\n"
            "- this slot does not point at a subject/object source host\n"
            "- resolved source type is '%s'\n"
            "Fix:\n"
            "- use a subject slot as the source\n"
            "- or use an object slot that mirrors the desired fields",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>");
        return true;
    }

    if (site != NULL && site->type == AST_ZONE_REFRESH
        && site->data.zone_refresh.field_map_count > 0) {
        for (size_t i = 0; i < site->data.zone_refresh.field_map_count; i++) {
            const char *mapped_target =
                site->data.zone_refresh.mapped_target_fields[i];

            if (!projection_target_decl_has_field(target_decl, mapped_target)) {
                semantic_error(ctx, site,
                    "%s %s projection map refers to unknown target field '%s' in slot '%s'.\n"
                    "Reason:\n"
                    "- target projection declaration '%s' does not declare field '%s'\n"
                    "- %s cannot write source slot '%s' into a field that does not exist\n"
                    "- target contract comes from projection type '%s'\n"
                    "Fix:\n"
                    "- add field '%s' to projection declaration '%s'\n"
                    "- or remove/update the field map entry for '%s'",
                    owner_label, action_name,
                    mapped_target != NULL ? mapped_target : "<field>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>",
                    action_name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>",
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>");
            }

            for (size_t j = i + 1; j < site->data.zone_refresh.field_map_count; j++) {
                const char *other_target =
                    site->data.zone_refresh.mapped_target_fields[j];
                if (mapped_target != NULL && other_target != NULL
                    && strcmp(mapped_target, other_target) == 0) {
                    semantic_error(ctx, site,
                        "%s %s projection map duplicates target field '%s'.\n"
                        "Reason:\n"
                        "- each projection target field may be filled from exactly one source field\n"
                        "- duplicate map entries make the projection source ambiguous\n"
                        "- target projection slot is '%s'\n"
                        "Fix:\n"
                        "- keep a single mapping for '%s'\n"
                        "- or split the target into distinct projection fields",
                        owner_label, action_name, mapped_target,
                        object_slot_name != NULL ? object_slot_name : "<unknown>",
                        mapped_target);
                }
            }
        }
    }

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        Type *target_field_type;
        Type *source_field_type;
        const char *source_field_name;
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL
            || target_field->type == NULL) {
            continue;
        }

        source_field_name = projection_refresh_source_field_name(site,
            target_field->name);
        source_status = resolve_projection_source_field_path(
            ctx->program_root, source_decl, source_field_name, ctx,
            &source_path, &source_field_type);
        if (source_status == 2) {
            semantic_error(ctx, site,
                "%s %s target field '%s' is ambiguous in source slot '%s'.\n"
                "Reason:\n"
                "- multiple projection source paths match field '%s'\n"
                "- automatic projection cannot choose one path safely\n"
                "- source declaration '%s' exposes overlapping candidate paths\n"
                "Fix:\n"
                "- rename one of the source fields to make the path unique\n"
                "- or expose the desired value through a dedicated object/tobject field",
                owner_label, action_name,
                target_field->name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                source_field_name != NULL ? source_field_name : target_field->name,
                source_type != NULL && source_type->name != NULL
                    ? source_type->name
                    : "<unknown>");
            continue;
        }
        if (source_status == 0 || source_field_type == NULL) {
            if (source_field_name != NULL
                && strcmp(source_field_name, target_field->name) != 0) {
                semantic_error(ctx, site,
                    "%s %s target field '%s' maps from missing source field '%s' in slot '%s'.\n"
                    "Reason:\n"
                    "- field map explicitly requests '%s <- %s'\n"
                    "- source declaration '%s' does not expose '%s'\n"
                    "- target projection declaration is '%s'\n"
                    "Fix:\n"
                    "- add source field '%s' to '%s'\n"
                    "- or change the map entry to a field that actually exists",
                    owner_label, action_name,
                    target_field->name,
                    source_field_name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    target_field->name,
                    source_field_name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>",
                    source_field_name,
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    source_field_name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>");
            } else {
                semantic_error(ctx, site,
                    "%s %s target field '%s' is missing from source slot '%s'.\n"
                    "Reason:\n"
                    "- projection target '%s' expects field '%s'\n"
                    "- source declaration '%s' does not expose a matching field\n"
                    "- automatic field matching therefore cannot derive a safe source path\n"
                    "Fix:\n"
                    "- add field '%s' to source declaration '%s'\n"
                    "- or add an explicit field map to a different existing source field",
                    owner_label, action_name,
                    target_field->name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    target_field->name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>",
                    target_field->name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>");
            }
            continue;
        }

        target_field_type = resolve_type_node(target_field->type, ctx);
        if (!type_is_assignable(source_field_type, target_field_type)) {
            semantic_error(ctx, site,
                "%s %s target field '%s' cannot accept source path '%s' from slot '%s'.\n"
                "Reason:\n"
                "- projection target slot '%s' expects field '%s' to have type '%s'\n"
                "- resolved source path '%s' from slot '%s' has type '%s'\n"
                "- projection sync cannot derive a safe coercion across this field boundary\n"
                "Fix:\n"
                "- change target field '%s' to type '%s'\n"
                "- or map '%s' from a source field/path whose type matches '%s'",
                owner_label, action_name,
                target_field->name,
                source_path != NULL ? source_path : source_field_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                target_field->name,
                type_name_or_unknown(target_field_type),
                source_path != NULL ? source_path : source_field_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                type_name_or_unknown(source_field_type),
                target_field->name,
                type_name_or_unknown(source_field_type),
                target_field->name,
                type_name_or_unknown(target_field_type));
            free(source_path);
            continue;
        }
        free(source_path);
    }

    return true;
}

static bool
type_check_zone_projection_contract(ASTNode *zone,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name)
{
    if (zone == NULL)
        return false;
    return type_check_projection_contract(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, "Zone", site,
        object_slot_name, source_slot_name, ctx, action_name);
}

static const char *
relation_endpoint_kind_label(RelationEndpointKind kind)
{
    switch (kind) {
    case RELATION_ENDPOINT_SUBJECT:
        return "subject";
    case RELATION_ENDPOINT_OBJECT:
        return "object";
    case RELATION_ENDPOINT_TOBJECT:
        return "tobject";
    case RELATION_ENDPOINT_CLASS:
        return "class/value";
    case RELATION_ENDPOINT_NAMED:
        return "named";
    default:
        return "endpoint";
    }
}

static bool
type_check_zone_effect_contract(ASTNode *zone,
                                ASTNode *apply_like,
                                const char *effect_slot_name,
                                const char *target_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *effect_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    ASTNode *decl_target;
    Type *target_type;
    Type *decl_target_type;
    size_t target_count;

    effect_slot = find_zone_effect_slot(zone, effect_slot_name);
    target_slot = find_zone_domain_slot(zone, target_slot_name);
    if (effect_slot == NULL || target_slot == NULL || ctx == NULL)
        return false;

    effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
        effect_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return false;

    target_count = count_bindable_domain_slots(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count);
    if (target_count != 1) {
        semantic_error(ctx, apply_like,
            "Zone %s requires effect '%s' to declare exactly one bindable target slot, found %zu.\n"
            "Reason:\n"
            "- zone effect slot '%s' binds effect '%s'\n"
            "- lifecycle propagation needs one concrete target path at the declaration boundary\n"
            "- current declaration exposes %zu bindable target paths, so target provenance is ambiguous\n"
            "Fix:\n"
            "- keep exactly one bindable target slot in effect '%s'\n"
            "- or split the effect contract if it truly targets multiple subjects",
            action_name,
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            target_count,
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            target_count,
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>");
        return true;
    }

    decl_target = find_nth_bindable_domain_slot(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count, 0);
    if (decl_target == NULL)
        return false;

    target_type = resolve_type_node(target_slot->data.domain_slot.type, ctx);
    decl_target_type = resolve_type_node(decl_target->data.domain_slot.type, ctx);
    if (!type_is_assignable(target_type, decl_target_type)) {
        semantic_error(ctx, apply_like,
            "Zone %s target slot '%s' has type '%s' but effect '%s' expects target type '%s'.\n"
            "Reason:\n"
            "- zone slot '%s' is bound to effect slot '%s'\n"
            "- propagation edge is effect slot '%s' -> target slot '%s'\n"
            "- effect '%s' declares target slot '%s' with required type '%s'\n"
            "- actual bound target type is '%s'\n"
            "Fix:\n"
            "- bind effect '%s' to a zone slot of type '%s'\n"
            "- or change effect '%s' so its target contract accepts '%s'",
            action_name,
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target->data.domain_slot.slot_name != NULL
                ? decl_target->data.domain_slot.slot_name
                : "<target-slot>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>");
    }

    return true;
}

static bool
relation_slot_matches_between_kind(ASTNode *slot,
                                   Type *slot_type,
                                   RelationEndpointKind kind,
                                   ASTNode *named_type_ref,
                                   SemanticContext *ctx)
{
    ASTNode *decl;
    Type *named_type;

    if (slot == NULL)
        return false;

    if (kind == RELATION_ENDPOINT_SUBJECT)
        return slot->data.domain_slot.is_subject;

    if (kind == RELATION_ENDPOINT_OBJECT) {
        if (slot->data.domain_slot.is_subject)
            return false;
        if (slot->data.domain_slot.is_tobject)
            return false;  /* tobject slot uses "tobject" kind, not "object" */
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
    }

    if (kind == RELATION_ENDPOINT_TOBJECT) {
        if (slot->data.domain_slot.is_subject)
            return false;
        if (!slot->data.domain_slot.is_tobject)
            return false;
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_TOBJECT;
    }

    if (kind == RELATION_ENDPOINT_CLASS) {
        if (slot->data.domain_slot.is_subject || slot->data.domain_slot.is_tobject)
            return false;
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        if (decl == NULL)
            return true;
        return decl->data.class_decl.nominal_kind != NOMINAL_DECL_OBJECT
            && decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT;
    }

    named_type = named_type_ref != NULL ? resolve_type_node(named_type_ref, ctx) : NULL;
    return slot_type != NULL
        && named_type != NULL
        && type_equals(slot_type, named_type);
}

static bool
type_check_zone_relation_contract(ASTNode *zone,
                                  ASTNode *link_like,
                                  const char *relation_slot_name,
                                  const char *left_slot_name,
                                  const char *right_slot_name,
                                  SemanticContext *ctx,
                                  const char *action_name)
{
    ASTNode *relation_slot;
    ASTNode *relation_decl;
    ASTNode *left_slot;
    ASTNode *right_slot;
    ASTNode *decl_left;
    ASTNode *decl_right;
    Type *left_type;
    Type *right_type;
    Type *decl_left_type;
    Type *decl_right_type;
    size_t endpoint_count;
    RelationEndpointKind between_left_kind;
    RelationEndpointKind between_right_kind;
    ASTNode *between_left_type;
    ASTNode *between_right_type;

    relation_slot = find_zone_relation_slot(zone, relation_slot_name);
    left_slot = find_zone_domain_slot(zone, left_slot_name);
    right_slot = find_zone_domain_slot(zone, right_slot_name);
    if (relation_slot == NULL || left_slot == NULL || right_slot == NULL || ctx == NULL)
        return false;

    relation_decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
        relation_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return false;

    between_left_kind = relation_decl->data.relation_decl.between_left_kind;
    between_right_kind = relation_decl->data.relation_decl.between_right_kind;
    between_left_type = relation_decl->data.relation_decl.between_left_type;
    between_right_type = relation_decl->data.relation_decl.between_right_type;
    left_type = resolve_type_node(left_slot->data.domain_slot.type, ctx);
    right_type = resolve_type_node(right_slot->data.domain_slot.type, ctx);

    if (between_left_kind != RELATION_ENDPOINT_NAMED || between_right_kind != RELATION_ENDPOINT_NAMED
        || between_left_type != NULL || between_right_type != NULL) {
        if (!relation_slot_matches_between_kind(left_slot, left_type, between_left_kind, between_left_type, ctx)) {
            semantic_error(ctx, link_like,
                "Zone %s left slot '%s' does not satisfy relation '%s' left-endpoint contract.\n"
                "Reason:\n"
                "- zone relation slot '%s' binds declaration '%s'\n"
                "- relation '%s' requires a %s-compatible left endpoint\n"
                "- actual left slot '%s' resolves to type '%s'\n"
                "Fix:\n"
                "- bind relation '%s' to a left slot that matches the declared endpoint kind\n"
                "- or change relation '%s' so its left endpoint contract accepts '%s'",
                action_name,
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_endpoint_kind_label(between_left_kind),
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>");
        }

        if (!relation_slot_matches_between_kind(right_slot, right_type, between_right_kind, between_right_type, ctx)) {
            semantic_error(ctx, link_like,
                "Zone %s right slot '%s' does not satisfy relation '%s' right-endpoint contract.\n"
                "Reason:\n"
                "- zone relation slot '%s' binds declaration '%s'\n"
                "- relation '%s' requires a %s-compatible right endpoint\n"
                "- actual right slot '%s' resolves to type '%s'\n"
                "Fix:\n"
                "- bind relation '%s' to a right slot that matches the declared endpoint kind\n"
                "- or change relation '%s' so its right endpoint contract accepts '%s'",
                action_name,
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_endpoint_kind_label(between_right_kind),
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>");
        }
        return true;
    }

    endpoint_count = count_bindable_domain_slots(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count);
    if (endpoint_count != 2) {
        semantic_error(ctx, link_like,
            "Zone %s requires relation '%s' to declare exactly two bindable endpoint slots, found %zu.\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- relation propagation needs one left endpoint and one right endpoint at declaration time\n"
            "- current declaration exposes %zu bindable endpoint paths, so edge provenance is ambiguous\n"
            "Fix:\n"
            "- keep exactly two bindable endpoint slots in relation '%s'\n"
            "- or split the relation contract if it truly models multiple independent edges",
            action_name,
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            endpoint_count,
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            endpoint_count,
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>");
        return true;
    }

    decl_left = find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 0);
    decl_right = find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 1);
    if (decl_left == NULL || decl_right == NULL)
        return false;

    decl_left_type = resolve_type_node(decl_left->data.domain_slot.type, ctx);
    decl_right_type = resolve_type_node(decl_right->data.domain_slot.type, ctx);

    if (!type_is_assignable(left_type, decl_left_type)) {
        semantic_error(ctx, link_like,
            "Zone %s left slot '%s' has type '%s' but relation '%s' expects left endpoint type '%s'.\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- relation '%s' declares left endpoint slot '%s' with required type '%s'\n"
            "- actual bound left slot type is '%s'\n"
            "Fix:\n"
            "- bind relation '%s' to a left slot of type '%s'\n"
            "- or change relation '%s' so its left endpoint contract accepts '%s'",
            action_name,
            left_slot_name != NULL ? left_slot_name : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left->data.domain_slot.slot_name != NULL
                ? decl_left->data.domain_slot.slot_name
                : "<left-slot>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>");
    }

    if (!type_is_assignable(right_type, decl_right_type)) {
        semantic_error(ctx, link_like,
            "Zone %s right slot '%s' has type '%s' but relation '%s' expects right endpoint type '%s'.\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- propagation edge is relation slot '%s' -> right endpoint '%s'\n"
            "- relation '%s' declares right endpoint slot '%s' with required type '%s'\n"
            "- actual bound right slot type is '%s'\n"
            "Fix:\n"
            "- bind relation '%s' to a right slot of type '%s'\n"
            "- or change relation '%s' so its right endpoint contract accepts '%s'",
            action_name,
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right->data.domain_slot.slot_name != NULL
                ? decl_right->data.domain_slot.slot_name
                : "<right-slot>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>");
    }

    return true;
}

static bool
type_check_overlay_decl_common(ASTNode *node,
                               SemanticContext *ctx,
                               const char *name,
                               SymbolKind kind,
                               ASTNode **shared_fields,
                               size_t shared_count,
                               ASTNode **methods,
                               size_t method_count,
                               const char *kind_name)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = kind;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = kind;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of %s '%s'", kind_name, name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    /* Register domain slots so bare slot access works in hosted funcs */
    if (node->type == AST_ZONE_DECL) {
        for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
            ASTNode *slot = node->data.zone_decl.slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && slot->data.domain_slot.type != NULL) {
                Type *slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
                Symbol *slot_sym = calloc(1, sizeof(Symbol));
                slot_sym->name = pergyra_strdup(slot->data.domain_slot.slot_name);
                slot_sym->kind = SYMBOL_VARIABLE;
                slot_sym->type = slot_type != NULL ? slot_type : TYPE_INT;
                slot_sym->decl_line = slot->line;
                slot_sym->decl_col = slot->column;
                scope_declare(ctx->scope, slot_sym);
            }
        }
    }
    if (node->type == AST_WORLD_DECL) {
        for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
            ASTNode *wz = node->data.world_decl.zones[i];
            if (wz != NULL && wz->type == AST_WORLD_ZONE
                && wz->data.world_zone.slot_name != NULL
                && wz->data.world_zone.zone_type != NULL) {
                Type *zone_type = resolve_named_type(
                    wz->data.world_zone.zone_type, ctx, wz);
                Symbol *zone_sym = calloc(1, sizeof(Symbol));
                zone_sym->name = pergyra_strdup(wz->data.world_zone.slot_name);
                zone_sym->kind = SYMBOL_VARIABLE;
                zone_sym->type = zone_type != NULL ? zone_type : TYPE_INT;
                zone_sym->decl_line = wz->line;
                zone_sym->decl_col = wz->column;
                scope_declare(ctx->scope, zone_sym);
            }
        }
    }
    /* Register shared fields so bare field access works in hosted funcs */
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL) {
            Type *field_type = TYPE_INT;
            if (shared->data.party_shared.type != NULL)
                field_type = resolve_type_node(shared->data.party_shared.type, ctx);
            Symbol *field_sym = calloc(1, sizeof(Symbol));
            field_sym->name = pergyra_strdup(shared->data.party_shared.name);
            field_sym->kind = SYMBOL_VARIABLE;
            field_sym->type = field_type;
            field_sym->decl_line = shared->line;
            field_sym->decl_col = shared->column;
            scope_declare(ctx->scope, field_sym);
        }
    }
    for (size_t i = 0; i < method_count; i++)
        type_check_func_decl(methods[i], ctx);
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_relation_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_relation = ctx->current_relation;
    ctx->current_relation = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.relation_decl.name,
        SYMBOL_RELATION,
        node->data.relation_decl.shared_fields,
        node->data.relation_decl.shared_count,
        node->data.relation_decl.methods,
        node->data.relation_decl.method_count,
        "relation");

    ok = type_check_domain_slots(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "Relation") && ok;
    ok = type_check_domain_slot_initializers(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "relation") && ok;
    for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.relation_decl.refreshes[i];
        const char *action_name =
            refresh->data.zone_refresh.derive_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.relation_decl.slots,
            node->data.relation_decl.slot_count, "Relation", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            action_name) && ok;
    }
    size_t endpoint_count = count_bindable_domain_slots(
        node->data.relation_decl.slots,
        node->data.relation_decl.slot_count,
        node->data.relation_decl.refreshes,
        node->data.relation_decl.refresh_count);
    if (endpoint_count == 0
        && !(node->data.relation_decl.between_left_kind != RELATION_ENDPOINT_NAMED
            || node->data.relation_decl.between_right_kind != RELATION_ENDPOINT_NAMED
            || node->data.relation_decl.between_left_type != NULL
            || node->data.relation_decl.between_right_type != NULL)) {
        semantic_warning(ctx, node,
            "Relation '%s' should declare at least one endpoint slot.\n"
            "Reason:\n"
            "- declaration propagation edge is relation '%s' -> endpoint slot\n"
            "- relation contracts need a concrete endpoint surface to bind/link against\n"
            "- without an endpoint slot, relation and projection propagation stay underspecified\n"
            "- branch/join or zone/world propagation cannot anchor a stable relation edge without at least one endpoint slot\n"
            "Fix:\n"
            "- use 'for name: Type'\n"
            "- or declare a bindable subject/object endpoint slot",
            node->data.relation_decl.name,
            node->data.relation_decl.name);
    } else if (endpoint_count > 2) {
        semantic_warning(ctx, node,
            "Relation '%s' currently declares more than two endpoint slots.\n"
            "Reason:\n"
            "- relation is currently treated as a small edge contract\n"
            "- more than two endpoints usually mean party/zone coordination rather than a single relation edge\n"
            "Fix:\n"
            "- reduce the relation to two endpoints\n"
            "- or move the coordination shape into party/zone",
            node->data.relation_decl.name);
    }
    /* Validate 'between' clause if present */
    RelationEndpointKind blk = node->data.relation_decl.between_left_kind;
    RelationEndpointKind brk = node->data.relation_decl.between_right_kind;
    ASTNode *blt = node->data.relation_decl.between_left_type;
    ASTNode *brt = node->data.relation_decl.between_right_type;
    if (blk != RELATION_ENDPOINT_NAMED || brk != RELATION_ENDPOINT_NAMED
        || blt != NULL || brt != NULL) {
        /* At least one side must be 'subject' */
        bool left_is_subject = (blk == RELATION_ENDPOINT_SUBJECT);
        bool right_is_subject = (brk == RELATION_ENDPOINT_SUBJECT);
        if (!left_is_subject && !right_is_subject) {
            semantic_error(ctx, node,
                "Relation '%s' must declare at least one subject endpoint in 'between'.\n"
                "Reason:\n"
                "- relation edges are anchored around at least one active subject endpoint\n"
                "- current endpoint kinds are %d and %d, so the contract has no active anchor\n"
                "Fix:\n"
                "- change one side of 'between' to subject\n"
                "- or model this shape as effect/object projection instead",
                node->data.relation_decl.name, (int)blk, (int)brk);
            ok = false;
        }
    }

    ctx->current_relation = saved_relation;
    return ok && !ctx->has_error;
}

bool
type_check_effect_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_effect = ctx->current_effect;
    ctx->current_effect = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.effect_decl.name,
        SYMBOL_EFFECT,
        node->data.effect_decl.shared_fields,
        node->data.effect_decl.shared_count,
        node->data.effect_decl.methods,
        node->data.effect_decl.method_count,
        "effect");

    ok = type_check_domain_slots(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "Effect") && ok;
    ok = type_check_domain_slot_initializers(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "effect") && ok;
    for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.effect_decl.refreshes[i];
        const char *action_name =
            refresh->data.zone_refresh.derive_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.effect_decl.slots,
            node->data.effect_decl.slot_count, "Effect", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            action_name) && ok;
    }
    size_t target_count = count_bindable_domain_slots(
        node->data.effect_decl.slots,
        node->data.effect_decl.slot_count,
        node->data.effect_decl.refreshes,
        node->data.effect_decl.refresh_count);
    if (target_count == 0) {
        semantic_warning(ctx, node,
            "Effect '%s' should declare at least one target slot.\n"
            "Reason:\n"
            "- declaration propagation edge is effect '%s' -> target slot\n"
            "- effect contracts need a concrete target surface to apply/maintain against\n"
            "- without a target slot, effect propagation and authority checks stay underspecified\n"
            "- branch/join or zone/world propagation cannot preserve an effect target path without a bindable target slot\n"
            "Fix:\n"
            "- use 'for name: Type'\n"
            "- or declare a bindable subject/object target slot",
            node->data.effect_decl.name,
            node->data.effect_decl.name);
    } else if (target_count > 1) {
        semantic_warning(ctx, node,
            "Effect '%s' currently declares multiple target slots.\n"
            "Reason:\n"
            "- effect is currently treated as a focused layer on one target path\n"
            "- multiple targets usually mean relation or zone-state coordination instead\n"
            "Fix:\n"
            "- keep one effect target slot\n"
            "- or move the multi-target shape into relation/zone",
            node->data.effect_decl.name);
    }
    ctx->current_effect = saved_effect;
    return ok && !ctx->has_error;
}

bool
type_check_zone_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_zone = ctx->current_zone;
    const char *prev_module_path = ctx->current_module_path;
    size_t subject_count;
    size_t object_count;
    size_t mutation_rule_count;
    ctx->current_zone = node;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.zone_decl.name,
        SYMBOL_ZONE,
        node->data.zone_decl.shared_fields,
        node->data.zone_decl.shared_count,
        node->data.zone_decl.methods,
        node->data.zone_decl.method_count,
        "zone");

    ok = type_check_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "Zone") && ok;
    ok = type_check_domain_slot_initializers(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "zone") && ok;
    subject_count = count_subject_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);
    object_count = count_object_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);

    if (subject_count > 4) {
        semantic_warning(ctx, node,
            "Zone '%s' declares %zu subject slots; prefer keeping active subjects to 4 or fewer and model supporting state as objects",
            node->data.zone_decl.name,
            subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has multiple subject slots but no object slots; consider modeling passive support state as objects",
            node->data.zone_decl.name);
    }

    mutation_rule_count = node->data.zone_decl.apply_count
        + node->data.zone_decl.link_count
        + node->data.zone_decl.detach_count
        + node->data.zone_decl.unlink_count
        + node->data.zone_decl.refresh_count
        + node->data.zone_decl.maintained_effect_count
        + node->data.zone_decl.maintained_relation_count
        + node->data.zone_decl.maintained_state_count;

    if (subject_count == 0
        && (mutation_rule_count > 0 || node->data.zone_decl.authority_count > 0)) {
        semantic_warning(ctx, node,
            "Zone '%s' mutates state or declares authority but has no subject slot",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
        ASTNode *authority = node->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(node, authority->data.zone_authority.subject_slot_name);
        if (slot == NULL) {
            semantic_error(ctx, authority,
                "Zone authority references unknown subject slot '%s'",
                authority->data.zone_authority.subject_slot_name);
            continue;
        }
        if (!slot->data.domain_slot.is_subject) {
            semantic_error(ctx, authority,
                "Zone authority '%s' must reference a subject slot",
                authority->data.zone_authority.subject_slot_name);
        }
        slot_type = slot != NULL ? resolve_type_node(slot->data.domain_slot.type, ctx) : NULL;
        for (size_t j = 0; j < authority->data.zone_authority.ability_count; j++) {
            ASTNode *ability_ref = authority->data.zone_authority.required_abilities[j];
            const char *ability_name = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                authority,
                authority->data.zone_authority.subject_slot_name != NULL
                    ? authority->data.zone_authority.subject_slot_name : "<authority>",
                ability_ref,
                "zone authority ability consumer lookup");
            if (resolve_required_ability_decl(
                    ability_ref, authority, ctx, "Zone authority",
                    authority->data.zone_authority.subject_slot_name) == NULL) {
                free(required_text);
                continue;
            }
            if (slot_type == NULL || slot_type == TYPE_UNKNOWN
                || slot_type->name == NULL) {
                free(required_text);
                continue;
            }
            if (!subject_type_has_ability(ctx->program_root, slot_type->name, ability_ref)) {
                ASTNode *actual_impl = subject_type_find_base_ability_impl(
                    ctx->program_root, slot_type->name, ability_name);
                char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                report_subject_ability_requirement_mismatch(
                    ctx, authority, "Zone authority",
                    authority->data.zone_authority.subject_slot_name,
                    "subject type", slot_type->name,
                    required_text != NULL ? required_text : ability_name,
                    actual_text,
                    "change/remove the zone authority requirement");
                free(actual_text);
            }
            free(required_text);
        }
        for (size_t j = i + 1; j < node->data.zone_decl.authority_count; j++) {
            ASTNode *other = node->data.zone_decl.authorities[j];
            if (other != NULL
                && other->data.zone_authority.subject_slot_name != NULL
                && strcmp(authority->data.zone_authority.subject_slot_name,
                          other->data.zone_authority.subject_slot_name) == 0) {
                semantic_error(ctx, other,
                    "Zone '%s' declares authority '%s' more than once.\n"
                    "Reason:\n"
                    "- zone authority must be uniquely attributable per subject slot\n"
                    "- duplicate authority declarations make provenance and diagnostics ambiguous\n"
                    "Fix:\n"
                    "- keep one authority declaration for '%s'\n"
                    "- or merge the required ability list into a single authority clause",
                    node->data.zone_decl.name,
                    authority->data.zone_authority.subject_slot_name,
                    authority->data.zone_authority.subject_slot_name);
            }
        }
    }

    if (mutation_rule_count > 0 && node->data.zone_decl.authority_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has lifecycle-changing rules but no explicit authority set",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = node->data.zone_decl.layer_slots[i];
        const char *type_name = layer_slot->data.zone_layer_slot.layer_type;
        Symbol *sym = type_name != NULL ? scope_lookup(ctx->scope, type_name) : NULL;
        ASTNodeType decl_type = layer_slot->data.zone_layer_slot.is_relation
            ? AST_RELATION_DECL
            : AST_EFFECT_DECL;
        ASTNode *decl = type_name != NULL
            ? find_domain_decl_by_name(ctx->program_root, decl_type, type_name)
            : NULL;
        SymbolKind expected = layer_slot->data.zone_layer_slot.is_relation
            ? SYMBOL_RELATION
            : SYMBOL_EFFECT;
        const char *kind_name = layer_slot->data.zone_layer_slot.is_relation
            ? "relation"
            : "effect";
        if ((sym == NULL || sym->kind != expected)
            && !(decl != NULL
                 && decl->type == decl_type
                 && explicit_type_reference_allowed(decl, node, ctx))) {
            semantic_error(ctx, layer_slot,
                "Zone layer slot '%s' references unknown %s type '%s'.\n"
                "Reason:\n"
                "- zone layer slots must bind to declared %s contracts before semantic closure\n"
                "- unresolved layer types would force later lowering/runtime paths to guess the contract\n"
                "Fix:\n"
                "- declare %s '%s' before using it in zone '%s'\n"
                "- or change the layer slot type to an existing %s declaration",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<slot>",
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                kind_name,
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                kind_name);
        }
        if (layer_slot->data.zone_layer_slot.is_relation
            && layer_slot->data.zone_layer_slot.is_pool) {
            semantic_error(ctx, layer_slot,
                "Zone relation pool '%s' is not supported yet",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<unknown>");
        }
        if (!layer_slot->data.zone_layer_slot.is_relation
            && layer_slot->data.zone_layer_slot.is_pool
            && layer_slot->data.zone_layer_slot.pool_capacity <= 0) {
            semantic_error(ctx, layer_slot,
                "Zone effect pool '%s' must declare a positive capacity",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<unknown>");
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        const char *effect_slot_name = apply->data.zone_apply.effect_slot_name;
        const char *target_slot_name = apply->data.zone_apply.target_slot_name;
        const char *state_name = apply->data.zone_apply.state_name;
        const char *participant_slot_name = apply->data.zone_apply.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, apply, state_name, ctx, "apply",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, apply,
                "Zone apply references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- apply mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, apply,
                "Zone apply references unknown target slot '%s'.\n"
                "Reason:\n"
                "- apply must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, apply,
            effect_slot_name, target_slot_name, ctx, "apply");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, apply,
                "Zone apply must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and apply mutates effect state on '%s'\n"
                "- without an approving subject slot, contract provenance becomes incomplete\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this apply clause\n"
                "- or remove zone authority if this rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<target>");
        }
        type_check_zone_participant_authority(node, apply, participant_slot_name, ctx, "apply");
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        const char *relation_slot_name = link->data.zone_link.relation_slot_name;
        const char *left_slot_name = link->data.zone_link.left_slot_name;
        const char *right_slot_name = link->data.zone_link.right_slot_name;
        const char *state_name = link->data.zone_link.state_name;
        const char *participant_slot_name = link->data.zone_link.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, link, state_name, ctx, "link",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- link mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown left slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown right slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, link,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "link");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, link,
                "Zone link must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and link mutates relation state\n"
                "- relation provenance must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this link clause\n"
                "- or remove zone authority if this relation edge is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_participant_authority(node, link, participant_slot_name, ctx, "link");
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        const char *effect_slot_name = detach->data.zone_detach.effect_slot_name;
        const char *target_slot_name = detach->data.zone_detach.target_slot_name;
        const char *state_name = detach->data.zone_detach.state_name;
        const char *participant_slot_name = detach->data.zone_detach.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, detach, state_name, ctx, "detach",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, detach,
                "Zone detach references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- detach mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, detach,
                "Zone detach references unknown target slot '%s'.\n"
                "Reason:\n"
                "- detach must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, detach,
            effect_slot_name, target_slot_name, ctx, "detach");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, detach,
                "Zone detach must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and detach mutates effect lifecycle on '%s'\n"
                "- detachment provenance must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this detach clause\n"
                "- or remove zone authority if this detach rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<target>");
        }
        type_check_zone_participant_authority(node, detach, participant_slot_name, ctx, "detach");
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        const char *relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
        const char *left_slot_name = unlink->data.zone_unlink.left_slot_name;
        const char *right_slot_name = unlink->data.zone_unlink.right_slot_name;
        const char *state_name = unlink->data.zone_unlink.state_name;
        const char *participant_slot_name = unlink->data.zone_unlink.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, unlink, state_name, ctx, "unlink",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- unlink mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown left slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown right slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, unlink,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "unlink");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and unlink mutates relation lifecycle\n"
                "- unlink provenance must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this unlink clause\n"
                "- or remove zone authority if this unlink rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_participant_authority(node, unlink, participant_slot_name, ctx, "unlink");
    }

    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.zone_decl.refreshes[i];
        const char *object_slot_name = refresh->data.zone_refresh.object_slot_name;
        const char *source_slot_name = refresh->data.zone_refresh.source_slot_name;
        const char *participant_slot_name = refresh->data.zone_refresh.participant_slot_name;
        ASTNode *target_slot = NULL;
        bool boundary_projection = false;
        const char *action_name =
            refresh->data.zone_refresh.derive_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (find_zone_domain_slot(node, object_slot_name) == NULL) {
            semantic_error(ctx, refresh,
                "Zone %s references unknown target slot '%s'.\n"
                "Reason:\n"
                "- the projection target must already be declared in the zone\n"
                "- '%s' is not a known object/tobject slot in zone '%s'\n"
                "Fix:\n"
                "- declare object/tobject slot '%s' in the zone first\n"
                "- or change %s to an existing projection target slot",
                action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                action_name);
        }
        if (find_zone_domain_slot(node, source_slot_name) == NULL) {
            semantic_error(ctx, refresh,
                "Zone %s references unknown source slot '%s'.\n"
                "Reason:\n"
                "- projection sync must read from a declared zone slot\n"
                "- '%s' is not a known source slot in zone '%s'\n"
                "Fix:\n"
                "- declare source slot '%s' in the zone first\n"
                "- or change %s to an existing source slot",
                action_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<slot>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                source_slot_name != NULL ? source_slot_name : "<slot>",
                action_name);
        }
        target_slot = find_zone_domain_slot(node, object_slot_name);
        boundary_projection = target_slot != NULL
            && target_slot->type == AST_DOMAIN_SLOT
            && target_slot->data.domain_slot.is_tobject;
        type_check_zone_projection_contract(node, refresh,
            object_slot_name, source_slot_name, ctx, action_name);
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            if (boundary_projection) {
                semantic_error(ctx, refresh,
                    "Zone %s to boundary target '%s' must specify 'by <subjectSlot>' when authority is declared.\n"
                    "Reason:\n"
                    "- boundary projection publishes authority-bearing state across the zone edge\n"
                    "- zone '%s' declares authority, so provenance must name the approving subject slot\n"
                    "Fix:\n"
                    "- add 'by <subjectSlot>' to this %s clause\n"
                    "- or publish into a non-authority zone",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    action_name);
            } else {
                semantic_error(ctx, refresh,
                    "Zone %s must specify 'by <subjectSlot>' when authority is declared.\n"
                    "Reason:\n"
                    "- the zone declares authority and this projection mutates local derived state\n"
                    "- without an explicit participant, contract provenance becomes harder to explain at diagnostics/runtime\n"
                    "Fix:\n"
                    "- add 'by <subjectSlot>' to this %s clause\n"
                    "- or remove the authority declaration if this projection is intentionally authority-free",
                    action_name,
                    action_name);
            }
        }
        type_check_zone_participant_authority(node, refresh, participant_slot_name, ctx, action_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        const char *effect_slot_name = maintain->data.zone_maintain_effect.effect_slot_name;
        const char *target_slot_name = maintain->data.zone_maintain_effect.target_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_effect.participant_slot_name;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps an effect lifecycle active and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown target slot '%s'.\n"
                "Reason:\n"
                "- maintain must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, maintain,
            effect_slot_name, target_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and maintain keeps effect '%s' active on '%s'\n"
                "- persistent lifecycle rules must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this maintain clause\n"
                "- or remove zone authority if this maintenance rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<effect>",
                target_slot_name != NULL ? target_slot_name : "<target>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_effect_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_effects[j];
            if (strcmp(effect_slot_name, other->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(target_slot_name, other->data.zone_maintain_effect.target_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains effect '%s' on '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for effect '%s' on '%s'\n"
                    "- or split the contract if the lifecycle targets are actually different",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
            ASTNode *detach = node->data.zone_decl.detaches[j];
            const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
            const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
            if (detach->data.zone_detach.state_name != NULL) {
                resolve_zone_effect_state(node, detach,
                    detach->data.zone_detach.state_name, ctx, "detach",
                    &detach_effect_slot_name, &detach_target_slot_name);
            }
            if (detach_effect_slot_name != NULL
                && detach_target_slot_name != NULL
                && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and detaches effect '%s' on '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while detach removes the same lifecycle path\n"
                    "- the current zone contract points in two different directions for one effect target pair\n"
                    "Fix:\n"
                    "- keep either maintain or detach for effect '%s' on '%s'\n"
                    "- or split the rules so they target different slots/states",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        const char *relation_slot_name = maintain->data.zone_maintain_relation.relation_slot_name;
        const char *left_slot_name = maintain->data.zone_maintain_relation.left_slot_name;
        const char *right_slot_name = maintain->data.zone_maintain_relation.right_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_relation.participant_slot_name;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps a relation lifecycle active and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown left slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown right slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, maintain,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and maintain keeps relation '%s' active\n"
                "- persistent relation lifecycle rules must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this maintain clause\n"
                "- or remove zone authority if this maintenance rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<relation>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_relation_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_relations[j];
            if (strcmp(relation_slot_name, other->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(left_slot_name, other->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(right_slot_name, other->data.zone_maintain_relation.right_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains relation '%s' between '%s' and '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same relation lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for relation '%s' between '%s' and '%s'\n"
                    "- or split the contract if the relation endpoints are actually different",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
            ASTNode *unlink = node->data.zone_decl.unlinks[j];
            const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
            const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
            const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
            if (unlink->data.zone_unlink.state_name != NULL) {
                resolve_zone_relation_state(node, unlink,
                    unlink->data.zone_unlink.state_name, ctx, "unlink",
                    &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
            }
            if (unlink_relation_slot_name != NULL
                && unlink_left_slot_name != NULL
                && unlink_right_slot_name != NULL
                && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                && strcmp(left_slot_name, unlink_left_slot_name) == 0
                && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and unlinks relation '%s' between '%s' and '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while unlink removes the same relation path\n"
                    "- the current zone contract points in two different directions for one relation edge\n"
                    "Fix:\n"
                    "- keep either maintain or unlink for relation '%s' between '%s' and '%s'\n"
                    "- or split the rules so they target different relation states",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        ASTNode *state;
        const char *state_name = maintain->data.zone_maintain_state.state_name;
        const char *participant_slot_name = maintain->data.zone_maintain_state.participant_slot_name;
        state = find_zone_state(node, state_name);
        if (state == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown state '%s'.\n"
                "Reason:\n"
                "- maintain state aliases must reference a declared zone state\n"
                "- zone '%s' does not declare state '%s'\n"
                "Fix:\n"
                "- declare state '%s' before this maintain clause\n"
                "- or change maintain to an existing state alias",
                state_name != NULL ? state_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                state_name != NULL ? state_name : "<unknown>",
                state_name != NULL ? state_name : "<unknown>");
        } else if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            type_check_zone_relation_contract(node, maintain,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        node->data.zone_decl.name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
                ASTNode *unlink = node->data.zone_decl.unlinks[j];
                const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
                const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
                const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
                if (unlink->data.zone_unlink.state_name != NULL) {
                    resolve_zone_relation_state(node, unlink,
                        unlink->data.zone_unlink.state_name, ctx, "unlink",
                        &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
                }
                if (unlink_relation_slot_name != NULL
                    && unlink_left_slot_name != NULL
                    && unlink_right_slot_name != NULL
                    && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                    && strcmp(left_slot_name, unlink_left_slot_name) == 0
                    && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and unlinks state '%s'.\n"
                        "Reason:\n"
                        "- maintain keeps the named relation-state active, while unlink removes the same underlying path\n"
                        "- the current zone contract points in two different directions for state '%s'\n"
                        "Fix:\n"
                        "- keep either maintain or unlink for state '%s'\n"
                        "- or split the rules so they target different states",
                        node->data.zone_decl.name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            type_check_zone_effect_contract(node, maintain,
                effect_slot_name, target_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        node->data.zone_decl.name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
                ASTNode *detach = node->data.zone_decl.detaches[j];
                const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
                const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
                if (detach->data.zone_detach.state_name != NULL) {
                    resolve_zone_effect_state(node, detach,
                        detach->data.zone_detach.state_name, ctx, "detach",
                        &detach_effect_slot_name, &detach_target_slot_name);
                }
                if (detach_effect_slot_name != NULL
                    && detach_target_slot_name != NULL
                    && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                    && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and detaches state '%s'.\n"
                        "Reason:\n"
                        "- maintain keeps the named effect-state active, while detach removes the same underlying path\n"
                        "- the current zone contract points in two different directions for state '%s'\n"
                        "Fix:\n"
                        "- keep either maintain or detach for state '%s'\n"
                        "- or split the rules so they target different states",
                        node->data.zone_decl.name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        }
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and maintain keeps named state '%s' active\n"
                "- persistent named-state lifecycle rules must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this maintain clause\n"
                "- or remove zone authority if this maintenance rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                state_name != NULL ? state_name : "<state>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        const char *state_name = state->data.zone_state.state_name;
        if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown relation slot '%s'.\n"
                    "Reason:\n"
                    "- state '%s' is declared as a relation-backed lifecycle alias\n"
                    "- zone '%s' does not declare relation slot '%s'\n"
                    "Fix:\n"
                    "- reference an existing relation slot in state '%s'\n"
                    "- or declare relation slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, left_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown left slot '%s'.\n"
                    "Reason:\n"
                    "- relation state '%s' needs a declared left endpoint slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the left endpoint\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, right_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown right slot '%s'.\n"
                    "Reason:\n"
                    "- relation state '%s' needs a declared right endpoint slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the right endpoint\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>");
            }
            type_check_zone_relation_contract(node, state,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "state");
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown effect slot '%s'.\n"
                    "Reason:\n"
                    "- state '%s' is declared as an effect-backed lifecycle alias\n"
                    "- zone '%s' does not declare effect slot '%s'\n"
                    "Fix:\n"
                    "- reference an existing effect slot in state '%s'\n"
                    "- or declare effect slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, target_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown target slot '%s'.\n"
                    "Reason:\n"
                    "- effect state '%s' needs a declared target slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the target\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>");
            }
            type_check_zone_effect_contract(node, state,
                effect_slot_name, target_slot_name, ctx, "state");
        }

        for (size_t j = i + 1; j < node->data.zone_decl.state_count; j++) {
            ASTNode *other = node->data.zone_decl.states[j];
            if (state_name != NULL
                && other != NULL
                && other->data.zone_state.state_name != NULL
                && strcmp(state_name, other->data.zone_state.state_name) == 0) {
                semantic_error(ctx, other,
                    "Redeclaration of zone state '%s'",
                    state_name);
            }
        }
    }

    ctx->current_zone = saved_zone;
    ctx->current_module_path = prev_module_path;
    return ok && !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Async system type checkers
 * ----------------------------------------------------------------- */

bool
type_check_async_block(ASTNode *node, SemanticContext *ctx)
{
    bool saved_async = ctx->in_async_func;
    ctx->in_async_func = true;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
        type_check_statement(node->data.async_block.statements[i], ctx);
    }

    ctx->in_async_func = saved_async;
    return !ctx->has_error;
}

bool
type_check_select_stmt(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        if (c != NULL) {
            bool valid_case = false;
            if (c->type == AST_BLOCK && c->data.block.count > 0) {
                ASTNode *first = c->data.block.statements[0];
                ASTNode *recv_expr = NULL;
                const char *bind_name = NULL;

                if (first != NULL && first->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    recv_expr = first;
                } else if (first != NULL && first->type == AST_ASSIGNMENT
                           && first->data.assignment.target != NULL
                           && first->data.assignment.target->type == AST_IDENTIFIER
                           && first->data.assignment.value != NULL
                           && first->data.assignment.value->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    bind_name = first->data.assignment.target->data.identifier.name;
                    recv_expr = first->data.assignment.value;
                }

                if (!valid_case) {
                    semantic_error(ctx, c,
                        "select case must begin with a channel receive pattern");
                    type_check_statement(c, ctx);
                    continue;
                }

                scope_enter(&ctx->scope, SCOPE_BLOCK);
                if (recv_expr != NULL) {
                    Type *recv_type = type_check_expression(recv_expr, ctx);
                    if (bind_name != NULL && recv_type != NULL) {
                        Symbol *binding = symbol_create_variable(
                            bind_name, recv_type, first->line, first->column);
                        scope_declare(ctx->scope, binding);
                    }
                }
                for (size_t j = 1; j < c->data.block.count; j++)
                    type_check_statement(c->data.block.statements[j], ctx);
                scope_exit(&ctx->scope);
            } else {
                semantic_error(ctx, c,
                    "select case must begin with a channel receive pattern");
                type_check_statement(c, ctx);
            }
        }
    }

    if (node->data.select_stmt.default_case)
        type_check_statement(node->data.select_stmt.default_case, ctx);

    return !ctx->has_error;
}
