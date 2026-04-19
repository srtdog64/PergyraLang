find_world_zone_slot_local(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
find_world_state_local(ASTNode *world, const char *state_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
find_world_state_before_local(ASTNode *world, const char *state_name, size_t limit)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    if (limit > world->data.world_decl.state_count)
        limit = world->data.world_decl.state_count;

    for (size_t i = 0; i < limit; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
resolve_world_zone_decl_local(ASTNode *world, SemanticContext *ctx, const char *slot_name)
{
    ASTNode *slot;
    if (world == NULL || world->type != AST_WORLD_DECL
        || ctx == NULL || slot_name == NULL) {
        return NULL;
    }

    slot = find_world_zone_slot_local(world, slot_name);
    if (slot == NULL || slot->data.world_zone.zone_type == NULL)
        return NULL;

    return find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
        slot->data.world_zone.zone_type);
}

static const char *
resolve_world_plain_zone_input_name(ASTNode *world, const char *input_name)
{
    ASTNode *state;

    if (world == NULL || world->type != AST_WORLD_DECL || input_name == NULL)
        return NULL;

    if (find_world_zone_slot_local(world, input_name) != NULL)
        return input_name;

    state = find_world_state_local(world, input_name);
    if (state != NULL && state->type == AST_WORLD_STATE
        && state->data.world_state.source_kind == WORLD_STATE_SOURCE_ZONE) {
        return state->data.world_state.zone_slot_name;
    }

    return NULL;
}

static ASTNode *
find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_state_decl_local(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static bool
resolve_world_zone_state(ASTNode *world, ASTNode *site, const char *state_name,
                         SemanticContext *ctx, const char *action_name,
                         const char **zone_slot_name_out)
{
    ASTNode *state = find_world_state_local(world, state_name);
    if (state == NULL) {
        ASTNode *zone = find_world_zone_slot_local(world, state_name);
        if (zone != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = zone->data.world_zone.slot_name;
            return true;
        }
    }
    if (state == NULL) {
        semantic_error(ctx, site,
            "World %s references unknown zone/state '%s'",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (state->data.world_state.source_kind != WORLD_STATE_SOURCE_ZONE) {
        semantic_error(ctx, site,
            "World %s cannot target derived state '%s'; use the underlying zone slot or a plain 'state name: zone slot' alias",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = state->data.world_state.zone_slot_name;
    return true;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.world_decl.name;
    ASTNode *saved_world = ctx->current_world;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_WORLD;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_WORLD;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of world '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    ctx->current_world = node;

    /* Check roster references */
    for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
        ASTNode *ws = node->data.world_decl.rosters[i];
        if (ws->data.world_roster.roster_type != NULL) {
            Symbol *sys = scope_lookup(ctx->scope,
                ws->data.world_roster.roster_type);
            if (sys == NULL || sys->kind != SYMBOL_ROSTER) {
                semantic_error(ctx, ws,
                    "Roster type '%s' not found for slot '%s'.\n"
                    "Reason:\n"
                    "- world roster slot '%s' must reference a visible roster declaration\n"
                    "- no roster named '%s' is visible at this point\n"
                    "Fix:\n"
                    "- declare roster '%s'\n"
                    "- or import/export the module that defines it",
                    ws->data.world_roster.roster_type,
                    ws->data.world_roster.slot_name,
                    ws->data.world_roster.slot_name,
                    ws->data.world_roster.roster_type,
                    ws->data.world_roster.roster_type);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        if (wz->data.world_zone.zone_type != NULL) {
            Symbol *zone = scope_lookup(ctx->scope,
                wz->data.world_zone.zone_type);
            ASTNode *zone_decl = find_domain_decl_by_name(
                ctx->program_root, AST_ZONE_DECL, wz->data.world_zone.zone_type);
            if ((zone == NULL || zone->kind != SYMBOL_ZONE)
                && !(zone_decl != NULL
                     && zone_decl->type == AST_ZONE_DECL
                     && explicit_type_reference_allowed(zone_decl, node, ctx))) {
                semantic_error(ctx, wz,
                    "Zone type '%s' not found for slot '%s'.\n"
                    "Reason:\n"
                    "- world zone slot '%s' must reference a visible zone declaration\n"
                    "- no zone named '%s' is visible at this point\n"
                    "Fix:\n"
                    "- declare zone '%s'\n"
                    "- or import/export the module that defines it",
                    wz->data.world_zone.zone_type,
                    wz->data.world_zone.slot_name,
                    wz->data.world_zone.slot_name,
                    wz->data.world_zone.zone_type,
                    wz->data.world_zone.zone_type);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        const char *zone_slot_name = state->data.world_state.zone_slot_name;
        ASTNode *zone_decl = NULL;
        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            bool saw_direct_zone_input = false;
            bool saw_state_input = false;
            if (state->data.world_state.input_count == 0) {
                semantic_error(ctx, state,
                    "Composed world state '%s' must reference at least one zone/state input.\n"
                    "Reason:\n"
                    "- `all`/`any` world-state composition needs at least one upstream zone/state contract\n"
                    "- an empty composition cannot produce observable world state\n"
                    "Fix:\n"
                    "- add one or more world zone/state inputs to '%s'\n"
                    "- or replace it with a plain zone/state alias",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                if (input_name == NULL)
                    continue;
                if (state->data.world_state.state_name != NULL
                    && strcmp(state->data.world_state.state_name, input_name) == 0) {
                    semantic_error(ctx, state,
                        "Composed world state '%s' cannot reference itself.\n"
                        "Reason:\n"
                        "- world-state composition must depend on earlier zone/state inputs\n"
                        "- self-reference would make the contract cyclic and under-specified\n"
                        "Fix:\n"
                        "- remove '%s' from its own input list\n"
                        "- or split the state into earlier intermediate aliases",
                        input_name,
                        input_name);
                    continue;
                }
                if (find_world_zone_slot_local(node, input_name) != NULL)
                {
                    saw_direct_zone_input = true;
                    continue;
                }
                if (find_world_state_before_local(node, input_name, i) != NULL)
                {
                    saw_state_input = true;
                    continue;
                }
                semantic_error(ctx, state,
                    "Composed world state '%s' references unknown or later world zone/state '%s'.\n"
                    "Reason:\n"
                    "- composed world state inputs must name either a declared zone slot or an earlier world-state alias\n"
                    "- '%s' is not visible as a prior input at this point\n"
                    "Fix:\n"
                    "- reference a declared world zone slot\n"
                    "- or move the producer state before '%s'\n"
                    "- or correct the input name",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>",
                    input_name,
                    input_name,
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                const char *input_plain_zone;
                if (input_name == NULL)
                    continue;
                input_plain_zone = resolve_world_plain_zone_input_name(node, input_name);
                for (size_t prev_i = 0; prev_i < input_i; prev_i++) {
                    const char *prev_name = state->data.world_state.input_names[prev_i];
                    const char *prev_plain_zone;
                    if (prev_name == NULL)
                        continue;
                    if (strcmp(prev_name, input_name) == 0) {
                        semantic_warning(ctx, state,
                            "Composed world state '%s' repeats input '%s'.\n"
                            "Reason:\n"
                            "- the same upstream zone/state input appears more than once\n"
                            "- duplicate inputs do not add meaning and make provenance noisier\n"
                            "Fix:\n"
                            "- keep a single '%s' input in world state '%s'\n"
                            "- or collapse duplicate entries before composition",
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>",
                            input_name,
                            input_name,
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>");
                        break;
                    }
                    prev_plain_zone = resolve_world_plain_zone_input_name(node, prev_name);
                    if (input_plain_zone != NULL && prev_plain_zone != NULL
                        && strcmp(input_plain_zone, prev_plain_zone) == 0) {
                        semantic_warning(ctx, state,
                            "Composed world state '%s' redundantly mixes zone slot '%s' with plain alias/input '%s'.\n"
                            "Reason:\n"
                            "- both inputs resolve to the same underlying zone source\n"
                            "- mixing the raw zone slot and its plain alias duplicates provenance without changing semantics\n"
                            "Fix:\n"
                            "- keep either zone slot '%s' or alias/input '%s'\n"
                            "- prefer one canonical upstream name per composed world state",
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>",
                            prev_name,
                            input_name,
                            prev_name,
                            input_name);
                        break;
                    }
                }
                if (find_world_zone_slot_local(node, input_name) != NULL) {
                    semantic_warning(ctx, state,
                        "Composed world state '%s' directly references zone slot '%s'; prefer a plain 'state name: zone %s' alias so command and derived layers stay separate.\n"
                        "Reason:\n"
                        "- raw zone slots blur command-layer input with derived world-state composition\n"
                        "- a plain alias keeps provenance and layering clearer\n"
                        "Fix:\n"
                        "- introduce 'state name: zone %s'\n"
                        "- and compose from that alias instead of the raw zone slot",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        input_name,
                        input_name,
                        input_name);
                }
            }
            if (saw_direct_zone_input && saw_state_input) {
                semantic_warning(ctx, state,
                    "Composed world state '%s' mixes direct zone-slot inputs with world-state inputs; prefer composing from world-state aliases only.\n"
                    "Reason:\n"
                    "- mixing raw zone slots with world-state aliases makes composition provenance harder to read\n"
                    "- world-state aliases are the preferred boundary between command and derived layers\n"
                    "Fix:\n"
                    "- compose from world-state aliases only\n"
                    "- or keep the composition entirely in raw zone-slot space, but not both",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
        } else if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, state,
                "World state '%s' references unknown zone slot '%s'",
                state->data.world_state.state_name != NULL
                    ? state->data.world_state.state_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        } else {
            zone_decl = resolve_world_zone_decl_local(node, ctx, zone_slot_name);
        }

        if (zone_decl != NULL && state->data.world_state.detail_name != NULL) {
            const char *detail_name = state->data.world_state.detail_name;
            ASTNode *detail_decl = NULL;

            if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                detail_decl = find_zone_domain_slot(zone_decl, detail_name);
                if (detail_decl == NULL
                    || detail_decl->data.domain_slot.is_subject) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown object/tobject projection slot '%s' in zone '%s'.\n"
                        "Reason:\n"
                        "- world state projection visibility can only observe object/tobject projection slots\n"
                        "- zone '%s' has no matching projection slot '%s'\n"
                        "Fix:\n"
                        "- reference a declared object/tobject projection slot from zone '%s'\n"
                        "- or declare projection slot '%s' on that zone",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name,
                        zone_slot_name,
                        detail_name,
                        zone_slot_name,
                        detail_name);
                }
            } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                detail_decl = find_zone_layer_slot_local(zone_decl, detail_name);
                if (detail_decl == NULL) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown layer slot '%s' in zone '%s'.\n"
                        "Reason:\n"
                        "- world layer visibility expects a declared zone layer slot\n"
                        "- zone '%s' has no layer slot named '%s'\n"
                        "Fix:\n"
                        "- reference a declared layer slot from zone '%s'\n"
                        "- or declare layer slot '%s' on that zone",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name,
                        zone_slot_name,
                        detail_name,
                        zone_slot_name,
                        detail_name);
                }
            } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                detail_decl = find_zone_state_decl_local(zone_decl, detail_name);
                if (detail_decl == NULL) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown zone state '%s' in zone '%s'.\n"
                        "Reason:\n"
                        "- world state composition expects a declared zone state alias\n"
                        "- zone '%s' has no state named '%s'\n"
                        "Fix:\n"
                        "- reference a declared zone state from zone '%s'\n"
                        "- or declare zone state '%s' before composing it into the world",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name,
                        zone_slot_name,
                        detail_name,
                        zone_slot_name,
                        detail_name);
                }
            }
        }
        for (size_t j = i + 1; j < node->data.world_decl.state_count; j++) {
            ASTNode *other = node->data.world_decl.states[j];
            if (other != NULL
                && other->data.world_state.state_name != NULL
                && state->data.world_state.state_name != NULL
                && strcmp(other->data.world_state.state_name,
                          state->data.world_state.state_name) == 0) {
                semantic_error(ctx, other,
                    "Redeclaration of world state '%s'",
                    state->data.world_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *activate = node->data.world_decl.activations[i];
        const char *zone_slot_name = activate->data.world_activate.zone_slot_name;
        if (activate->data.world_activate.state_name != NULL) {
            if (!resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, activate,
                "World activate references unknown zone slot '%s'.\n"
                "Reason:\n"
                "- activate needs a declared world zone slot or zone-state alias\n"
                "- '%s' does not resolve to any visible world zone slot here\n"
                "Fix:\n"
                "- activate a declared world zone slot\n"
                "- or correct the referenced zone/state alias",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.activate_count; j++) {
            ASTNode *other = node->data.world_decl.activations[j];
            const char *other_zone = other->data.world_activate.zone_slot_name;
            if (other->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' activates zone '%s' more than once.\n"
                    "Reason:\n"
                    "- the same world zone slot is activated by multiple lifecycle entries\n"
                    "- repeated activation does not add meaning and obscures lifecycle intent\n"
                    "Fix:\n"
                    "- keep a single activate entry for zone '%s'\n"
                    "- or collapse duplicates into one lifecycle rule",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and deactivates zone '%s'; choose one lifecycle direction.\n"
                    "Reason:\n"
                    "- the same world zone slot appears in both activate and deactivate lifecycle paths\n"
                    "- this makes the lifecycle contract contradictory\n"
                    "Fix:\n"
                    "- keep only activate or deactivate for zone '%s'\n"
                    "- or split the behavior into separate world-state aliases",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *maintain = node->data.world_decl.maintained_zones[j];
            const char *other_zone = maintain->data.world_maintain.zone_slot_name;
            if (maintain->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, maintain,
                    maintain->data.world_maintain.state_name, ctx, "maintain",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and maintains zone '%s'; maintain already implies the continuing direction.\n"
                    "Reason:\n"
                    "- `maintain` already carries the continuing lifecycle direction for that zone\n"
                    "- adding an explicit activate for the same zone makes the contract redundant\n"
                    "Fix:\n"
                    "- keep `maintain` for zone '%s'\n"
                    "- or drop `maintain` and keep only the activate rule if that is the intended contract",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = node->data.world_decl.deactivations[i];
        const char *zone_slot_name = deactivate->data.world_deactivate.zone_slot_name;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            if (!resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, deactivate,
                "World deactivate references unknown zone slot '%s'.\n"
                "Reason:\n"
                "- deactivate needs a declared world zone slot or zone-state alias\n"
                "- '%s' does not resolve to any visible world zone slot here\n"
                "Fix:\n"
                "- deactivate a declared world zone slot\n"
                "- or correct the referenced zone/state alias",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *other = node->data.world_decl.deactivations[j];
            const char *other_zone = other->data.world_deactivate.zone_slot_name;
            if (other->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' deactivates zone '%s' more than once.\n"
                    "Reason:\n"
                    "- the same world zone slot is deactivated by multiple lifecycle entries\n"
                    "- repeated deactivation does not add meaning and obscures lifecycle intent\n"
                    "Fix:\n"
                    "- keep a single deactivate entry for zone '%s'\n"
                    "- or collapse duplicates into one lifecycle rule",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = node->data.world_decl.maintained_zones[i];
        const char *zone_slot_name = maintain->data.world_maintain.zone_slot_name;
        const char *state_name = maintain->data.world_maintain.state_name;
        if (state_name != NULL) {
            if (!resolve_world_zone_state(node, maintain, state_name, ctx, "maintain",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "World maintain references unknown zone slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared world zone slot or zone-state alias\n"
                "- '%s' does not resolve to any visible world zone slot here\n"
                "Fix:\n"
                "- maintain a declared world zone slot\n"
                "- or correct the referenced zone/state alias",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *other = node->data.world_decl.maintained_zones[j];
            const char *other_zone = other->data.world_maintain.zone_slot_name;
            if (other->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_maintain.state_name, ctx, "maintain", &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' maintains zone '%s' more than once.\n"
                    "Reason:\n"
                    "- the same world zone slot is maintained by multiple lifecycle entries\n"
                    "- repeated maintain rules do not add meaning and obscure provenance\n"
                    "Fix:\n"
                    "- keep a single maintain entry for zone '%s'\n"
                    "- or collapse duplicates into one lifecycle rule",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and deactivates zone '%s'; choose one lifecycle direction.\n"
                    "Reason:\n"
                    "- `maintain` keeps the zone active across the world lifecycle\n"
                    "- `deactivate` closes that same lifecycle path\n"
                    "- using both on the same zone makes the contract contradictory\n"
                    "Fix:\n"
                    "- keep only maintain or deactivate for zone '%s'\n"
                    "- or split the behavior into separate world-state aliases",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.activate_count; j++) {
            ASTNode *activate = node->data.world_decl.activations[j];
            const char *other_zone = activate->data.world_activate.zone_slot_name;
            if (activate->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and activates zone '%s'; maintain already implies the continuing direction.\n"
                    "Reason:\n"
                    "- `maintain` already expresses the continuing active lifecycle for that zone\n"
                    "- adding activate for the same zone makes the contract redundant\n"
                    "Fix:\n"
                    "- keep `maintain` for zone '%s'\n"
                    "- or drop `maintain` and keep only the activate rule if that is the intended contract",
                    node->data.world_decl.name, zone_slot_name,
                    zone_slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        type_check_func_decl(node->data.world_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_world = saved_world;
    return !ctx->has_error;
}

static bool
type_check_domain_slots(ASTNode **slots,
                        size_t slot_count,
                        SemanticContext *ctx,
                        const char *kind_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot->data.domain_slot.is_subject
            && !type_is_subject_type(slot_type, ctx)) {
            semantic_error(ctx, slot,
                "%s subject slot '%s' requires a subject type",
                kind_name,
                slot->data.domain_slot.slot_name);
        } else if (slot->data.domain_slot.is_vessel) {
            ASTNode *slot_decl = NULL;
            if (slot_type != NULL && slot_type->kind == TYPE_KIND_CLASS
                && slot_type->name != NULL) {
                slot_decl = find_type_decl_by_name(ctx->program_root, slot_type->name);
            }
            if (slot_decl == NULL || slot_decl->type != AST_CLASS_DECL
                || slot_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
                semantic_error(ctx, slot,
                    "%s vessel slot '%s' requires a vessel type",
                    kind_name,
                    slot->data.domain_slot.slot_name);
            }
        }
    }

    return !ctx->has_error;
}

static bool
type_check_domain_slot_initializers(ASTNode **slots,
                                    size_t slot_count,
                                    SemanticContext *ctx,
                                    const char *kind_name)
{
    if (slots == NULL || ctx == NULL)
        return true;

    scope_enter(&ctx->scope, SCOPE_BLOCK);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Symbol *sym;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }

        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type == NULL || slot_type == TYPE_UNKNOWN)
            continue;

        sym = symbol_create_variable(slot->data.domain_slot.slot_name, slot_type,
            slot->line, slot->column);
        if (sym == NULL)
            continue;

        if (!scope_declare(ctx->scope, sym)) {
            Symbol *existing = scope_lookup_current(ctx->scope,
                slot->data.domain_slot.slot_name);
            if (existing != NULL
                && existing->type != NULL
                && type_equals(existing->type, slot_type)) {
                symbol_destroy(sym);
                continue;
            }
            semantic_error(ctx, slot,
                "Redeclaration of %s slot '%s'",
                kind_name,
                slot->data.domain_slot.slot_name);
            symbol_destroy(sym);
        }
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Type *init_type;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.initializer == NULL) {
            continue;
        }

        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        init_type = type_check_expression(slot->data.domain_slot.initializer, ctx);
        if (slot_type == NULL || init_type == NULL
            || slot_type == TYPE_UNKNOWN || init_type == TYPE_UNKNOWN) {
            continue;
        }

        require_assignable(init_type, slot_type,
            slot->data.domain_slot.initializer, ctx);
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

static size_t
