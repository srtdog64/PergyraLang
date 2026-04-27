static ASTNode *
transpiler_find_local_type_ast_in_block(TranspilerCtx *ctx,
                                        ASTNode *body,
                                        const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_local_type_ast_in_block(
                ctx, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL)
            return body->data.let_decl.type;
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_CALL
            && body->data.let_decl.initializer->data.call.callee != NULL
            && body->data.let_decl.initializer->data.call.callee->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.call.callee->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.call.callee->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
                return decl->data.func_decl.return_type;
            }
        }
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
                return decl->data.func_decl.return_type;
            }
        }
        return NULL;
    }
    if (body->type == AST_WITH_STMT)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.with_stmt.body, base_name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_local_type_ast_in_block(
            ctx, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.while_loop.body, base_name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.for_loop.body, base_name);
    return NULL;
}

static ASTNode *
transpiler_find_local_type_ast(TranspilerCtx *ctx,
                               const ASTNode *func_decl,
                               const char *base_name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL
        || base_name == NULL) {
        return NULL;
    }
    return transpiler_find_local_type_ast_in_block(
        ctx, func_decl->data.func_decl.body, base_name);
}

static char *
transpiler_render_effective_local_type_name(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (type_node != NULL
        && type_node->type == AST_TYPE
        && type_node->data.type.name != NULL) {
        ASTNode *class_decl = find_class_decl(ctx, type_node->data.type.name);
        if (class_decl != NULL && class_has_generic_params(class_decl)) {
            const char *spec_name =
                ensure_generic_class_specialization(ctx, class_decl, type_node);
            if (spec_name != NULL && strcmp(spec_name, type_node->data.type.name) != 0)
                return pergyra_strdup(spec_name);
        }
    }
    return render_type_name(type_node);
}

static bool
transpiler_has_local_binding_in_block(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return false;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            if (transpiler_has_local_binding_in_block(
                    body->data.block.statements[i], base_name)) {
                return true;
            }
        }
        return false;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        return true;
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL
            && strcmp(body->data.with_stmt.alias, base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(body->data.with_stmt.body, base_name);
    }
    if (body->type == AST_IF_STMT) {
        return transpiler_has_local_binding_in_block(body->data.if_stmt.then_branch, base_name)
            || transpiler_has_local_binding_in_block(body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_has_local_binding_in_block(body->data.while_loop.body, base_name);
    if (body->type == AST_FOR_LOOP) {
        if (body->data.for_loop.variable != NULL
            && strcmp(body->data.for_loop.variable, base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(body->data.for_loop.body, base_name);
    }
    return false;
}

static void
transpiler_register_with_alias_bindings_in_block(TranspilerSSANameMap *ssa_map,
                                                 ASTNode *body)
{
    if (ssa_map == NULL || body == NULL)
        return;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++)
            transpiler_register_with_alias_bindings_in_block(ssa_map,
                body->data.block.statements[i]);
        return;
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL)
            transpiler_ssa_name_map_set(ssa_map,
                body->data.with_stmt.alias, body->data.with_stmt.alias);
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.with_stmt.body);
        return;
    }
    if (body->type == AST_LET_DESTRUCTURE) {
        /* Destructuring bindings need precheck registration because MIR
         * DEF emission for the individual bindings is not guaranteed across
         * block boundaries (a `let (a, b) = ...; if a { ... }` splits the
         * DEF producer and the consumer across blocks, and the per-block
         * ssa_entry_values only propagate if the block's def list matches).
         *
         * Claim-shape cases (ClaimSlot/ClaimSecureSlot): the bindings are
         * slot-anchored so the emit path self-maps them.
         * All other cases (Array, Slice, tuple, user function): register
         * them as self-mapping up-front so the precheck's identifier walk
         * succeeds. The actual emit-time ssa_map picks up the versioned
         * `<name>.1` mapping from the destructure's own emission. */
        for (size_t i = 0; i < body->data.let_destructure.name_count; i++) {
            const char *pname = body->data.let_destructure.names[i];
            if (pname != NULL)
                transpiler_ssa_name_map_set(ssa_map, pname, pname);
        }
        return;
    }
    if (body->type == AST_IF_STMT) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.if_stmt.then_branch);
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.if_stmt.else_branch);
        return;
    }
    if (body->type == AST_WHILE_LOOP) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.while_loop.body);
        return;
    }
    if (body->type == AST_FOR_LOOP) {
        if (body->data.for_loop.variable != NULL) {
            transpiler_ssa_name_map_set(ssa_map,
                body->data.for_loop.variable,
                body->data.for_loop.variable);
        }
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.for_loop.body);
    }
}

