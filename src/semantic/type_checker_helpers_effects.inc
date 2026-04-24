mark_world_embedded_zone_arguments(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *world_decl;
    const char *world_name = NULL;

    if (call == NULL || call->type != AST_CALL || ctx == NULL
        || call->data.call.callee == NULL
        || call->data.call.callee->type != AST_IDENTIFIER) {
        return;
    }

    world_name = call->data.call.callee->data.identifier.name;
    world_decl = find_domain_decl_by_name(ctx->program_root, AST_WORLD_DECL,
        world_name);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        ASTNode *arg = call->data.call.arguments[i];
        Symbol *arg_sym;
        bool matched_zone_slot = false;

        if (arg == NULL || arg->type != AST_IDENTIFIER)
            continue;

        arg_sym = scope_lookup(ctx->scope, arg->data.identifier.name);
        if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE
            || arg_sym->type == NULL || arg_sym->type->name == NULL) {
            if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE)
                continue;
        }

        if (i < world_decl->data.world_decl.zone_count) {
            ASTNode *zone_slot = world_decl->data.world_decl.zones[i];
            if (zone_slot != NULL && zone_slot->type == AST_WORLD_ZONE
                && zone_slot->data.world_zone.zone_type != NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, arg,
                    "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                    "Reason:\n"
                    "- origin binding is '%s'\n"
                    "Contract source:\n"
                    "- world '%s' zone slot '%s'\n"
                    "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                    "- ownership/authority after construction belongs to the world-owned slot, not the origin binding\n"
                    "- owned embedding hands authority-bearing visibility to the world-owned zone slot\n"
                    "- mutating the original binding afterwards would diverge from the world-owned handoff destination\n"
                    "Fix:\n"
                    "- use Clone(%s) for an explicit copy\n"
                    "- or construct the zone inline / mutate it through the owning world after embedding",
                    world_name != NULL ? world_name : "<world>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg->data.identifier.name,
                    world_name,
                    zone_slot->data.world_zone.slot_name);
                matched_zone_slot = true;
            }
        }

        if (matched_zone_slot)
            continue;

        for (size_t zi = 0; zi < world_decl->data.world_decl.zone_count; zi++) {
            ASTNode *zone_slot = world_decl->data.world_decl.zones[zi];
            if (zone_slot == NULL || zone_slot->type != AST_WORLD_ZONE
                || zone_slot->data.world_zone.zone_type == NULL) {
                continue;
            }
            if (arg_sym->type != NULL && arg_sym->type->name != NULL
                && strcmp(arg_sym->type->name, zone_slot->data.world_zone.zone_type) == 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, arg,
                    "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                    "Reason:\n"
                    "- origin binding is '%s'\n"
                    "Contract source:\n"
                    "- world '%s' zone slot '%s'\n"
                    "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                    "- ownership/authority after construction belongs to the world-owned slot, not the origin binding\n"
                    "- owned embedding hands authority-bearing visibility to the world-owned zone slot\n"
                    "- mutating the original binding afterwards would diverge from the world-owned handoff destination\n"
                    "Fix:\n"
                    "- use Clone(%s) for an explicit copy\n"
                    "- or construct the zone inline / mutate it through the owning world after embedding",
                    world_name != NULL ? world_name : "<world>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg->data.identifier.name,
                    world_name,
                    zone_slot->data.world_zone.slot_name);
                break;
            }
        }
    }
}

static uint32_t
parse_effect_word(const char *word)
{
    if (word == NULL || *word == '\0')
        return EFFECT_NONE;
    if (strcmp(word, "secure") == 0)
        return EFFECT_SECURE;
    if (strcmp(word, "remote") == 0)
        return EFFECT_REMOTE;
    if (strcmp(word, "nondeterministic") == 0)
        return EFFECT_NONDETERMINISTIC;
    if (strcmp(word, "collapse") == 0)
        return EFFECT_COLLAPSE;
    if (strcmp(word, "local") == 0)
        return EFFECT_NONE;
    return UINT32_MAX;
}

static void
append_effect_name(char *buf, size_t buf_size, const char *name, bool *first)
{
    if (buf == NULL || buf_size == 0 || name == NULL || first == NULL)
        return;

    if (!*first)
        strncat(buf, ", ", buf_size - strlen(buf) - 1);
    strncat(buf, name, buf_size - strlen(buf) - 1);
    *first = false;
}

