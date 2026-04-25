char *
emit_unary(ASTNode *expr, TranspilerCtx *ctx)
{
    /* Postfix ? ??try/propagate: expr? ??early return on error */
    if (expr->data.unary.op.type == TOKEN_QUESTION) {
        char *operand = emit_expression(expr->data.unary.operand, ctx);
        int try_id = ctx->tmp_counter++;
        char *result = strdup_fmt(
            "({ __typeof__(%s) __try_%d = %s; "
            "if (__try_%d.tag != PgyResultOk) return __try_%d; "
            "__try_%d.ok; })",
            operand, try_id, operand,
            try_id, try_id,
            try_id);
        free(operand);
        return result;
    }

    char *operand = emit_expression(expr->data.unary.operand, ctx);
    const char *op = (expr->data.unary.op.type == TOKEN_NOT) ? "!" : "-";
    char *result = strdup_fmt("(%s%s)", op, operand);
    free(operand);
    return result;
}
static char *
emit_call_builtin_dispatch(ASTNode *call, BuiltinKind bk, TranspilerCtx *ctx, bool *handled)
{
    *handled = true;
    switch (bk) {
    case BUILTIN_CLAIM_SLOT:
    case BUILTIN_CLAIM_SECURE_SLOT:
        return emit_builtin_claim_slot(call, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return emit_builtin_claim_device_slot(call, ctx);
    case BUILTIN_VIEW_READ:
    case BUILTIN_VIEW_WRITE:
    case BUILTIN_MOVE:
        return emit_builtin_view(call, ctx);
    case BUILTIN_WRITE:
        return emit_builtin_write(call, ctx);
    case BUILTIN_READ:
        return emit_builtin_read(call, ctx);
    case BUILTIN_RELEASE:
        return emit_builtin_release(call, ctx);
    case BUILTIN_DEVICE_WRITE:
        return emit_builtin_device_write(call, ctx);
    case BUILTIN_DEVICE_READ:
        return emit_builtin_device_read(call, ctx);
    case BUILTIN_RELEASE_DEVICE_SLOT:
        return emit_builtin_release_device_slot(call, ctx);
    case BUILTIN_SUBMIT_DEVICE_READ:
        return emit_builtin_submit_device_read(call, ctx);
    case BUILTIN_LOG:
        return emit_builtin_log(call, ctx);
    case BUILTIN_LOG_RAW:
        return emit_builtin_log_raw(call, ctx);
    case BUILTIN_LOG_BANNER:
    case BUILTIN_LOG_BLOCK:
        return emit_builtin_log_banner(call, ctx);
    case BUILTIN_CLONE:
        if (call->data.call.arg_count >= 1 && call->data.call.arguments[0] != NULL)
            return emit_expression(call->data.call.arguments[0], ctx);
        return pergyra_strdup("0");
    case BUILTIN_RC_NEW:
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_DOWNGRADE:
    case BUILTIN_RC_GET:
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        return emit_builtin_rc(call, bk, ctx);
    case BUILTIN_BOX:
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
    case BUILTIN_BOX_ARRAY:
        return emit_builtin_box(call, bk, ctx);
    case BUILTIN_TO_OBJECT:
        return emit_builtin_to_dto(call, ctx);
    case BUILTIN_TO_TOBJECT:
        return emit_builtin_to_dto(call, ctx);
    case BUILTIN_HAS_PROJECTION:
        if (call->data.call.arg_count == 1 && call->data.call.arguments[0] != NULL) {
            const char *slot_name = NULL;
            ASTNode *slot_decl = NULL;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                slot_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                slot_name = call->data.call.arguments[0]->data.string.value;
            slot_decl = current_overlay_domain_slot_decl(ctx, slot_name);
            if (slot_decl != NULL
                && slot_decl->type == AST_DOMAIN_SLOT
                && !slot_decl->data.domain_slot.is_subject) {
                return strdup_fmt("self->__projection_ready_%s", slot_name);
            }
            if (slot_name != NULL) {
                ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
                if (host_decl != NULL
                    && (host_decl->type == AST_RELATION_DECL
                        || host_decl->type == AST_EFFECT_DECL
                        || host_decl->type == AST_ZONE_DECL)) {
                    return strdup_fmt("self->__projection_ready_%s", slot_name);
                }
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasProjection requires active relation/effect/zone projection context");
        return pergyra_strdup("false");
    case BUILTIN_HAS_LAYER:
        {
            ASTNode *zone_decl = NULL;
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (host_decl != NULL && host_decl->type == AST_ZONE_DECL)
                zone_decl = host_decl;
            if (zone_decl != NULL
            && call->data.call.arg_count == 1
            && call->data.call.arguments[0] != NULL) {
                const char *layer_name = NULL;
                if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                    layer_name = call->data.call.arguments[0]->data.identifier.name;
                else if (call->data.call.arguments[0]->type == AST_STRING)
                    layer_name = call->data.call.arguments[0]->data.string.value;
                if (layer_name != NULL)
                    return strdup_fmt("%s_has_layer_%s(self, __pgy_zone_gen)",
                        zone_decl->data.zone_decl.name, layer_name);
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasLayer requires active zone context");
        return pergyra_strdup("false");
    case BUILTIN_HAS_STATE:
        {
            ASTNode *zone_decl = NULL;
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (host_decl != NULL && host_decl->type == AST_ZONE_DECL)
                zone_decl = host_decl;
            if (zone_decl != NULL
            && call->data.call.arg_count >= 1
            && call->data.call.arguments[0] != NULL) {
                const char *state_name = NULL;
                ASTNode *state_decl;
                if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                    state_name = call->data.call.arguments[0]->data.identifier.name;
                else if (call->data.call.arguments[0]->type == AST_STRING)
                    state_name = call->data.call.arguments[0]->data.string.value;
                state_decl = transpiler_find_zone_state_decl(zone_decl, state_name);
                if (state_decl != NULL && state_name != NULL)
                    return strdup_fmt("self->__state_%s", state_name);
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasState requires active zone context with matching state");
        return pergyra_strdup("false");
    case BUILTIN_HAS_ZONE:
        {
            ASTNode *world_decl = NULL;
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (host_decl != NULL && host_decl->type == AST_WORLD_DECL)
                world_decl = host_decl;
            if (world_decl != NULL
            && call->data.call.arg_count >= 1
            && call->data.call.arguments[0] != NULL) {
                const char *name = NULL;
                ASTNode *state_decl;
                if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                    name = call->data.call.arguments[0]->data.identifier.name;
                else if (call->data.call.arguments[0]->type == AST_STRING)
                    name = call->data.call.arguments[0]->data.string.value;
                state_decl = find_world_state_decl(world_decl, name);
                if (state_decl != NULL && name != NULL)
                    return strdup_fmt("self->__zone_state_%s", name);
                if (transpiler_world_has_zone_slot(world_decl, name))
                    return strdup_fmt("self->__zone_active_%s", name);
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasZone requires active world context");
        return pergyra_strdup("false");
    case BUILTIN_HAS_ZONE_PROJECTION:
        {
            ASTNode *world_decl = NULL;
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (host_decl != NULL && host_decl->type == AST_WORLD_DECL)
                world_decl = host_decl;
            if (world_decl != NULL
            && call->data.call.arg_count == 2
            && call->data.call.arguments[0] != NULL
            && call->data.call.arguments[1] != NULL) {
                const char *zone_name = NULL;
                const char *slot_name = NULL;
                ASTNode *zone_decl;
                ASTNode *slot_decl;
                if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                    zone_name = call->data.call.arguments[0]->data.identifier.name;
                else if (call->data.call.arguments[0]->type == AST_STRING)
                    zone_name = call->data.call.arguments[0]->data.string.value;
                if (call->data.call.arguments[1]->type == AST_IDENTIFIER)
                    slot_name = call->data.call.arguments[1]->data.identifier.name;
                else if (call->data.call.arguments[1]->type == AST_STRING)
                    slot_name = call->data.call.arguments[1]->data.string.value;
                zone_decl = transpiler_resolve_world_zone_decl(ctx, world_decl, zone_name);
                slot_decl = zone_decl != NULL && slot_name != NULL
                    ? transpiler_find_zone_domain_slot(zone_decl, slot_name)
                    : NULL;
                if (zone_decl != NULL && slot_decl != NULL
                    && !slot_decl->data.domain_slot.is_subject) {
                    return strdup_fmt("self->%s.__projection_ready_%s", zone_name, slot_name);
                }
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasZoneProjection requires active world context with matching zone projection");
        return pergyra_strdup("false");
    case BUILTIN_HAS_ZONE_LAYER:
        {
            ASTNode *world_decl = NULL;
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (host_decl != NULL && host_decl->type == AST_WORLD_DECL)
                world_decl = host_decl;
            if (world_decl != NULL
            && call->data.call.arg_count == 2
            && call->data.call.arguments[0] != NULL
            && call->data.call.arguments[1] != NULL) {
                const char *zone_name = NULL;
                const char *layer_name = NULL;
                ASTNode *zone_decl;
                if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                    zone_name = call->data.call.arguments[0]->data.identifier.name;
                else if (call->data.call.arguments[0]->type == AST_STRING)
                    zone_name = call->data.call.arguments[0]->data.string.value;
                if (call->data.call.arguments[1]->type == AST_IDENTIFIER)
                    layer_name = call->data.call.arguments[1]->data.identifier.name;
                else if (call->data.call.arguments[1]->type == AST_STRING)
                    layer_name = call->data.call.arguments[1]->data.string.value;
                zone_decl = transpiler_resolve_world_zone_decl(ctx, world_decl, zone_name);
                if (zone_decl != NULL && layer_name != NULL
                    && transpiler_find_zone_layer_slot(zone_decl, layer_name) != NULL) {
                    return strdup_fmt("self->%s.__layer_active_%s", zone_name, layer_name);
                }
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasZoneLayer requires active world context with matching zone layer");
        return pergyra_strdup("false");
    case BUILTIN_HAS_ZONE_STATE:
        {
            ASTNode *world_decl = NULL;
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (host_decl != NULL && host_decl->type == AST_WORLD_DECL)
                world_decl = host_decl;
            if (world_decl != NULL
            && call->data.call.arg_count == 2
            && call->data.call.arguments[0] != NULL
            && call->data.call.arguments[1] != NULL) {
                const char *zone_name = NULL;
                const char *state_name = NULL;
                ASTNode *zone_decl;
                if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                    zone_name = call->data.call.arguments[0]->data.identifier.name;
                else if (call->data.call.arguments[0]->type == AST_STRING)
                    zone_name = call->data.call.arguments[0]->data.string.value;
                if (call->data.call.arguments[1]->type == AST_IDENTIFIER)
                    state_name = call->data.call.arguments[1]->data.identifier.name;
                else if (call->data.call.arguments[1]->type == AST_STRING)
                    state_name = call->data.call.arguments[1]->data.string.value;
                zone_decl = transpiler_resolve_world_zone_decl(ctx, world_decl, zone_name);
                if (zone_decl != NULL && state_name != NULL
                    && transpiler_find_zone_state_decl(zone_decl, state_name) != NULL) {
                    return strdup_fmt("self->%s.__state_%s", zone_name, state_name);
                }
            }
        }
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: HasZoneState requires active world context with matching zone state");
        return pergyra_strdup("false");
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
    case BUILTIN_ALLOCATOR_POOL:
        return emit_builtin_allocator(call, bk, ctx);
    /* I/O built-ins */
    case BUILTIN_FILE_OPEN: {
        /* FileOpen(path, mode) ??pgy_file_open(path, mode) */
        if (call->data.call.arg_count < 2) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: FileOpen requires path and mode");
            return pergyra_strdup("0");
        }
        char *path = emit_expression(call->data.call.arguments[0], ctx);
        char *mode = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_file_open(%s, %s)", path, mode);
        free(path); free(mode);
        return result;
    }
    case BUILTIN_FILE_READ: {
        /* FileRead(fd) ??pgy_file_read(fd) */
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: FileRead requires file descriptor");
            return pergyra_strdup("0");
        }
        char *fd = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_file_read(%s)", fd);
        free(fd);
        return result;
    }
    case BUILTIN_FILE_WRITE: {
        /* FileWrite(fd, data) ??pgy_file_write(fd, data) */
        if (call->data.call.arg_count < 2) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: FileWrite requires file descriptor and data");
            return pergyra_strdup("0");
        }
        char *fd   = emit_expression(call->data.call.arguments[0], ctx);
        char *data = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_file_write(%s, %s)", fd, data);
        free(fd); free(data);
        return result;
    }
    case BUILTIN_FILE_CLOSE: {
        /* FileClose(fd) ??pgy_file_close(fd) */
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: FileClose requires file descriptor");
            return pergyra_strdup("0");
        }
        char *fd = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_file_close(%s)", fd);
        free(fd);
        return result;
    }
    case BUILTIN_READ_FILE: {
        /* ReadFile(path) ??pgy_read_file(path) */
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: ReadFile requires path");
            return pergyra_strdup("0");
        }
        char *path = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_read_file(%s)", path);
        free(path);
        return result;
    }
    case BUILTIN_WRITE_FILE: {
        /* WriteFile(path, data) ??pgy_write_file(path, data) */
        if (call->data.call.arg_count < 2) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: WriteFile requires path and data");
            return pergyra_strdup("0");
        }
        char *path = emit_expression(call->data.call.arguments[0], ctx);
        char *data = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_write_file(%s, %s)", path, data);
        free(path); free(data);
        return result;
    }
    case BUILTIN_INPUT: {
        /* Input(prompt) ??pgy_input(prompt) */
        char *prompt = (call->data.call.arg_count >= 1)
            ? emit_expression(call->data.call.arguments[0], ctx)
            : pergyra_strdup("\"\"");
        char *result = strdup_fmt("pgy_input(%s)", prompt);
        free(prompt);
        return result;
    }
    case BUILTIN_PRINT: {
        /* Print(msg) ??pgy_print(msg) */
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Print requires message");
            return pergyra_strdup("0");
        }
        char *msg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_print(%s)", msg);
        free(msg);
        return result;
    }
    case BUILTIN_READ_LINE:
        return pergyra_strdup("pgy_input(\"\")");
    case BUILTIN_NOW:
        return pergyra_strdup("pgy_now_ms()");
    case BUILTIN_SLEEP: {
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Sleep requires milliseconds");
            return pergyra_strdup("0");
        }
        char *ms = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_sleep_ms(%s)", ms);
        free(ms);
        return result;
    }
    case BUILTIN_INTENT_LAST_TRACE:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_trace_export()");
    case BUILTIN_INTENT_LAST_FAILURE:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_failure_export()");
    case BUILTIN_INTENT_LAST_NAME:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_name_export()");
    case BUILTIN_INTENT_LAST_HANDLE:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_handle_export()");
    case BUILTIN_INTENT_LAST_TRACE_ID:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_trace_id_export()");
    case BUILTIN_INTENT_LAST_STEP_COUNT:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_step_count_export()");
    case BUILTIN_INTENT_LAST_FAILED:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_last_failed_export()");
    case BUILTIN_INTENT_HISTORY_COUNT:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_history_count_export()");
    case BUILTIN_INTENT_HISTORY_STEP_NAME: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_name_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_ZONE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_zone_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_PHASE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_phase_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_participant_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_SLOT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_slot_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_from_zone_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_from_slot_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_to_zone_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_to_slot_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_OK: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_ok_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_history_step_failure_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_COUNT:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_active_count_export()");
    case BUILTIN_INTENT_ACTIVE_NAME: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_name_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_HANDLE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_handle_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_parent_handle_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_TRACE_ID: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_trace_id_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_PRIORITY: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_priority_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_subject_count_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_count_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_CONCURRENT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_concurrent_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_FAILED: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_failed_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_FAILURE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_failure_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_TRACE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_active_trace_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_NAME: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_name_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_zone_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_phase_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_participant_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_slot_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_from_zone_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_from_slot_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_to_zone_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_to_slot_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_OK: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_ok_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE: {
        ctx->uses_intent_observability = true;
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_intent_active_step_failure_export(%s, %s)", intent_index, step_index);
        free(intent_index); free(step_index);
        return result;
    }
    case BUILTIN_INTENT_CURRENT_HANDLE:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_current_handle_export()");
    case BUILTIN_INTENT_RECENT_COUNT:
        ctx->uses_intent_observability = true;
        return pergyra_strdup("pgy_intent_recent_count_export()");
    case BUILTIN_INTENT_RECENT_HANDLE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_handle_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_RECENT_TRACE_ID: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_trace_id_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_RECENT_NAME: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_name_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_RECENT_TRACE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_trace_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_RECENT_FAILURE: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_failure_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_RECENT_STEP_COUNT: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_step_count_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_INTENT_RECENT_FAILED: {
        ctx->uses_intent_observability = true;
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_intent_recent_failed_export(%s)", index);
        free(index);
        return result;
    }
    case BUILTIN_PARALLEL:
        /* parallel { ... } is a statement, not an expression.
         * If encountered as expression, emit empty compound literal. */
        return pergyra_strdup("((void)0)");
    default:
        *handled = false;
        return NULL;
    }
}
