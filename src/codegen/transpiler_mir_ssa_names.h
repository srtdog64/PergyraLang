static const char *
transpiler_resolve_ssa_name(const TranspilerSSANameMap *ssa_map,
                            const char *base_name)
{
    size_t idx;
    size_t attempts;

    if (ssa_map == NULL || base_name == NULL)
        return NULL;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        const TranspilerSSANameBucket *bucket = &ssa_map->buckets[idx];
        if (!bucket->in_use)
            return NULL;
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0)
            return bucket->versioned_name;
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_method(const TranspilerCtx *ctx,
                           const char *owner_name,
                           const ASTNode *method_decl)
{
    const char *target = NULL;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    target = method_decl->data.func_decl.name;
    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_METHOD)
            continue;
        if (routine->ast == method_decl)
            return routine;
        if (routine->name != NULL
            && routine->owner_name != NULL
            && strcmp(routine->name, target) == 0
            && strcmp(routine->owner_name, owner_name) == 0) {
            return routine;
        }
    }

    return NULL;
}

static bool
transpiler_name_is_token_local(const char *name);

static bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name);

static const char *
transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
                                   const char *base_name)
{
    const char *resolved;
    if (ctx == NULL || ctx->active_ssa_map == NULL || base_name == NULL)
        return NULL;
    if (transpiler_is_implicit_field((TranspilerCtx *)ctx, base_name))
        return NULL;
    resolved = transpiler_resolve_ssa_name(
        (const TranspilerSSANameMap *)ctx->active_ssa_map,
        base_name);
    if (resolved != NULL)
        return resolved;
    if (transpiler_name_is_token_local(base_name))
        return base_name;
    return NULL;
}

static bool
transpiler_name_is_token_local(const char *name)
{
    const char *tag;
    if (name == NULL)
        return false;
    tag = strstr(name, "_token");
    if (tag == NULL)
        return false;
    tag += 6;
    while (*tag != '\0') {
        if (*tag < '0' || *tag > '9')
            return false;
        tag++;
    }
    return true;
}

static bool
transpiler_type_name_is_slot_like(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return strncmp(type_name, "Slot<", 5) == 0
        || strncmp(type_name, "SecureSlot<", 11) == 0
        || strncmp(type_name, "DeviceSlot<", 11) == 0;
}

/* Locals whose declaration is emitted alongside a slot claim (slots,
 * tokens, views) must not get an SSA pre-allocation in the MIR header
 * because the claim emit provides the real declaration. */
static bool
transpiler_type_name_is_claim_shape(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return transpiler_type_name_is_slot_like(type_name)
        || strncmp(type_name, "Token<", 6) == 0;
}

static bool
transpiler_block_has_claim_for_slot_local(const MIRBasicBlock *block,
                                          const char *slot_name)
{
    if (block == NULL || slot_name == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        const char *claim_name;
        if (inst->kind != MIR_INST_RESOURCE_OP
            || inst->name == NULL
            || strcmp(inst->name, "Claim") != 0) {
            continue;
        }
        claim_name = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
        if (claim_name != NULL && strcmp(claim_name, slot_name) == 0)
            return true;
    }
    return false;
}

static bool
transpiler_mir_routine_has_explicit_cfg(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    if (routine->block_count > 1)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_BRANCH
                || inst->kind == MIR_INST_PHI
                || inst->kind == MIR_INST_CLEANUP_EDGE) {
                return true;
            }
        }
    }
    return false;
}

