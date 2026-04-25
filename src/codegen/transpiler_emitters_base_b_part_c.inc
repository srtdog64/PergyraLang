/* -----------------------------------------------------------------
 * Statement dispatcher
 * ----------------------------------------------------------------- */

static void
emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias_name;
    const char *target_c_type;

    if (node == NULL || node->type != AST_TYPE_ALIAS
        || node->data.type_alias.name == NULL
        || node->data.type_alias.target_type == NULL) {
        return;
    }

    alias_name = node->data.type_alias.name;
    ensure_type_specializations_from_ast(ctx, node->data.type_alias.target_type);
    target_c_type = pergyra_ast_type_to_c(node->data.type_alias.target_type);
    codebuf_write(ctx->out, "typedef %s %s;\n", target_c_type, alias_name);
}

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_TYPE_ALIAS:
        emit_type_alias_decl(node, ctx);
        break;
    case AST_LET_DESTRUCTURE:
    {
        /* let (a, b, c) = expr;
         * → auto _tmp = expr;
         *   Type a = _tmp.data[0]; // Array
         *   Type b = _tmp.data[1]; */
        ASTNode *init = node->data.let_destructure.initializer;
        char *init_expr = emit_expression(init, ctx);
        const char *init_type = infer_expression_type_name(ctx, init);
        if ((init_type == NULL || strcmp(init_type, "Unknown") == 0)
            && init != NULL
            && init->type == AST_IDENTIFIER
            && init->data.identifier.name != NULL) {
            const char *resolved =
                transpiler_current_local_type_name(ctx, init->data.identifier.name);
            if (resolved != NULL)
                init_type = resolved;
        }
        const char *c_init_type = pergyra_type_to_c(init_type);

        /* Tuple destructuring: let (a, b, ...) = tuple_expr */
        if (init_type != NULL && init_type[0] == '(') {
            /* Element type names: "(Int, String, Bool)" → {"Int","String","Bool"} */
            char elem_names[8][64];
            size_t arity = 0;
            {
                size_t i = 1;
                size_t n = strlen(init_type);
                while (i < n && init_type[i] != ')' && arity < 8) {
                    while (i < n && (init_type[i] == ' ' || init_type[i] == '\t'))
                        i++;
                    size_t eo = 0;
                    int depth = 0;
                    while (i < n && eo + 1 < sizeof(elem_names[0])) {
                        char c = init_type[i];
                        if (depth == 0 && (c == ',' || c == ')'))
                            break;
                        if (c == '<' || c == '(') depth++;
                        if (c == '>' || c == ')') depth--;
                        elem_names[arity][eo++] = c;
                        i++;
                    }
                    elem_names[arity][eo] = '\0';
                    while (eo > 0 && (elem_names[arity][eo-1] == ' '
                                   || elem_names[arity][eo-1] == '\t'))
                        elem_names[arity][--eo] = '\0';
                    arity++;
                    if (i < n && init_type[i] == ',') i++;
                }
            }
            if (arity != node->data.let_destructure.name_count) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                        (unsigned long long) node->data.let_destructure.name_count,
                        (unsigned long long) arity);
                }
                free(init_expr);
                return;
            }
            int tmp_id = ++ctx->tmp_counter;
            write_indent(ctx);
            codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
                c_init_type, tmp_id, init_expr);
            for (size_t i = 0; i < arity; i++) {
                const char *e_ctype = pergyra_type_to_c(elem_names[i]);
                write_indent(ctx);
                codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.f%zu;\n",
                    e_ctype != NULL ? e_ctype : elem_names[i],
                    node->data.let_destructure.names[i], tmp_id, i);
                register_typed_var(ctx, node->data.let_destructure.names[i],
                    elem_names[i]);
            }
            free(init_expr);
            break;
        }

        const char *elem_c_type = NULL;
        const char *inner = NULL;
        if (init_type != NULL
            && (strncmp(init_type, "Array<", 6) == 0
                || strncmp(init_type, "Slice<", 6) == 0)) {
            inner = slot_inner_type_name(init_type);
            elem_c_type = pergyra_type_to_c(inner);
        }
        if (c_init_type == NULL || elem_c_type == NULL || inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                    init_type != NULL ? init_type : "(unknown)");
            }
            return;
        }
        int tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.data[%zu];\n",
                elem_c_type, node->data.let_destructure.names[i], tmp_id, i);
            register_typed_var(ctx, node->data.let_destructure.names[i], inner);
        }
        free(init_expr);
        break;
    }
    case AST_FUNC_DECL:
        emit_func_decl(node, ctx);
        break;
    case AST_CLASS_DECL:
        emit_class_decl(node, ctx);
        break;
    case AST_EXTERN_BLOCK:
        emit_extern_block(node, ctx);
        break;
    case AST_IMPORT_DECL:
        /* Import is resolved at driver level (AST merging).
         * Nothing to emit — the imported declarations are
         * already present in the merged AST. */
        break;
    case AST_USE_DECL:
        /* use module; — standard library modules.
         * Runtime functions are always available via pgy_runtime.h.
         * Future: emit module-specific includes/initializers here. */
        codebuf_write(ctx->out, "/* use %s */\n",
            node->data.use_decl.module_name != NULL
                ? node->data.use_decl.module_name : "unknown");
        break;
    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } → emit body directly (no safety wrappers) */
        write_indent(ctx);
        codebuf_write(ctx->out, "/* unsafe */\n");
        if (node->data.unsafe_block.body != NULL)
            emit_block(node->data.unsafe_block.body, ctx);
        break;
    case AST_DEFER_STMT: {
        /* defer { body } → GCC __attribute__((cleanup)) pattern.
         * 1. Emit a static cleanup function into the helpers buffer.
         * 2. Declare a sentinel variable with cleanup attribute.
         * When the sentinel goes out of scope, GCC calls the cleanup. */
        int defer_id = ctx->defer_counter++;

        /* Generate cleanup function in helpers buffer */
        codebuf_write(ctx->helpers,
            "\nstatic void _pgy_defer_%d(int *_pgy_unused) {\n"
            "    (void)_pgy_unused;\n", defer_id);

        /* Save current output, redirect to helpers for the body */
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        if (node->data.defer_stmt.body != NULL)
            emit_block(node->data.defer_stmt.body, ctx);
        ctx->out = saved_out;
        ctx->indent = saved_indent;

        codebuf_write(ctx->helpers, "}\n");

        /* Emit sentinel with cleanup attribute */
        write_indent(ctx);
        codebuf_write(ctx->out,
            "int _pgy_defer_sentinel_%d "
            "__attribute__((cleanup(_pgy_defer_%d))) = 0;\n",
            defer_id, defer_id);
        break;
    }
    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * → lookup party's typed_var to get PartyType,
         *   then emit PartyType_bind_slot(&party, NULL, &Role_Ability_vtable_instance)
         * For now: use the typed_var mapping to find the party type. */
        const char *pvar = node->data.bind_stmt.party_var;
        const char *slot = node->data.bind_stmt.slot_name;
        const char *role = node->data.bind_stmt.role_name;
        const char *party_type = NULL;
        for (int ti = 0; ti < ctx->typed_var_count; ti++) {
            if (strcmp(ctx->typed_vars[ti].name, pvar) == 0) {
                party_type = ctx->typed_vars[ti].type_name;
                break;
            }
        }
        if (party_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve party type for bind statement '%s.%s = %s'",
                pvar != NULL ? pvar : "<party>",
                slot != NULL ? slot : "<slot>",
                role != NULL ? role : "<role>");
            break;
        }

        /* Find the ability name by scanning the current program view for the
         * party declaration. In MIR-backed emission this view is synthesized
         * from MIRProgram inventory, not from the original HIR. The dyn role
         * slot records the required ability. */
        const char *ability_name = NULL;
        char *ability_tag = NULL;
        {
            ASTNode *it = find_party_decl(ctx, party_type);
            if (it == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve party declaration '%s' while emitting bind statement '%s.%s = %s'",
                    party_type,
                    pvar != NULL ? pvar : "<party>",
                    slot != NULL ? slot : "<slot>",
                    role != NULL ? role : "<role>");
                break;
            }
            for (size_t ri = 0; ri < it->data.party_decl.role_count; ri++) {
                ASTNode *rs = it->data.party_decl.role_slots[ri];
                if (strcmp(rs->data.role_slot.slot_name, slot) == 0
                    && rs->data.role_slot.ability_count > 0
                    && rs->data.role_slot.required_abilities[0] != NULL) {
                    ability_tag = render_ability_ref_vtable_tag(
                        rs->data.role_slot.required_abilities[0]);
                    ability_name = ability_tag;
                    break;
                }
            }
        }
        if (ability_name == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve required ability tag for party slot '%s.%s' while emitting bind statement",
                party_type,
                slot != NULL ? slot : "<slot>");
            free(ability_tag);
            break;
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
            party_type, slot, pvar,
            role, ability_name);
        free(ability_tag);
        break;
    }
    case AST_ABILITY_DECL:
        emit_ability_decl(node, ctx);
        break;
    case AST_ROLE_DECL:
        emit_role_decl(node, ctx);
        break;
    case AST_PARTY_DECL:
        emit_party_decl(node, ctx);
        break;
    case AST_ROSTER_DECL:
        emit_roster_decl(node, ctx);
        break;
    case AST_WORLD_DECL:
        emit_world_decl(node, ctx);
        break;
    case AST_RELATION_DECL:
        emit_relation_decl(node, ctx);
        break;
    case AST_EFFECT_DECL:
        emit_effect_decl(node, ctx);
        break;
    case AST_ZONE_DECL:
        emit_zone_decl(node, ctx);
        break;
    case AST_EVENT_DECL:
        emit_event_decl(node, ctx);
        break;
    case AST_EVENT_SUBSCRIBE:
        emit_event_subscribe(node, ctx);
        break;
    case AST_EVENT_UNSUBSCRIBE:
        emit_event_unsubscribe(node, ctx);
        break;
    case AST_IF_STMT:
        emit_if_stmt(node, ctx);
        break;
    case AST_FOR_LOOP:
        emit_for_loop(node, ctx);
        break;
    case AST_WHILE_LOOP:
        emit_while_loop(node, ctx);
        break;
    case AST_MATCH_STMT:
        emit_match_stmt(node, ctx);
        break;
    case AST_RETURN:
        emit_return_stmt(node, ctx);
        break;
    case AST_BREAK:
        write_indent(ctx);
        if (node->data.break_stmt.label != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, node->data.break_stmt.label);
            if (target >= 0) {
                ctx->loop_break_label_used[target] = true;
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_break_labels[target]);
            } else
                codebuf_write(ctx->out, "break;\n");
        } else {
            codebuf_write(ctx->out, "break;\n");
        }
        break;
    case AST_ENUM_DECL:
        emit_enum_decl_stmt(node, ctx);
        break;
    case AST_CONTINUE:
        write_indent(ctx);
        if (node->data.continue_stmt.label != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, node->data.continue_stmt.label);
            if (target >= 0) {
                ctx->loop_continue_label_used[target] = true;
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_continue_labels[target]);
            } else
                codebuf_write(ctx->out, "continue;\n");
        } else {
            codebuf_write(ctx->out, "continue;\n");
        }
        break;
    case AST_WITH_STMT:
        emit_with_stmt(node, ctx);
        break;
    case AST_PARALLEL_BLOCK:
        emit_parallel_block(node, ctx);
        break;
    case AST_BLOCK:
        emit_block(node, ctx);
        break;
    case AST_LAMBDA_EXPR:
        {
            char *expr = emit_expression(node, ctx);
            if (expr != NULL && expr[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expr);
            }
            free(expr);
            break;
        }
    case AST_SELECT_STMT:
        emit_select_stmt(node, ctx);
        break;
    case AST_ASYNC_BLOCK:
        emit_async_block(node, ctx);
        break;
    default: {
        /* Expression statement (including event invoke) */
        char *expr = emit_expression(node, ctx);
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
            if (node->type == AST_CALL)
                emit_zone_action_effect_runtime(node, ctx);
        }
        free(expr);
        break;
    }
    }
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        int saved_alias_count = ctx->alias_var_count;
        for (size_t i = 0; i < node->data.block.count; i++)
            emit_statement(node->data.block.statements[i], ctx);

        /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
         * Skip slots already explicitly released by the user. */
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            SlotVarEntry *e = &ctx->slot_vars[i];
            if (e->released) continue;
            write_indent(ctx);
            if (e->is_secure) {
                codebuf_write(ctx->out,
                    "pgy_secure_release_%s(&%s, &%s_token);\n",
                    e->inner_type, e->name, e->name);
            } else {
                codebuf_write(ctx->out,
                    "pgy_release_%s(&%s);\n",
                    e->inner_type, e->name);
            }
        }

        transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                      saved_typed_count,
                                                      saved_alias_count);
    } else {
        emit_statement(node, ctx);
    }
}