static void
transpiler_register_explicit_local_bindings_in_block(TranspilerCtx *ctx,
                                                     const ASTNode *func_decl,
                                                     ASTNode *body)
{
    if (ctx == NULL || body == NULL || body->type != AST_BLOCK)
        return;
    for (size_t i = 0; i < body->data.block.count; i++) {
        ASTNode *stmt = body->data.block.statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_BLOCK) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl, stmt);
            continue;
        }
        if (stmt->type == AST_LET_DECL && stmt->data.let_decl.name != NULL) {
            const char *type_name = NULL;
            char *rendered_type = NULL;
            if (stmt->data.let_decl.type != NULL) {
                rendered_type = transpiler_render_effective_local_type_name(
                    ctx, stmt->data.let_decl.type);
                type_name = rendered_type;
            } else if (stmt->data.let_decl.initializer != NULL) {
                type_name = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, stmt->data.let_decl.initializer);
            }
            if (type_name != NULL && type_name[0] != '\0') {
                bool registered_view_like = false;
                if ((strcmp(type_name, "ReadView") == 0
                     || strncmp(type_name, "ReadView<", 9) == 0
                     || strcmp(type_name, "WriteView") == 0
                     || strncmp(type_name, "WriteView<", 10) == 0)
                    && stmt->data.let_decl.initializer != NULL
                    && stmt->data.let_decl.initializer->type == AST_CALL
                    && stmt->data.let_decl.initializer->data.call.callee != NULL
                    && stmt->data.let_decl.initializer->data.call.callee->type == AST_IDENTIFIER
                    && stmt->data.let_decl.initializer->data.call.arg_count >= 1
                    && stmt->data.let_decl.initializer->data.call.arguments[0] != NULL
                    && stmt->data.let_decl.initializer->data.call.arguments[0]->type == AST_IDENTIFIER) {
                    const char *callee =
                        stmt->data.let_decl.initializer->data.call.callee->data.identifier.name;
                    const char *source =
                        stmt->data.let_decl.initializer->data.call.arguments[0]->data.identifier.name;
                    if (callee != NULL
                        && source != NULL
                        && (strcmp(callee, "ViewRead") == 0
                            || strcmp(callee, "ViewWrite") == 0)) {
                        const char *source_type = lookup_typed_var(ctx, source);
                        bool source_secure = lookup_slot_is_secure(ctx, source);
                        if (!source_secure
                            && source_type != NULL
                            && (strcmp(source_type, "SecureSlot") == 0
                                || strncmp(source_type, "SecureSlot<", 11) == 0)) {
                            source_secure = true;
                        }
                        register_view_like_var(ctx, stmt->data.let_decl.name,
                            type_name, source, source_secure, false);
                        registered_view_like = true;
                    }
                }
                if (!registered_view_like)
                    register_typed_var(ctx, stmt->data.let_decl.name, type_name);
                if (transpiler_type_name_is_slot_like(type_name)) {
                    register_slot_var(ctx, stmt->data.let_decl.name,
                        slot_inner_type_name(type_name),
                        strncmp(type_name, "SecureSlot<", 11) == 0,
                        false);
                }
            }
            free(rendered_type);
            continue;
        }
        if (stmt->type == AST_WITH_STMT && stmt->data.with_stmt.alias != NULL) {
            char *inner = render_type_name(stmt->data.with_stmt.slot_type);
            char *slot_type = strdup_fmt("%s<%s>",
                stmt->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                inner != NULL ? inner : "Int");
            register_typed_var(ctx, stmt->data.with_stmt.alias,
                slot_type != NULL ? slot_type : "Slot<Int>");
            register_slot_var(ctx, stmt->data.with_stmt.alias,
                inner != NULL ? inner : "Int",
                stmt->data.with_stmt.is_secure, false);
            free(slot_type);
            free(inner);
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.with_stmt.body);
            continue;
        }
        if (stmt->type == AST_IF_STMT) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.if_stmt.then_branch);
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.if_stmt.else_branch);
            continue;
        }
        if (stmt->type == AST_WHILE_LOOP) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.while_loop.body);
            continue;
        }
        if (stmt->type == AST_FOR_LOOP) {
            if (stmt->data.for_loop.variable != NULL) {
                register_typed_var(ctx, stmt->data.for_loop.variable, "Int");
            }
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.for_loop.body);
        }
    }
}

static bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        return false;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0)
            return true;
    }
    return transpiler_has_local_binding_in_block(func_decl->data.func_decl.body,
        base_name);
}

