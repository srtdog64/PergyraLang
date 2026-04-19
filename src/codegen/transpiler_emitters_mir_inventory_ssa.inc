static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    g_type_render_ctx = ctx;
    ensure_type_specializations_from_ast(ctx, node->data.func_decl.return_type);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (pt == NULL) {
            transpiler_set_backend_error_with_hints(ctx, "PGY_C_TYPE_UNSUPPORTED", "c_codegen:type:unsupported", "use-llvm-backend-or-extend-transpiler", "cannot determine parameter type for forward declaration '%s' at argument %zu",
                name != NULL ? name : "<function>",
                i);
            if (params_sig != NULL)
                codebuf_destroy(params_sig);
            free(header_decl);
            g_type_render_ctx = saved_render_ctx;
            return;
        }
        if (i > 0)
            codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(buf, "%s;\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);
    g_type_render_ctx = saved_render_ctx;
}

static void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, node->data.func_decl.name, buf, ctx);
}

static const MIRRoutine *
transpiler_find_mir_function(const TranspilerCtx *ctx, const ASTNode *func_decl)
{
    if (ctx == NULL || ctx->mir == NULL || func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    const char *target = func_decl->data.func_decl.name;
    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_FUNCTION)
            continue;
        if (routine->name == NULL)
            continue;
        /* Exact match */
        if (strcmp(routine->name, target) == 0)
            return routine;
        /* Specialized name: e.g. "Identity_Int" matches "Identity" */
        size_t name_len = strlen(target);
        if (strncmp(routine->name, target, name_len) == 0
            && (routine->name[name_len] == '_' || routine->name[name_len] == '\0'))
            return routine;
    }

    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_intent(const TranspilerCtx *ctx, const ASTNode *intent_decl)
{
    if (ctx == NULL || ctx->mir == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL
        || intent_decl->data.intent_decl.name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_INTENT
            || routine->name == NULL
            || strcmp(routine->name, intent_decl->data.intent_decl.name) != 0) {
            continue;
        }
        /* Match by name only - pointer comparison may fail across different AST instances */
        return routine;
    }

    return NULL;
}

static const char *
transpiler_find_mir_intent_meta_arg(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name)
{
    if (routine == NULL || inst_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, inst_name) != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->arg0;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                          const char *step_name,
                                          const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentWho") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

static size_t
transpiler_collect_mir_intent_participants(const MIRRoutine *routine,
                                           const char ***aliases_out,
                                           const char ***types_out)
{
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (types_out != NULL)
        *types_out = NULL;
    if (routine == NULL || aliases_out == NULL || types_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown_aliases;
            const char **grown_types;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentParticipant") != 0)
                continue;
            if (inst->arg0 == NULL || inst->arg1 == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown_aliases = malloc(new_capacity * sizeof(const char *));
                grown_types = malloc(new_capacity * sizeof(const char *));
                if (grown_aliases == NULL || grown_types == NULL) {
                    free((void *)grown_aliases);
                    free((void *)grown_types);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                if (count > 0) {
                    memcpy((void *)grown_aliases, (const void *)aliases,
                           count * sizeof(const char *));
                    memcpy((void *)grown_types, (const void *)types,
                           count * sizeof(const char *));
                }
                free((void *)aliases);
                free((void *)types);
                aliases = grown_aliases;
                types = grown_types;
                capacity = new_capacity;
            }
            aliases[count] = inst->arg0;
            types[count] = inst->arg1;
            count++;
        }
    }

    *aliases_out = aliases;
    *types_out = types;
    return count;
}

static size_t
transpiler_collect_mir_intent_steps(const MIRRoutine *routine, ASTNode ***steps_out)
{
    ASTNode **steps = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (steps_out != NULL)
        *steps_out = NULL;
    if (routine == NULL || steps_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL
                || inst->ast->type != AST_INTENT_STEP) {
                continue;
            }
            if (inst->name == NULL || strcmp(inst->name, "IntentStep") != 0)
                continue;
            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc(steps, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(steps);
                    return 0;
                }
                steps = grown;
                capacity = new_capacity;
            }
            steps[count++] = inst->ast;
        }
    }

    *steps_out = steps;
    return count;
}