static ASTNode *
find_intent_participant_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
find_subject_action_decl(TranspilerCtx *ctx, const char *subject_name, const char *action_name)
{
    ASTNode *decl;
    if (ctx == NULL || subject_name == NULL || action_name == NULL)
        return NULL;
    decl = find_subject_host_decl(ctx, subject_name);
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return method;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_decl_in_program_view(TranspilerCtx *ctx, const char *zone_name)
{
    return transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL, zone_name);
}

static const char *
intent_participant_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias);

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias);

static const char *
intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

static const char *
intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
intent_zone_binding_type_name_with_metadata(ASTNode *intent,
                                            const char *alias,
                                            const char **participant_aliases,
                                            const char **participant_types,
                                            size_t participant_count)
{
    if (alias != NULL && participant_aliases != NULL && participant_types != NULL) {
        for (size_t i = 0; i < participant_count; i++) {
            if (participant_aliases[i] != NULL
                && participant_types[i] != NULL
                && strcmp(participant_aliases[i], alias) == 0) {
                return participant_types[i];
            }
        }
    }
    return intent_zone_binding_type_name(intent, alias);
}

static const char *
intent_involves_type_name_local(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
intent_involves_is_subject_participant(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_subject_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static bool
intent_involves_uses_pointer_self(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_pointer_self_host_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static void
emit_intent_step_bind_bound_zone(CodeBuf *out, TranspilerCtx *ctx,
                                 ASTNode *intent, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;
    const char *from_alias;
    const char *from_zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    from_alias = step->data.intent_step.transfer_from_alias;
    from_zone_type = intent_zone_binding_type_name(intent, from_alias);

    if (from_alias != NULL && from_zone_type != NULL) {

        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names[i];
            const char *from_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                from_zone_type, alias);
            const char *to_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                zone_type, alias);
            if (alias == NULL)
                continue;
            if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", from_alias, from_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                    alias, from_slot_name, from_zone_type);
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_transfer_export(__intent_handle, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");\n",
                    alias,
                    from_zone_type != NULL ? from_zone_type : "<zone>",
                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                    zone_type,
                    to_slot_name);
            }
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(%s);\n", from_zone_type, from_alias);
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type, zone_type) != 0) {
            write_indent(ctx);
            codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
        }
        return;
    }

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name(ctx, intent, step, alias);
        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s->%s = *%s;\n", zone_alias, slot_name, alias);
        write_indent(ctx);
        codebuf_write(out,
            "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
            alias, slot_name, zone_type);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_bind_bound_zone_with_metadata(CodeBuf *out, TranspilerCtx *ctx,
                                               ASTNode *intent,
                                               const char *zone_type,
                                               const char *zone_alias,
                                               const char *from_alias,
                                               const char **who_aliases,
                                               size_t who_alias_count,
                                               const char **participant_aliases,
                                               const char **participant_types,
                                               size_t participant_count)
{
    const char *from_zone_type;

    if (out == NULL || ctx == NULL || intent == NULL
        || zone_alias == NULL || zone_type == NULL) {
        return;
    }

    from_zone_type = intent_zone_binding_type_name_with_metadata(
        intent, from_alias, participant_aliases, participant_types, participant_count);

    if (from_alias != NULL && from_zone_type != NULL) {
        for (size_t i = 0; i < who_alias_count; i++) {
            const char *alias = who_aliases[i];
            const char *from_slot_name = resolve_intent_zone_slot_name_for_zone(
                ctx, intent, from_zone_type, alias);
            const char *to_slot_name = resolve_intent_zone_slot_name_for_zone(
                ctx, intent, zone_type, alias);
            if (alias == NULL)
                continue;
            if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", from_alias, from_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                    alias, from_slot_name, from_zone_type);
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_transfer_export(__intent_handle, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");\n",
                    alias,
                    from_zone_type != NULL ? from_zone_type : "<zone>",
                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                    zone_type,
                    to_slot_name);
            }
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(%s);\n", from_zone_type, from_alias);
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type, zone_type) != 0) {
            write_indent(ctx);
            codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
        }
        return;
    }

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s->%s = *%s;\n", zone_alias, slot_name, alias);
        write_indent(ctx);
        codebuf_write(out,
            "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
            alias, slot_name, zone_type);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static bool