static const char *
transpiler_lookup_current_owner_member_type_name(TranspilerCtx *ctx,
                                                 const char *member_name)
{
    const char *member_type = NULL;
    const char *host_name = NULL;
    ASTNode *host_decl = NULL;

    if (ctx == NULL || member_name == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL) {
        switch (host_decl->type) {
        case AST_CLASS_DECL:
            host_name = host_decl->data.class_decl.name;
            break;
        case AST_RELATION_DECL:
            host_name = host_decl->data.relation_decl.name;
            break;
        case AST_EFFECT_DECL:
            host_name = host_decl->data.effect_decl.name;
            break;
        case AST_ZONE_DECL:
            host_name = host_decl->data.zone_decl.name;
            break;
        case AST_WORLD_DECL:
            host_name = host_decl->data.world_decl.name;
            break;
        default:
            break;
        }
        if (host_name != NULL) {
            member_type = transpiler_lookup_nominal_host_member_type_name(ctx,
                host_name, member_name);
            if (member_type != NULL)
                return member_type;
        }
    }

    return NULL;
}

static const char *
transpiler_find_local_type_name(TranspilerCtx *ctx,
                                const ASTNode *func_decl,
                                const char *base_name)
{
    const char *typed_name = NULL;

    if (base_name == NULL)
        return NULL;
    if (ctx != NULL) {
        typed_name = lookup_typed_var(ctx, base_name);
        if (typed_name != NULL && strcmp(typed_name, "Unknown") != 0)
            return typed_name;
    }
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return transpiler_lookup_current_owner_member_type_name(ctx, base_name);
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0 && p->type != NULL) {
            static char *rendered_param = NULL;
            free(rendered_param);
            rendered_param = transpiler_render_effective_local_type_name(ctx, p->type);
            if (ctx != NULL && rendered_param != NULL)
                register_typed_var(ctx, base_name, rendered_param);
            return rendered_param;
        }
    }
    typed_name = transpiler_find_local_type_name_in_block(ctx, func_decl,
        func_decl->data.func_decl.body, base_name);
    if (typed_name != NULL) {
        if (ctx != NULL)
            register_typed_var(ctx, base_name, typed_name);
        return typed_name;
    }

    return transpiler_lookup_current_owner_member_type_name(ctx, base_name);
}

static bool
transpiler_mir_type_supported(const char *type_name)
{
    if (type_name == NULL)
        return false;
    if (strcmp(type_name, "Void") == 0)
        return true;
    return strcmp(type_name, "Int") == 0
           || strcmp(type_name, "Long") == 0
           || strcmp(type_name, "Float") == 0
           || strcmp(type_name, "Bool") == 0
           || strcmp(type_name, "String") == 0
           || strncmp(type_name, "Slot<", 5) == 0
           || strncmp(type_name, "SecureSlot<", 11) == 0
           || strncmp(type_name, "DeviceSlot<", 11) == 0;
}

static bool
transpiler_mir_ast_type_supported(const ASTNode *type_node)
{
    char *type_name = NULL;
    const char *c_type = NULL;

    if (type_node == NULL)
        return true;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        if (!transpiler_mir_ast_type_supported(
                type_node->data.event_handler_type.return_type)) {
            return false;
        }
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            if (!transpiler_mir_ast_type_supported(
                    type_node->data.event_handler_type.param_types[i])) {
                return false;
            }
        }
        return true;
    }

    type_name = render_type_name((ASTNode *)type_node);
    if (type_name == NULL)
        return false;
    if (transpiler_mir_type_supported(type_name)) {
        free(type_name);
        return true;
    }

    c_type = pergyra_ast_type_to_c((ASTNode *)type_node);
    if (c_type == NULL || c_type[0] == '\0') {
        free(type_name);
        return false;
    }
    if (strcmp(type_name, "Unknown") == 0 || strcmp(c_type, "Unknown") == 0) {
        free(type_name);
        return false;
    }

    free(type_name);
    return true;
}

static bool
transpiler_mir_function_signature_supported(const ASTNode *func_decl)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    if (!transpiler_mir_ast_type_supported(func_decl->data.func_decl.return_type))
        return false;

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL)
            continue;
        if (!transpiler_mir_ast_type_supported(param->type))
            return false;
    }

    return true;
}

static char *
emit_expression_with_ssa_map(ASTNode *node, TranspilerCtx *ctx,
                             const TranspilerSSANameMap *ssa_map)
{
    const void *saved_active_ssa_map;
    char *result;

    if (node == NULL)
        return pergyra_strdup("0");
    if (ctx == NULL)
        return emit_expression(node, ctx);

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map;
    result = emit_expression(node, ctx);
    ctx->active_ssa_map = saved_active_ssa_map;
    return result;
}