static bool
transpiler_emit_mir_block_with_ssa_map(TranspilerSSANameMap *ssa_map,
                                       const MIRBasicBlock *block)
{
    const char *base_names[TRANSPILE_SSA_MAP_CAPACITY];
    const char *versioned_names[TRANSPILE_SSA_MAP_CAPACITY];
    size_t map_count = 0;

    transpiler_ssa_map_clear(ssa_map);
    if (block == NULL || block->ssa_entry_values == NULL || block->ssa_entry_value_count == 0)
        return true;
    if (!transpiler_collect_ssa_name_entries(block->ssa_entry_values, block->ssa_entry_value_count,
                                            base_names, versioned_names,
                                            TRANSPILE_SSA_MAP_CAPACITY,
                                            &map_count)) {
        transpiler_free_ssa_name_entries(base_names, map_count);
        return false;
    }
    if (!transpiler_rebuild_ssa_map(ssa_map, base_names, versioned_names, map_count))
        return false;
    transpiler_free_ssa_name_entries(base_names, map_count);
    return true;
}

static char *
transpiler_make_c_ssa_name(const char *versioned_name)
{
    if (versioned_name == NULL)
        return NULL;
    char base[128];
    size_t version = 0;
    if (!transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version))
        return pergyra_strdup(versioned_name);
    if (g_type_render_ctx != NULL && transpiler_is_implicit_field(g_type_render_ctx, base)) {
        if (current_class_has_field(g_type_render_ctx, base)) {
            return strdup_fmt(current_class_uses_self_cell(g_type_render_ctx)
                ? "self->%s"
                : "self.%s", base);
        }
        return strdup_fmt("self->%s", base);
    }
    return strdup_fmt("_pgy_ssa_%s_%zu", base, version);
}

static bool
transpiler_is_implicit_field(TranspilerCtx *ctx, const char *base_name)
{
    ASTNode *host_decl = NULL;
    bool in_zone_context = false;
    const char *host_name = NULL;

    if (ctx == NULL || base_name == NULL)
        return false;
    if (strcmp(base_name, "self") == 0)
        return false;
    if (ctx->current_func_decl != NULL
        && transpiler_has_explicit_local_binding(ctx->current_func_decl, base_name))
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    in_zone_context = (host_decl != NULL && host_decl->type == AST_ZONE_DECL);
    host_name = transpiler_decl_name_local(host_decl);
    if (current_class_has_field(ctx, base_name))
        return true;
    if (current_relation_has_field(ctx, base_name))
        return true;
    if (current_effect_has_field(ctx, base_name))
        return true;
    if (in_zone_context) {
        if (current_zone_has_field(ctx, base_name))
            return true;
        if (lookup_typed_var(ctx, base_name) == NULL && !is_slot_var(ctx, base_name))
            return true;
    }
    if (transpiler_current_world_has_field(ctx, base_name))
        return true;
    if (!in_zone_context && host_name != NULL) {
        ASTNode *zone_decl = find_zone_decl(ctx, host_name);
        if (zone_decl != NULL) {
            for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
                ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, base_name) == 0) {
                    return true;
                }
            }
            for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
                ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
                if (slot != NULL && slot->data.zone_layer_slot.slot_name != NULL
                    && strcmp(slot->data.zone_layer_slot.slot_name, base_name) == 0) {
                    return true;
                }
            }
            for (size_t i = 0; i < zone_decl->data.zone_decl.shared_count; i++) {
                ASTNode *shared = zone_decl->data.zone_decl.shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, base_name) == 0) {
                    return true;
                }
            }
        }
        if (host_name != NULL) {
            size_t name_len = strlen(host_name);
            if (name_len > 4
                && strcmp(host_name + name_len - 4, "Zone") == 0
                && lookup_typed_var(ctx, base_name) == NULL
                && !is_slot_var(ctx, base_name)) {
                return true;
            }
        }
    }
    return false;
}

static char *
transpiler_render_ssa_name(TranspilerCtx *ctx, const char *versioned_name)
{
    char base[128];
    size_t version = 0;

    if (ctx != NULL
        && versioned_name != NULL
        && transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version)
        && transpiler_is_implicit_field(ctx, base)) {
        if (current_class_has_field(ctx, base)) {
            return strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", base);
        }
        return strdup_fmt("self->%s", base);
    }
    return transpiler_make_c_ssa_name(versioned_name);
}