static void
effect_mask_to_string(uint32_t mask, char *buf, size_t buf_size)
{
    bool first = true;

    if (buf == NULL || buf_size == 0)
        return;

    buf[0] = '\0';
    mask = type_effect_mask_closure(mask);
    if (mask == EFFECT_NONE) {
        strncat(buf, "local", buf_size - 1);
        return;
    }

    if (type_effect_mask_has(mask, EFFECT_SECURE))
        append_effect_name(buf, buf_size, "secure", &first);
    if (type_effect_mask_has(mask, EFFECT_REMOTE))
        append_effect_name(buf, buf_size, "remote", &first);
    if (type_effect_mask_has(mask, EFFECT_NONDETERMINISTIC))
        append_effect_name(buf, buf_size, "nondeterministic", &first);
    if (type_effect_mask_has(mask, EFFECT_COLLAPSE))
        append_effect_name(buf, buf_size, "collapse", &first);
}

static uint32_t
effects_from_structured_comment(StructuredComment *comment,
                                SemanticContext *ctx,
                                const ASTNode *site)
{
    uint32_t mask = EFFECT_NONE;

    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag == NULL || tag->type != DOC_TAG_EFFECTS || tag->content == NULL)
                continue;

            char *copy = pergyra_strdup(tag->content);
            char *tok = copy != NULL ? strtok(copy, ", \t\r\n|") : NULL;
            while (tok != NULL) {
                for (char *p = tok; *p != '\0'; p++)
                    *p = (char)tolower((unsigned char)*p);

                uint32_t effect = parse_effect_word(tok);
                if (effect == UINT32_MAX) {
                    semantic_warning(ctx, site,
                        "Unknown effect tag '%s' in structured comment; expected secure, remote, nondeterministic, collapse, or local",
                        tok);
                } else {
                    mask |= effect;
                }
                tok = strtok(NULL, ", \t\r\n|");
            }
            free(copy);
        }
    }

    return mask;
}

static bool
structured_comment_has_effects_tag(StructuredComment *comment)
{
    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag != NULL && tag->type == DOC_TAG_EFFECTS)
                return true;
        }
    }
    return false;
}

uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out)
{
    bool has_doc_contract;
    bool has_sig_contract;
    uint32_t doc_effects;
    uint32_t sig_effects;

    if (node == NULL)
        return EFFECT_NONE;

    has_doc_contract = structured_comment_has_effects_tag(node->data.func_decl.doc_comment);
    has_sig_contract = node->data.func_decl.has_effects_clause;
    doc_effects = effects_from_structured_comment(node->data.func_decl.doc_comment, ctx, node);
    sig_effects = node->data.func_decl.declared_effects;

    if (has_contract_out != NULL)
        *has_contract_out = has_doc_contract || has_sig_contract;

    return type_effect_mask_closure(doc_effects | sig_effects);
}

void
semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return;
    ctx->current_function_effects =
        type_effect_mask_join(ctx->current_function_effects, effect_mask);
}

bool
type_is_constructed_named(const Type *type, const char *name)
{
    return type != NULL
        && type->kind == TYPE_KIND_CONSTRUCTED
        && type->data.constructed.constructor != NULL
        && strcmp(type->data.constructed.constructor->name, name) == 0;
}

bool
type_is_qubit(const Type *type)
{
    if (type == NULL)
        return false;
    if (TYPE_QUBIT != NULL && type_equals(type, TYPE_QUBIT))
        return true;
    return type->name != NULL && strcmp(type->name, "QubitSlot") == 0;
}

bool
type_is_slot_handle(const Type *type)
{
    return type != NULL && type->kind == TYPE_KIND_SLOT;
}

bool
type_is_owned_slot_handle(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_OWNED;
}

bool
type_is_read_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_READ_VIEW;
}

bool
type_is_write_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_WRITE_VIEW;
}

bool
type_is_move_token(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_MOVE_TOKEN;
}