emit_intent_step_rebind_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                           ASTNode *intent, ASTNode *step,
                                           size_t step_index)
{
    const char *zone_alias;
    const char *zone_type;
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return false;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return false;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        participant_c_type = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

static bool
emit_intent_step_rebind_bound_zone_aliases_with_metadata(CodeBuf *out,
                                                         TranspilerCtx *ctx,
                                                         ASTNode *intent,
                                                         const char *zone_type,
                                                         const char *zone_alias,
                                                         const char **who_aliases,
                                                         size_t who_alias_count,
                                                         size_t step_index,
                                                         const char **participant_aliases,
                                                         const char **participant_types,
                                                         size_t participant_count)
{
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL
        || zone_alias == NULL || zone_type == NULL) {
        return false;
    }

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        const char *participant_type = intent_zone_binding_type_name_with_metadata(
            intent, alias, participant_aliases, participant_types, participant_count);
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || participant_type == NULL) {
            continue;
        }

        participant_c_type = pergyra_type_to_c(participant_type);
        if (participant_c_type == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

static void
emit_intent_step_mark_caused_effect(CodeBuf *out, TranspilerCtx *ctx,
                                    const char *zone_type,
                                    const char *zone_alias,
                                    const char *causes_effect)
{
    ASTNode *zone_decl;

    if (out == NULL || ctx == NULL || zone_type == NULL || zone_alias == NULL
        || causes_effect == NULL) {
        return;
    }

    zone_decl = find_zone_decl(ctx, zone_type);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        const char *layer_name;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.slot_name == NULL
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, causes_effect) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        write_indent(ctx);
        codebuf_write(out, "%s->__layer_epoch_%s++;\n", zone_alias, layer_name);
        write_indent(ctx);
        codebuf_write(out, "%s->__layer_cause_%s = 11;\n", zone_alias, layer_name);

        for (size_t j = 0; j < zone_decl->data.zone_decl.state_count; j++) {
            ASTNode *state = zone_decl->data.zone_decl.states[j];
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.state_name == NULL
                || state->data.zone_state.layer_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(out, "%s->__state_epoch_%s++;\n",
                zone_alias, state->data.zone_state.state_name);
            write_indent(ctx);
            codebuf_write(out, "%s->__state_cause_%s = 11;\n",
                zone_alias, state->data.zone_state.state_name);
        }
    }
}