static bool
transpiler_versioned_name_list_contains(const char **names,
                                        size_t count,
                                        const char *name)
{
    if (names == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static bool
transpiler_versioned_name_list_add(const char **names,
                                   size_t *count,
                                   size_t capacity,
                                   const char *name)
{
    if (names == NULL || count == NULL || name == NULL)
        return false;
    if (transpiler_versioned_name_list_contains(names, *count, name))
        return true;
    if (*count >= capacity)
        return false;
    names[(*count)++] = name;
    return true;
}

static bool
transpiler_c_type_uses_scalar_zero(const char *c_type)
{
    if (c_type == NULL)
        return true;
    if (strchr(c_type, '*') != NULL)
        return true;
    return strcmp(c_type, "int") == 0
           || strcmp(c_type, "int32_t") == 0
           || strcmp(c_type, "int64_t") == 0
           || strcmp(c_type, "size_t") == 0
           || strcmp(c_type, "bool") == 0
           || strcmp(c_type, "float") == 0
           || strcmp(c_type, "double") == 0;
}

static const char *
transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                           const ASTNode *func_decl,
                                           ASTNode *expr)
{
    const char *semantic_type = infer_expression_type_name(ctx, expr);
    if (semantic_type != NULL
        && semantic_type[0] != '\0'
        && strcmp(semantic_type, "Int") != 0)
        return semantic_type;
    if (expr == NULL)
        return NULL;
    switch (expr->type) {
        case AST_NUMBER:
            return expr->data.number.is_long ? "Long" : "Int";
        case AST_STRING:
            return "String";
        case AST_BOOLEAN:
            return "Bool";
        case AST_IDENTIFIER:
            return transpiler_find_local_type_name(ctx, func_decl, expr->data.identifier.name);
        case AST_MEMBER_ACCESS: {
            const char *resolved = transpiler_resolve_nominal_host_expr_type_name(ctx, expr);
            if (resolved != NULL && resolved[0] != '\0')
                return resolved;
            if (expr->data.member.object != NULL && expr->data.member.name != NULL) {
                const char *obj_type = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, expr->data.member.object);
                if (obj_type != NULL) {
                    ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                    if (obj_decl != NULL) {
                        for (size_t fi = 0; fi < obj_decl->data.class_decl.field_count; fi++) {
                            ClassField *f = obj_decl->data.class_decl.fields[fi];
                            if (f != NULL && f->name != NULL && f->type != NULL
                                && strcmp(f->name, expr->data.member.name) == 0) {
                                static char *rendered_member = NULL;
                                free(rendered_member);
                                rendered_member = render_type_name(f->type);
                                return rendered_member;
                            }
                        }
                    }
                }
            }
            return semantic_type != NULL && semantic_type[0] != '\0' ? semantic_type : NULL;
        }
        case AST_BINARY:
            switch (expr->data.binary.op.type) {
                case TOKEN_EQUAL:
                case TOKEN_NOT_EQUAL:
                case TOKEN_LESS:
                case TOKEN_GREATER:
                case TOKEN_LESS_EQUAL:
                case TOKEN_GREATER_EQUAL:
                case TOKEN_AND:
                case TOKEN_OR:
                    return "Bool";
                default:
                    return transpiler_infer_local_type_name_from_expr(ctx, func_decl, expr->data.binary.left);
            }
        case AST_UNARY:
            if (expr->data.unary.op.type == TOKEN_NOT)
                return "Bool";
            return transpiler_infer_local_type_name_from_expr(ctx, func_decl, expr->data.unary.operand);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_MEMBER_ACCESS
                && expr->data.call.callee->data.member.name != NULL) {
                ASTNode *receiver = expr->data.call.callee->data.member.object;
                const char *method_name = expr->data.call.callee->data.member.name;
                const char *receiver_type = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, receiver);
                ASTNode *method_decl = NULL;
                if (receiver_type != NULL
                    && method_name != NULL
                    && strcmp(method_name, "Slice") == 0
                    && (strncmp(receiver_type, "Array<", 6) == 0
                        || strncmp(receiver_type, "Slice<", 6) == 0)) {
                    static char rendered_slice[128];
                    const char *inner = slot_inner_type_name(receiver_type);
                    snprintf(rendered_slice, sizeof(rendered_slice), "Slice<%s>",
                        inner != NULL ? inner : "Int");
                    return rendered_slice;
                }
                if (receiver_type != NULL) {
                    method_decl = find_nominal_host_method_decl(ctx, receiver_type, method_name);
                }
                if (method_decl != NULL && method_decl->type == AST_FUNC_DECL
                    && method_decl->data.func_decl.return_type != NULL) {
                    static char *rendered_return = NULL;
                    free(rendered_return);
                    rendered_return = render_type_name(method_decl->data.func_decl.return_type);
                    return rendered_return;
                }
            }
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_IDENTIFIER
                && expr->data.call.callee->data.identifier.name != NULL) {
                const char *callee_name = expr->data.call.callee->data.identifier.name;
                ASTNode *callee_decl = find_function_decl(ctx, callee_name);
                if (callee_decl != NULL
                    && callee_decl->type == AST_FUNC_DECL
                    && callee_decl->data.func_decl.return_type != NULL) {
                    static char *rendered_func_return = NULL;
                    free(rendered_func_return);
                    rendered_func_return = render_type_name(callee_decl->data.func_decl.return_type);
                    return rendered_func_return;
                }
                if (find_class_decl(ctx, callee_name) != NULL
                    || find_zone_decl(ctx, callee_name) != NULL
                    || find_world_decl(ctx, callee_name) != NULL
                    || find_relation_decl(ctx, callee_name) != NULL
                    || find_effect_decl(ctx, callee_name) != NULL
                    || find_party_decl(ctx, callee_name) != NULL
                    || find_roster_decl(ctx, callee_name) != NULL) {
                    return callee_name;
                }
            }
            return semantic_type != NULL && semantic_type[0] != '\0' ? semantic_type : NULL;
        default:
            return NULL;
    }
}