bool
type_is_resource_handle(const Type *type)
{
    return type_is_qubit(type)
        || type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_anchored_resource_handle(const Type *type)
{
    return type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_movable_resource_handle(const Type *type)
{
    return type_is_qubit(type);
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_value_nominal_boundary_type(const Type *type, SemanticContext *ctx);

bool
type_is_general_boundary_type(const Type *type, SemanticContext *ctx)
{
    return type_is_movable_resource_handle(type)
        || type_is_subject_type(type, ctx)
        || type_is_value_nominal_boundary_type(type, ctx);
}

bool
type_requires_boundary_borrow_tracking(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || !type_is_general_boundary_type(type, ctx))
        return false;
    if (type_is_subject_type(type, ctx) || type_is_movable_resource_handle(type))
        return false;

    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_ENUM:
        return false;
    default:
        return true;
    }
}

bool
type_is_capability_bearing(const Type *type)
{
    return (type_is_owned_slot_handle(type) && type->data.slot.is_secure)
        || type_is_constructed_named(type, "Token");
}

const char *
resource_handle_display_name(const Type *type)
{
    if (type == NULL)
        return "resource";
    if (type_is_qubit(type))
        return "QubitSlot";
    if (type_is_read_view(type))
        return "ReadView";
    if (type_is_write_view(type))
        return "WriteView";
    if (type_is_move_token(type))
        return "MoveToken";
    if (type_is_owned_slot_handle(type))
        return type->data.slot.is_secure ? "SecureSlot" : "Slot";
    if (type_is_constructed_named(type, "DeviceSlot"))
        return "DeviceSlot";
    return type->name != NULL ? type->name : "resource";
}

ASTNode *
find_type_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL)
            continue;
        if (stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

ASTNode *
find_ability_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL
            || stmt->data.ability_decl.name == NULL)
            continue;
        if (strcmp(stmt->data.ability_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

TypeNominalFlavor
nominal_flavor_from_decl(const ASTNode *decl)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return TYPE_NOMINAL_NONE;

    switch (decl->data.class_decl.nominal_kind) {
    case NOMINAL_DECL_SUBJECT:
        return TYPE_NOMINAL_SUBJECT;
    case NOMINAL_DECL_VESSEL:
        return TYPE_NOMINAL_VESSEL;
    case NOMINAL_DECL_STRUCT:
        return TYPE_NOMINAL_STRUCT;
    case NOMINAL_DECL_OBJECT:
        return TYPE_NOMINAL_OBJECT;
    case NOMINAL_DECL_TOBJECT:
        return TYPE_NOMINAL_TOBJECT;
    case NOMINAL_DECL_CLASS:
    default:
        return TYPE_NOMINAL_CLASS;
    }
}

static bool
decl_is_subject_type(const ASTNode *decl)
{
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
}

bool
decl_is_subject_host(const ASTNode *decl)
{
    return decl_is_subject_type(decl);
}

ASTNode *
find_subject_host_decl_by_name(ASTNode *program, const char *type_name)
{
    ASTNode *decl = find_type_decl_by_name(program, type_name);
    if (decl_is_subject_host(decl))
        return decl;
    return NULL;
}

ASTNode *
find_callable_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_FUNC_DECL
            && stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, name) == 0)
            return stmt;
        if (stmt->type == AST_EVENT_DECL
            && stmt->data.event_decl.name != NULL
            && strcmp(stmt->data.event_decl.name, name) == 0)
            return stmt;
        if (stmt->type == AST_INTENT_DECL
            && stmt->data.intent_decl.name != NULL
            && strcmp(stmt->data.intent_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

bool
decl_is_projection_source(const ASTNode *decl)
{
    if (decl_is_subject_host(decl))
        return true;
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
}

ASTNode *
find_domain_decl_by_name(ASTNode *program, ASTNodeType decl_type,
                         const char *name);

ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name);

ClassField *
subject_host_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static size_t
projection_source_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
projection_source_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static int
resolve_projection_source_field_path_rec(ASTNode *program_root,
                                         ASTNode *source_decl,
                                         const char *field_name,
                                         unsigned depth,
                                         SemanticContext *ctx,
                                         char **path_out,
                                         Type **field_type_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;
    Type *resolved_type = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (field_type_out != NULL)
        *field_type_out = NULL;

    if (program_root == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            if (field_type_out != NULL)
                *field_type_out = field->type != NULL
                    ? resolve_type_node(field->type, ctx)
                    : TYPE_UNKNOWN;
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        Type *nested_type = NULL;
        char *prefixed_path;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->name == NULL || field->type == NULL
            || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_type_decl_by_name(program_root, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_field_path_rec(
            program_root, vessel_decl, field_name, depth + 1, ctx,
            &nested_path, &nested_type);
        if (nested_status != 1) {
            if (nested_path != NULL)
                free(nested_path);
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = tc_strdup_fmt("%s.%s", field->name, nested_path);
        free(nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
            resolved_type = nested_type;
        } else {
            free(prefixed_path);
            free(resolved_path);
            resolved_path = NULL;
            resolved_type = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        else
            free(resolved_path);
        if (field_type_out != NULL)
            *field_type_out = resolved_type;
        return 1;
    }

    if (resolved_path != NULL)
        free(resolved_path);
    return match_count > 1 ? 2 : 0;
}

int
resolve_projection_source_field_path(ASTNode *program_root,
                                     ASTNode *source_decl,
                                     const char *field_name,
                                     SemanticContext *ctx,
                                     char **path_out,
                                     Type **field_type_out)
{
    return resolve_projection_source_field_path_rec(program_root, source_decl,
        field_name, 0, ctx, path_out, field_type_out);
}