static bool
transpiler_emit_mir_phi_copies(CodeBuf *buf, TranspilerCtx *ctx, int indent,
                               size_t pred_block_index,
                               const MIRBasicBlock *pred_block,
                               const MIRBasicBlock *target_block)
{
    const char *pred_bases[TRANSPILE_SSA_MAP_CAPACITY];
    const char *pred_versions[TRANSPILE_SSA_MAP_CAPACITY];
    const char *target_bases[TRANSPILE_SSA_MAP_CAPACITY];
    const char *target_versions[TRANSPILE_SSA_MAP_CAPACITY];
    size_t pred_count = 0;
    size_t target_count = 0;

    if (buf == NULL || pred_block == NULL || target_block == NULL)
        return false;

    for (size_t i = 0; i < target_block->instruction_count; i++) {
        const MIRInstruction *inst = &target_block->instructions[i];
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        for (size_t j = 0; j < inst->phi_incoming_count; j++) {
            const MIRPhiIncoming *incoming = &inst->phi_incomings[j];
            if (incoming->predecessor_block != pred_block_index
                || incoming->value_name == NULL
                || strcmp(inst->result_name, incoming->value_name) == 0) {
                continue;
            }
            {
                char *lhs = transpiler_render_ssa_name(ctx, inst->result_name);
                char *rhs = transpiler_render_ssa_name(ctx, incoming->value_name);
                write_indent_to(buf, indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
            }
            break;
        }
    }

    if (!transpiler_collect_ssa_name_entries(
            pred_block->ssa_exit_value_count > 0
                ? pred_block->ssa_exit_values
                : pred_block->ssa_entry_values,
            pred_block->ssa_exit_value_count > 0
                ? pred_block->ssa_exit_value_count
                : pred_block->ssa_entry_value_count,
            pred_bases,
            pred_versions,
            TRANSPILE_SSA_MAP_CAPACITY,
            &pred_count)) {
        return false;
    }
    if (!transpiler_collect_ssa_name_entries(target_block->ssa_entry_values,
                                             target_block->ssa_entry_value_count,
                                             target_bases,
                                             target_versions,
                                             TRANSPILE_SSA_MAP_CAPACITY,
                                             &target_count)) {
        transpiler_free_ssa_name_entries(pred_bases, pred_count);
        return false;
    }

    for (size_t i = 0; i < target_count; i++) {
        const char *lhs_version = target_versions[i];
        const char *rhs_version = NULL;

        if (lhs_version == NULL || target_bases[i] == NULL)
            continue;
        for (size_t j = 0; j < pred_count; j++) {
            if (pred_bases[j] != NULL
                && strcmp(pred_bases[j], target_bases[i]) == 0) {
                rhs_version = pred_versions[j];
                break;
            }
        }
        if (rhs_version == NULL || strcmp(lhs_version, rhs_version) == 0)
            continue;
        {
            char *lhs = transpiler_render_ssa_name(ctx, lhs_version);
            char *rhs = transpiler_render_ssa_name(ctx, rhs_version);
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }
    }

    transpiler_free_ssa_name_entries(pred_bases, pred_count);
    transpiler_free_ssa_name_entries(target_bases, target_count);
    return true;
}

static const char *
transpiler_find_prior_block_ssa_name(const MIRBasicBlock *block,
                                     size_t limit_inst_index,
                                     const char *base_name)
{
    const char *resolved = NULL;

    if (block == NULL || base_name == NULL)
        return NULL;

    for (size_t i = 0; i < block->instruction_count && i < limit_inst_index; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char parsed_base[128];
        size_t version = 0;

        if ((inst->kind != MIR_INST_DEF && inst->kind != MIR_INST_PHI)
            || inst->result_name == NULL) {
            continue;
        }
        if (!transpiler_parse_versioned_name(inst->result_name, parsed_base,
                                             sizeof(parsed_base), &version)) {
            continue;
        }
        if (strcmp(parsed_base, base_name) == 0)
            resolved = inst->result_name;
    }

    return resolved;
}

static const char *
transpiler_find_block_exit_ssa_name(const MIRBasicBlock *block,
                                    const char *base_name)
{
    if (block == NULL || base_name == NULL || block->ssa_exit_values == NULL)
        return NULL;

    for (size_t i = 0; i < block->ssa_exit_value_count; i++) {
        const char *versioned = block->ssa_exit_values[i];
        char parsed_base[128];
        size_t version = 0;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned,
                                             parsed_base,
                                             sizeof(parsed_base),
                                             &version)) {
            continue;
        }
        if (strcmp(parsed_base, base_name) == 0)
            return versioned;
    }

    return NULL;
}

static const char *
transpiler_find_routine_exit_ssa_name(const MIRRoutine *mir_routine,
                                      const char *base_name)
{
    if (mir_routine == NULL || base_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < mir_routine->block_count; bi++) {
        const char *versioned =
            transpiler_find_block_exit_ssa_name(&mir_routine->blocks[bi], base_name);
        if (versioned != NULL)
            return versioned;
    }

    return NULL;
}