static void
emit_intent_step_validate_authority(CodeBuf *out,
                                    TranspilerCtx *ctx,
                                    const char *intent_name,
                                    const char *step_name,
                                    const char *zone_type,
                                    const char *zone_alias,
                                    const char **authorized_aliases,
                                    size_t authorized_alias_count,
                                    bool emit_cleanup_from_mir,
                                    size_t cleanup_block)
{
    if (out == NULL || ctx == NULL || zone_type == NULL
        || authorized_aliases == NULL || authorized_alias_count == 0) {
        return;
    }

    for (size_t i = 0; i < authorized_alias_count; i++) {
        const char *alias = authorized_aliases[i];
        TypedVarEntry *entry;
        const char *participant_present = "true";
        const char *zone_present = "true";

        if (alias == NULL)
            continue;

        entry = lookup_typed_entry(ctx, alias);
        if (entry != NULL && entry->is_subject_ref)
            participant_present = transpiler_scratch_fmt(ctx, "(%s != NULL)", alias);
        if (zone_alias != NULL)
            zone_present = transpiler_scratch_fmt(ctx, "(%s != NULL)", zone_alias);

        write_indent(ctx);
        codebuf_write(out,
            "if (!pgy_zone_authority_validate_flags_export(%s, %s, \"%s\", \"%s\")) { ",
            zone_present,
            participant_present,
            zone_type,
            alias);
        codebuf_write(out, "__intent_failed = true; ");
        codebuf_write(out,
            "pgy_intent_trace_fail_export(__intent_handle, \"authority:%s\"); __intent_result = false; ",
            step_name != NULL ? step_name : "<step>");
        if (emit_cleanup_from_mir) {
            codebuf_write(out, "goto _pgy_mir_bb_%s_%zu; }\n",
                intent_name != NULL ? intent_name : "intent", cleanup_block);
        } else {
            codebuf_write(out, "goto __intent_cleanup; }\n");
        }
    }
}