static const char *
transpiler_find_local_type_name_in_block(TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         ASTNode *body,
                                         const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx,
                func_decl, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL) {
            static char *rendered = NULL;
            free(rendered);
            rendered = transpiler_render_effective_local_type_name(
                ctx, body->data.let_decl.type);
            return rendered;
        }
        return transpiler_infer_local_type_name_from_expr(ctx, func_decl, body->data.let_decl.initializer);
    }
    if (body->type == AST_LET_DESTRUCTURE) {
        /* let (a, b, c) = expr;
         * For Array<T>/Slice<T> each binding is T. For ClaimSecureSlot<T>
         * destructuring the first binding is SecureSlot<T> and the second
         * is Token<T>. */
        for (size_t i = 0; i < body->data.let_destructure.name_count; i++) {
            const char *pname = body->data.let_destructure.names[i];
            if (pname == NULL || strcmp(pname, base_name) != 0)
                continue;
            ASTNode *init = body->data.let_destructure.initializer;
            if (init != NULL
                && init->type == AST_CALL
                && init->data.call.callee != NULL
                && init->data.call.callee->type == AST_IDENTIFIER
                && init->data.call.callee->data.identifier.name != NULL) {
                const char *callee =
                    init->data.call.callee->data.identifier.name;
                if (strcmp(callee, "ClaimSecureSlot") == 0) {
                    static char rendered_secure[128];
                    const char *inner = "Int";
                    /* Prefer the explicit generic arg parsed from
                     * ClaimSecureSlot<T>(...). */
                    if (init->data.call.generic_args != NULL
                        && init->data.call.generic_args->count > 0
                        && init->data.call.generic_args->params[0] != NULL
                        && init->data.call.generic_args->params[0]->name != NULL) {
                        inner = init->data.call.generic_args->params[0]->name;
                    } else {
                        const char *init_type = infer_expression_type_name(ctx, init);
                        if (init_type != NULL && strncmp(init_type, "SecureSlot<", 11) == 0) {
                            const char *resolved_inner = slot_inner_type_name(init_type);
                            if (resolved_inner != NULL && resolved_inner[0] != '\0')
                                inner = resolved_inner;
                        }
                    }
                    snprintf(rendered_secure, sizeof(rendered_secure),
                        i == 0 ? "SecureSlot<%s>" : "Token<%s>", inner);
                    return rendered_secure;
                }
                if (strcmp(callee, "ClaimSlot") == 0 && i == 0) {
                    static char rendered_slot[128];
                    const char *inner = "Int";
                    if (init->data.call.generic_args != NULL
                        && init->data.call.generic_args->count > 0
                        && init->data.call.generic_args->params[0] != NULL
                        && init->data.call.generic_args->params[0]->name != NULL) {
                        inner = init->data.call.generic_args->params[0]->name;
                    }
                    snprintf(rendered_slot, sizeof(rendered_slot),
                        "Slot<%s>", inner);
                    return rendered_slot;
                }
            }
            const char *init_type =
                infer_expression_type_name(ctx, init);
            if ((init_type == NULL || strcmp(init_type, "Unknown") == 0)
                && init != NULL
                && init->type == AST_IDENTIFIER
                && init->data.identifier.name != NULL) {
                const char *resolved =
                    transpiler_find_local_type_name(ctx, func_decl,
                        init->data.identifier.name);
                if (resolved != NULL)
                    init_type = resolved;
            }
            if (init_type != NULL
                && (strncmp(init_type, "Array<", 6) == 0
                    || strncmp(init_type, "Slice<", 6) == 0)) {
                const char *inner = slot_inner_type_name(init_type);
                if (inner != NULL) {
                    static char rendered_arr[128];
                    snprintf(rendered_arr, sizeof(rendered_arr), "%s", inner);
                    return rendered_arr;
                }
            }
            /* Tuple destructuring: element i has the i-th tuple element type */
            if (init_type != NULL && init_type[0] == '(') {
                size_t idx = i;
                size_t pi = 1;
                size_t plen = strlen(init_type);
                size_t cur = 0;
                while (pi < plen && init_type[pi] != ')') {
                    while (pi < plen && (init_type[pi] == ' ' || init_type[pi] == '\t'))
                        pi++;
                    static char rendered_tup[128];
                    size_t eo = 0;
                    int depth = 0;
                    while (pi < plen && eo + 1 < sizeof(rendered_tup)) {
                        char c = init_type[pi];
                        if (depth == 0 && (c == ',' || c == ')'))
                            break;
                        if (c == '<' || c == '(') depth++;
                        if (c == '>' || c == ')') depth--;
                        rendered_tup[eo++] = c;
                        pi++;
                    }
                    rendered_tup[eo] = '\0';
                    while (eo > 0 && (rendered_tup[eo-1] == ' ' || rendered_tup[eo-1] == '\t'))
                        rendered_tup[--eo] = '\0';
                    if (cur == idx)
                        return rendered_tup;
                    cur++;
                    if (pi < plen && init_type[pi] == ',') pi++;
                }
            }
            return NULL;
        }
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL
            && strcmp(body->data.with_stmt.alias, base_name) == 0) {
            static char rendered_slot[256];
            char *inner = render_type_name(body->data.with_stmt.slot_type);
            snprintf(rendered_slot, sizeof(rendered_slot),
                     "%s<%s>",
                     body->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                     inner != NULL ? inner : "Int");
            free(inner);
            return rendered_slot;
        }
        return transpiler_find_local_type_name_in_block(ctx, func_decl,
            body->data.with_stmt.body, base_name);
    }
    if (body->type == AST_IF_STMT) {
        const char *found = transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.while_loop.body, base_name);
    return NULL;
}