static size_t
transpiler_collect_mir_intent_step_names(const MIRRoutine *routine, const char ***names_out)
{
    const char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (names_out != NULL)
        *names_out = NULL;
    if (routine == NULL || names_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentStep") != 0)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc((void *)names, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)names);
                    return 0;
                }
                names = grown;
                capacity = new_capacity;
            }
            names[count++] = inst->arg0 != NULL ? inst->arg0 : inst->name;
        }
    }

    *names_out = names;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_check_expr(const MIRRoutine *routine,
                                      const char *step_name,
                                      const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentCheck") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->ast;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                         const char *step_name,
                                         const char *phase_name,
                                         ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentEval") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc(exprs, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(exprs);
                    return 0;
                }
                exprs = grown;
                capacity = new_capacity;
            }
            exprs[count++] = inst->ast;
        }
    }

    *exprs_out = exprs;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_eval_expr(const MIRRoutine *routine,
                                     const char *step_name,
                                     const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = transpiler_collect_mir_intent_eval_exprs(
        routine, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    free(exprs);
    return result;
}

static size_t
transpiler_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                               const char *step_name,
                                               const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentDispatch") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

static bool
transpiler_parse_versioned_name(const char *versioned, char *base, size_t base_size,
                                size_t *version_out)
{
    const char *dot;
    size_t len;
    if (versioned == NULL || base == NULL || base_size == 0 || version_out == NULL)
        return false;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return false;
    len = (size_t)(dot - versioned);
    if (len + 1 > base_size)
        return false;
    memcpy(base, versioned, len);
    base[len] = '\0';
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return true;
}

static size_t
transpiler_ssa_name_bucket_index(const char *key)
{
    size_t hash = 5381u;
    const unsigned char *u = (const unsigned char *)key;
    while (u != NULL && *u != '\0') {
        hash = ((hash << 5) + hash) + (size_t)(*u);
        ++u;
    }
    return hash % TRANSPILE_SSA_NAME_BUCKETS;
}

static void
transpiler_ssa_map_clear(TranspilerSSANameMap *map)
{
    if (map == NULL)
        return;
    for (size_t i = 0; i < TRANSPILE_SSA_NAME_BUCKETS; ++i) {
        if (map->buckets[i].in_use && map->buckets[i].base_name != NULL)
            free((void *)map->buckets[i].base_name);
    }
    memset(map, 0, sizeof(*map));
}

static bool
transpiler_ssa_name_map_set(TranspilerSSANameMap *map,
                            const char *base_name,
                            const char *versioned_name)
{
    size_t idx;
    size_t attempts;

    if (map == NULL || base_name == NULL || versioned_name == NULL)
        return false;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        TranspilerSSANameBucket *bucket = &map->buckets[idx];
        if (!bucket->in_use) {
            bucket->in_use = true;
            bucket->base_name = pergyra_strdup(base_name);
            if (bucket->base_name == NULL) {
                bucket->in_use = false;
                return false;
            }
            bucket->versioned_name = versioned_name;
            return true;
        }
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0) {
            bucket->versioned_name = versioned_name;
            return true;
        }
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return false;
}

static bool
transpiler_collect_ssa_name_entries(const char **versioned_values,
                                   size_t value_count,
                                   const char **base_names,
                                   const char **versioned_names,
                                   size_t max_entries,
                                   size_t *map_count_out)
{
    size_t map_count = 0;

    if (versioned_values == NULL
        || base_names == NULL
        || versioned_names == NULL
        || max_entries == 0) {
        return true;
    }
    for (size_t i = 0; i < value_count; i++) {
        const char *versioned = versioned_values[i];
        char base[128];
        size_t parsed_version = 0;
        bool replaced = false;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned, base, sizeof(base), &parsed_version))
            continue;
        for (size_t j = 0; j < map_count; j++) {
            if (base_names[j] != NULL && strcmp(base_names[j], base) == 0) {
                versioned_names[j] = versioned;
                replaced = true;
                break;
            }
        }
        if (replaced)
            continue;
        if (map_count >= max_entries)
            return false;
        base_names[map_count] = pergyra_strdup(base);
        versioned_names[map_count] = versioned;
        map_count++;
    }
    if (map_count_out != NULL)
        *map_count_out = map_count;
    return true;
}

static void
transpiler_free_ssa_name_entries(const char **base_names, size_t entry_count)
{
    for (size_t i = 0; i < entry_count; i++) {
        free((void *)base_names[i]);
    }
}

static bool
transpiler_rebuild_ssa_map(TranspilerSSANameMap *ssa_map,
                          const char **base_names,
                          const char **versioned_names,
                          size_t map_count)
{
    if (ssa_map == NULL || base_names == NULL || versioned_names == NULL) {
        if (ssa_map != NULL)
            transpiler_ssa_map_clear(ssa_map);
        return false;
    }
    transpiler_ssa_map_clear(ssa_map);
    for (size_t i = 0; i < map_count; i++) {
        if (base_names[i] == NULL || versioned_names[i] == NULL)
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map, base_names[i], versioned_names[i]))
            return false;
    }
    return true;
}

static void
transpiler_emit_mir_block_mapping_comment(CodeBuf *out,
                                          int indent,
                                          const char *routine_name,
                                          const MIRRoutine *routine,
                                          const MIRBasicBlock *block)
{
    const ASTNode *source_stmt = NULL;
    uint32_t line = 0;
    uint32_t column = 0;

    if (out == NULL || routine == NULL || block == NULL)
        return;

    source_stmt = block->source_ast;
    if (source_stmt != NULL) {
        line = source_stmt->line;
        column = source_stmt->column;
    }

    if (source_stmt != NULL) {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%zu (%s) src=%u:%u ast=%p */\n",
            block->id,
            block->source_hir_block_id,
            routine_name != NULL ? routine_name : "<routine>",
            line,
            column,
            (const void *)source_stmt);
    } else {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%s (%s) */\n",
            block->id,
            block->source_hir_block_id == SIZE_MAX ? "<none>" : "mapped",
            routine_name != NULL ? routine_name : "<routine>");
    }
}

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
    host_name = current_host_decl_name(ctx);
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
    if (current_world_has_field(ctx, base_name))
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
            const char *resolved = resolve_nominal_host_expr_type_name(ctx, expr);
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
    if (body->type == AST_FOR_LOOP)
        return transpiler_has_local_binding_in_block(body->data.for_loop.body, base_name);
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
            member_type = lookup_nominal_host_member_type_name(ctx,
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