static const char *
infer_intent_step_causes_from_on_exprs(TranspilerCtx *ctx,
                                       ASTNode *intent,
                                       ASTNode **on_exprs,
                                       size_t on_expr_count,
                                       const char **participant_aliases,
                                       const char **participant_types,
                                       size_t participant_count)
{
    if (ctx == NULL || intent == NULL || on_exprs == NULL)
        return NULL;

    for (size_t i = 0; i < on_expr_count; i++) {
        ASTNode *expr = on_exprs[i];
        ASTNode *callee;
        ASTNode *receiver;
        const char *alias;
        const char *method_name;
        const char *subject_name;
        ASTNode *action_decl;

        if (expr == NULL || expr->type != AST_CALL)
            continue;
        callee = expr->data.call.callee;
        if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
            continue;
        receiver = callee->data.member.object;
        method_name = callee->data.member.name;
        if (receiver == NULL || receiver->type != AST_IDENTIFIER
            || receiver->data.identifier.name == NULL || method_name == NULL) {
            continue;
        }

        alias = receiver->data.identifier.name;
        subject_name = intent_zone_binding_type_name_with_metadata(
            intent, alias, participant_aliases, participant_types, participant_count);
        action_decl = find_subject_action_decl(ctx, subject_name, method_name);
        if (action_decl != NULL && action_decl->type == AST_FUNC_DECL
            && action_decl->data.func_decl.causes_effect != NULL) {
            return action_decl->data.func_decl.causes_effect;
        }
    }

    return NULL;
}

static void
emit_intent_step_sync_effective_zone(CodeBuf *out, TranspilerCtx *ctx,
                                     ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;

    if (out == NULL || ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_sync_effective_zone_with_metadata(CodeBuf *out, TranspilerCtx *ctx,
                                                   const char *zone_type,
                                                   const char *zone_alias)
{
    if (out == NULL || ctx == NULL || zone_alias == NULL || zone_type == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}
