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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (state->data.zone_state.is_relation) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (!state->data.zone_state.is_relation) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s references unknown authority subject slot '%s'",
            action_name, participant_slot_name);
        return true;
    }

    if (!participant_slot->data.domain_slot.is_subject) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s authority '%s' must be a subject slot",
            action_name, participant_slot_name);
        return true;
    }

    authority = find_zone_authority(zone, participant_slot_name);
    if (zone->data.zone_decl.authority_count > 0 && authority == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
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
                               const char *owner_name,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s target slot '%s' is not a projection slot.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
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
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s target slot '%s' must be a tobject slot.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
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
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s target slot '%s' does not use a projection declaration.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
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
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                "%s %s target slot '%s' uses the wrong projection kind.\n"
                "Contract source:\n"
                "- target slot '%s' on %s '%s'\n"
                "- source slot '%s' is driving this %s path\n"
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
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                owner_label,
                owner_name != NULL ? owner_name : "<owner>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                action_name,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s source slot '%s' cannot be a tobject slot.\n"
            "Reason:\n"
            "- projection consumer path is target slot '%s' <- source slot '%s'\n"
            "- tobject is already a boundary snapshot\n"
            "- projection sync must read from a subject/object source, not from a transfer snapshot\n"
            "- source slot '%s' currently resolves to tobject '%s'\n"
            "- projection contract originates from target slot '%s' on %s '%s'\n"
            "Fix:\n"
            "- use the original subject/object slot as the source\n"
            "- or materialize a new object/tobject from the original source instead",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label != NULL ? owner_label : "<owner>",
            owner_label != NULL ? owner_label : "<owner>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>");
        return true;
    }

    source_decl = find_subject_host_decl_by_name(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl))
        source_decl = find_named_class_decl_local(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s source slot '%s' is not a valid projection source.\n"
            "Reason:\n"
            "- projection consumer path is target slot '%s' <- source slot '%s'\n"
            "- projection sync reads fields from a subject/object declaration\n"
            "- this slot does not point at a subject/object source host\n"
            "- resolved source type is '%s'\n"
            "- projection contract originates from target slot '%s' on %s '%s'\n"
            "Fix:\n"
            "- use a subject slot as the source\n"
            "- or use an object slot that mirrors the desired fields",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label != NULL ? owner_label : "<owner>",
            owner_label != NULL ? owner_label : "<owner>");
        return true;
    }

    if (site != NULL && site->type == AST_ZONE_REFRESH
        && site->data.zone_refresh.field_map_count > 0) {
        for (size_t i = 0; i < site->data.zone_refresh.field_map_count; i++) {
            const char *mapped_target =
                site->data.zone_refresh.mapped_target_fields[i];

            if (!projection_target_decl_has_field(target_decl, mapped_target)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                    "%s %s projection map refers to unknown target field '%s' in slot '%s'.\n"
                    "Reason:\n"
                    "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                    "- explicit field map entry is '%s <- %s'\n"
                    "- target projection declaration '%s' does not declare field '%s'\n"
                    "- %s cannot write source slot '%s' into a field that does not exist\n"
                    "- target contract comes from projection type '%s'\n"
                    "Fix:\n"
                    "- add field '%s' to projection declaration '%s'\n"
                    "- or remove/update the field map entry for '%s'",
                    owner_label, action_name,
                    mapped_target != NULL ? mapped_target : "<field>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>",
                    projection_refresh_source_field_name(site, mapped_target) != NULL
                        ? projection_refresh_source_field_name(site, mapped_target)
                        : "<source-field>",
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
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                        "%s %s projection map duplicates target field '%s'.\n"
                        "Reason:\n"
                        "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                        "- each projection target field may be filled from exactly one source field\n"
                        "- duplicate map entries make the projection source ambiguous\n"
                        "- target projection slot is '%s'\n"
                        "Fix:\n"
                        "- keep a single mapping for '%s'\n"
                        "- or split the target into distinct projection fields",
                        owner_label, action_name, mapped_target,
                        object_slot_name != NULL ? object_slot_name : "<unknown>",
                        source_slot_name != NULL ? source_slot_name : "<unknown>",
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                "%s %s target field '%s' is ambiguous in source slot '%s'.\n"
                "Contract source:\n"
                "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                "- target projection declaration is '%s'\n"
                "Reason:\n"
                "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                "- requested source field/path is '%s'\n"
                "- multiple projection source paths match field '%s'\n"
                "- automatic projection cannot choose one path safely\n"
                "- source declaration '%s' exposes overlapping candidate paths\n"
                "Fix:\n"
                "- rename one of the source fields to make the path unique\n"
                "- or expose the desired value through a dedicated object/tobject field",
                owner_label, action_name,
                target_field->name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                target_type != NULL && target_type->name != NULL
                    ? target_type->name
                    : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                source_field_name != NULL ? source_field_name : target_field->name,
                source_field_name != NULL ? source_field_name : target_field->name,
                source_type != NULL && source_type->name != NULL
                    ? source_type->name
                    : "<unknown>");
            continue;
        }
        if (source_status == 0 || source_field_type == NULL) {
            if (source_field_name != NULL
                && strcmp(source_field_name, target_field->name) != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                    "%s %s target field '%s' maps from missing source field '%s' in slot '%s'.\n"
                    "Reason:\n"
                    "- projection consumer path is target slot '%s' <- source slot '%s'\n"
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
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
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
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                    "%s %s target field '%s' is missing from source slot '%s'.\n"
                    "Reason:\n"
                    "- projection consumer path is target slot '%s' <- source slot '%s'\n"
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                "%s %s target field '%s' cannot accept source path '%s' from slot '%s'.\n"
                "Reason:\n"
                "- projection consumer path is target slot '%s' <- source slot '%s'\n"
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
        zone->data.zone_decl.slot_count, "Zone",
        zone->data.zone_decl.name, site,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply_like,
            "Zone %s requires effect '%s' to declare exactly one bindable target slot, found %llu.\n"
            "Reason:\n"
            "- zone effect slot '%s' binds effect '%s'\n"
            "- lifecycle propagation needs one concrete target path at the declaration boundary\n"
            "- current declaration exposes %llu bindable target paths, so target provenance is ambiguous\n"
            "Fix:\n"
            "- keep exactly one bindable target slot in effect '%s'\n"
            "- or split the effect contract if it truly targets multiple subjects",
            action_name,
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            (unsigned long long) target_count,
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            (unsigned long long) target_count,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply_like,
            "Zone %s target slot '%s' has type '%s' but effect '%s' expects target type '%s'.\n"
            "Contract source:\n"
            "- effect slot '%s' in zone contract '%s'\n"
            "- propagation edge is effect slot '%s' -> target slot '%s'\n"
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
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            action_name != NULL ? action_name : "<zone>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            target_slot_name != NULL ? target_slot_name : "<target-slot>",
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
            "Zone %s requires relation '%s' to declare exactly two bindable endpoint slots, found %llu.\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- relation propagation needs one left endpoint and one right endpoint at declaration time\n"
            "- current declaration exposes %llu bindable endpoint paths, so edge provenance is ambiguous\n"
            "Fix:\n"
            "- keep exactly two bindable endpoint slots in relation '%s'\n"
            "- or split the relation contract if it truly models multiple independent edges",
            action_name,
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            (unsigned long long) endpoint_count,
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            (unsigned long long) endpoint_count,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
            "Zone %s left slot '%s' has type '%s' but relation '%s' expects left endpoint type '%s'.\n"
            "Contract source:\n"
            "- relation slot '%s' in zone contract '%s'\n"
            "- endpoint edge: left '%s' -> relation '%s'\n"
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
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            left_slot_name != NULL ? left_slot_name : "<left-slot>",
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
            "Zone %s right slot '%s' has type '%s' but relation '%s' expects right endpoint type '%s'.\n"
            "Contract source:\n"
            "- relation slot '%s' in zone contract '%s'\n"
            "- endpoint edge: relation '%s' -> right '%s'\n"
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
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            right_slot_name != NULL ? right_slot_name : "<right-slot>",
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
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_OVERLAY_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of %s '%s'", kind_name, name);
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
