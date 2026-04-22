/* -----------------------------------------------------------------
 * Built-in call emitters
 * ----------------------------------------------------------------- */

static char *normalize_banner_string_literal(const char *src);
static bool transpiler_type_name_is_slot_like(const char *type_name);
static bool transpiler_parse_versioned_name(const char *versioned,
                                            char *base,
                                            size_t base_size,
                                            size_t *version_out);

char *
emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx)
{
    /*
     * ClaimSlot<T>() is handled in let_decl where the type is known.
     * If encountered standalone, emit a comment.
     */
    (void)call;
    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: standalone ClaimSlot expression is unsupported; ClaimSlot<T>() must lower through let binding");
    return pergyra_strdup("0");
}

static char *
emit_builtin_claim_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    (void)call;
    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: standalone ClaimDeviceSlot expression is unsupported; ClaimDeviceSlot<T>() must lower through let binding");
    return pergyra_strdup("pgy_claim_device_Int()");
}

char *
emit_builtin_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 2) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Write requires slot and value");
        return pergyra_strdup("0");
    }

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = call->data.call.arguments[0];
    (void)resolve_slot_target(ctx, slot_arg, &inner, &slot_name, &secure);

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *value_expr = emit_expression(call->data.call.arguments[1], ctx);

    char *result;
    if (call->data.call.arg_count >= 3) {
        /* SecureSlot: Write(slot, value, token) */
        char *token_expr = emit_expression(call->data.call.arguments[2], ctx);
        result = strdup_fmt(
            "pgy_secure_write_%s(%s, %s, &%s)",
            inner, slot_ref, value_expr, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *token_name = lookup_slot_token_name(ctx, slot_name);
        char fallback_token[96];
        if (token_name == NULL) {
            snprintf(fallback_token, sizeof(fallback_token), "%s_token", slot_name);
            token_name = fallback_token;
        }
        result = strdup_fmt(
            "pgy_secure_write_%s(%s, %s, &%s)",
            inner, slot_ref, value_expr, token_name);
    } else {
        /* Plain slot: Write(slot, value) */
        result = strdup_fmt(
            "pgy_write_%s(%s, %s)",
            inner, slot_ref, value_expr);
    }

    free(slot_ref);
    free(slot_expr);
    free(value_expr);
    return result;
}

static char *
emit_builtin_view(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: View requires source slot");
        return pergyra_strdup("0");
    }

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(call->data.call.arguments[0], ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    return slot_expr;
}

static bool
resolve_slot_target(TranspilerCtx *ctx, ASTNode *slot_arg,
                    const char **inner_out, const char **slot_name_out,
                    bool *secure_out)
{
    const char *inner = NULL;
    const char *slot_name = NULL;
    bool secure = false;

    if (slot_arg == NULL)
        return false;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *id = slot_arg->data.identifier.name;
        TypedVarEntry *entry = lookup_typed_entry(ctx, id);
        if (entry != NULL && (entry->is_view || entry->is_move_token)
            && entry->source_slot[0] != '\0') {
            slot_name = entry->source_slot;
            secure = entry->source_secure;
            inner = slot_inner_type_name(entry->type_name);
        } else {
            slot_name = id;
            inner = lookup_slot_type(ctx, id);
            secure = lookup_slot_is_secure(ctx, id);
        }
    } else if (slot_arg->type == AST_CALL
               && slot_arg->data.call.callee != NULL
               && slot_arg->data.call.callee->type == AST_IDENTIFIER
               && slot_arg->data.call.arg_count >= 1
               && slot_arg->data.call.arguments[0] != NULL
               && slot_arg->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee = slot_arg->data.call.callee->data.identifier.name;
        const char *src = slot_arg->data.call.arguments[0]->data.identifier.name;
        if (callee != NULL
            && (strcmp(callee, "ViewRead") == 0
                || strcmp(callee, "ViewWrite") == 0
                || strcmp(callee, "Move") == 0)) {
            slot_name = src;
            inner = lookup_slot_type(ctx, src);
            secure = lookup_slot_is_secure(ctx, src);
        }
    }

    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for '%s'",
            slot_name != NULL ? slot_name : "<slot>");
        return false;
    }
    if (inner_out != NULL)
        *inner_out = inner;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (secure_out != NULL)
        *secure_out = secure;
    return slot_name != NULL;
}

char *
emit_builtin_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Read requires slot");
        return pergyra_strdup("0");
    }

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = call->data.call.arguments[0];
    (void)resolve_slot_target(ctx, slot_arg, &inner, &slot_name, &secure);

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *result;

    if (call->data.call.arg_count >= 2) {
        char *token_expr = emit_expression(call->data.call.arguments[1], ctx);
        result = strdup_fmt(
            "pgy_secure_read_%s(%s, &%s)",
            inner, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *token_name = lookup_slot_token_name(ctx, slot_name);
        char fallback_token[96];
        if (token_name == NULL) {
            snprintf(fallback_token, sizeof(fallback_token), "%s_token", slot_name);
            token_name = fallback_token;
        }
        result = strdup_fmt("pgy_secure_read_%s(%s, &%s)",
            inner, slot_ref, token_name);
    } else {
        result = strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
    }

    free(slot_ref);
    free(slot_expr);
    return result;
}

char *
emit_builtin_release(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Release requires slot");
        return pergyra_strdup("0");
    }

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = call->data.call.arguments[0];
    (void)resolve_slot_target(ctx, slot_arg, &inner, &slot_name, &secure);

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *result;

    if (call->data.call.arg_count >= 2) {
        char *token_expr = emit_expression(call->data.call.arguments[1], ctx);
        result = strdup_fmt(
            "pgy_secure_release_%s(%s, &%s)",
            inner, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *token_name = lookup_slot_token_name(ctx, slot_name);
        char fallback_token[96];
        if (token_name == NULL) {
            snprintf(fallback_token, sizeof(fallback_token), "%s_token", slot_name);
            token_name = fallback_token;
        }
        result = strdup_fmt("pgy_secure_release_%s(%s, &%s)",
            inner, slot_ref, token_name);
    } else {
        result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
    }

    /* Mark slot as explicitly released — prevents auto-release at scope exit */
    if (slot_arg->type == AST_IDENTIFIER) {
        const char *sname = slot_name != NULL ? slot_name : slot_arg->data.identifier.name;
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].name, sname) == 0) {
                ctx->slot_vars[i].released = true;
                break;
            }
        }
    }

    free(slot_ref);
    free(slot_expr);
    return result;
}

static char *
emit_builtin_device_write(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *value_expr = emit_expression(call->data.call.arguments[1], ctx);
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_device_write_%s(&%s, %s)", inner, slot_expr, value_expr);
    free(slot_expr);
    free(value_expr);
    return result;
}

static char *
emit_builtin_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_device_read_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

static char *
emit_builtin_release_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_release_device_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

static char *
emit_builtin_submit_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_submit_device_read_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

char *
emit_builtin_log(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (call->data.call.arg_count == 1) {
        ASTNode *arg_node = call->data.call.arguments[0];
        if (arg_node != NULL && arg_node->type == AST_STRING
            && arg_node->data.string.value != NULL) {
            const char *raw = arg_node->data.string.value;
            bool multiline = (strchr(raw, '\n') != NULL)
                           || (strchr(raw, '\r') != NULL);
            char *escaped = escape_c_string_literal(arg_node->data.string.value);
            if (escaped != NULL) {
                char *result;
                if (multiline) {
                    char *normalized = normalize_banner_string_literal(raw);
                    if (normalized == NULL)
                        normalized = pergyra_strdup(raw);
                    free(escaped);
                    escaped = escape_c_string_literal(normalized);
                    free(normalized);
                    if (escaped == NULL) {
                        return pergyra_strdup("/* Log: failed to normalize string */");
                    }
                }
                result = strdup_fmt("pgy_log%s(\"%s\")", multiline ? "_banner" : "", escaped);
                free(escaped);
                return result;
            }
            return pergyra_strdup("/* Log: failed to escape string */");
        }
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    /* Multi-arg Log: emit each argument with pgy_log() */
    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_builtin_log_raw(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (call->data.call.arg_count == 1) {
        ASTNode *arg_node = call->data.call.arguments[0];

        if (arg_node != NULL && arg_node->type == AST_STRING
            && arg_node->data.string.value != NULL) {
            char *escaped = escape_c_string_literal(arg_node->data.string.value);
            if (escaped != NULL) {
                char *result = strdup_fmt("pgy_log(\"%s\")", escaped);
                free(escaped);
                return result;
            }
            return pergyra_strdup("/* LogRaw: failed to escape string */");
        }

        char *arg = emit_expression(arg_node, ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    /* Multi-arg LogRaw: emit each argument with pgy_log() */
    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static size_t
count_banner_line_indent(const char *line_start, const char *line_end)
{
    size_t indent = 0;

    while (line_start < line_end) {
        if (*line_start == ' ' || *line_start == '\t') {
            indent++;
            line_start++;
            continue;
        }
        break;
    }
    return indent;
}

static bool
line_is_empty_with_only_ws(const char *line_start, const char *line_end)
{
    for (const char *p = line_start; p < line_end; p++) {
        if (*p != ' ' && *p != '\t')
            return false;
    }
    return true;
}

static char *
normalize_banner_string_literal(const char *src)
{
    const char *cursor = src;
    const char *end;
    size_t min_indent = 0;
    bool found_content_line = false;
    CodeBuf *buf;

    if (src == NULL)
        return pergyra_strdup("");

    if (cursor[0] == '\n')
        cursor++;
    else if (cursor[0] == '\r') {
        cursor += (cursor[1] == '\n') ? 2 : 1;
    }

    end = cursor + strlen(cursor);
    while (end > cursor && (end[-1] == '\n' || end[-1] == '\r'))
        end--;

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        if (!line_is_empty_with_only_ws(line, line_end)) {
            size_t indent = count_banner_line_indent(line, line_end);
            if (!found_content_line || indent < min_indent) {
                min_indent = indent;
                found_content_line = true;
            }
        }

        if (line_end == end)
            break;
        if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
            line = line_end + 2;
        else
            line = line_end + 1;
    }

    if (!found_content_line)
        min_indent = 0;

    buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("");

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        const char *body_start = line;
        if (line_is_empty_with_only_ws(line, line_end))
            body_start = line_end;
        else {
            for (size_t i = 0; i < min_indent && body_start < line_end; i++) {
                if (*body_start == ' ' || *body_start == '\t')
                    body_start++;
                else
                    break;
            }
        }

        if (line_end > body_start)
            codebuf_write_raw(buf, body_start, (size_t)(line_end - body_start));

        if (line_end < end) {
            codebuf_write(buf, "\n");
            if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
                line = line_end + 2;
            else
                line = line_end + 1;
        } else {
            break;
        }
    }

    char *normalized = buf->data ? pergyra_strdup(buf->data) : pergyra_strdup("");
    codebuf_destroy(buf);
    return normalized;
}

char *
emit_builtin_log_banner(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1 || call->data.call.arguments[0] == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: LogBanner requires one argument");
        return pergyra_strdup("0");
    }

    if (call->data.call.arg_count != 1)
        return emit_builtin_log(call, ctx);

    ASTNode *arg_node = call->data.call.arguments[0];
    if (arg_node->type != AST_STRING)
        return emit_builtin_log(call, ctx);

    char *normalized = normalize_banner_string_literal(arg_node->data.string.value);
    if (normalized == NULL)
        return emit_builtin_log(call, ctx);

    char *escaped = escape_c_string_literal(normalized);
    if (escaped == NULL) {
        free(normalized);
        return emit_builtin_log(call, ctx);
    }
    char *result = strdup_fmt("pgy_log_banner(\"%s\")", escaped);
    free(normalized);
    free(escaped);
    return result;
}

static const char *
lookup_wrapped_inner_type(TranspilerCtx *ctx, ASTNode *arg, const char *wrapper)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, arg->data.identifier.name);
        size_t wrapper_len = strlen(wrapper);
        if (type_name != NULL && strncmp(type_name, wrapper, wrapper_len) == 0
            && type_name[wrapper_len] == '<') {
            return slot_inner_type_name(type_name);
        }
    }
    return "Int";
}

char *
emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_RC_NEW:
        if (call->data.call.arg_count != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: RcNew requires exactly one argument");
            return pergyra_strdup("0");
        }
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, arg->data.identifier.name);
            if (arg_type != NULL)
                inner = arg_type;
        }
        break;
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_GET:
    case BUILTIN_RC_DOWNGRADE:
        inner = lookup_wrapped_inner_type(ctx, arg, "Rc");
        break;
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        inner = lookup_wrapped_inner_type(ctx, arg, "Weak");
        break;
    default:
        break;
    }

    if (kind == BUILTIN_RC_NEW) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_CLONE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_clone_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("(*pgy_rc_get_%s(&%s))", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DOWNGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_downgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_UPGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_upgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported Rc builtin kind %d", (int)kind);
    return pergyra_strdup("0");
}

char *
emit_builtin_box(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_BOX:
        if (call->data.call.arg_count != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Box requires exactly one argument");
            return pergyra_strdup("0");
        }
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_CALL
                 && arg->data.call.callee != NULL
                 && arg->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee_name = arg->data.call.callee->data.identifier.name;
            ASTNode *class_decl = find_class_decl(ctx, callee_name);
            if (class_decl != NULL && class_decl->type == AST_CLASS_DECL)
                inner = callee_name;
            else {
                const char *arg_type = infer_expression_type_name(ctx, arg);
                if (arg_type != NULL)
                    inner = arg_type;
            }
        }
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, arg->data.identifier.name);
            if (arg_type != NULL)
                inner = arg_type;
        } else {
            const char *arg_type = infer_expression_type_name(ctx, arg);
            if (arg_type != NULL)
                inner = arg_type;
        }
        break;
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
        inner = lookup_wrapped_inner_type(ctx, arg, "Box");
        break;
    case BUILTIN_BOX_ARRAY:
        /* BoxArray(arr) → Box<Array<T>> wrapper around an existing array */
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: BoxArray requires an array argument");
            return pergyra_strdup("0");
        }
        {
            const char *arr_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = arr_type;
            else if (arr_type != NULL)
                inner = arr_type;
        }
        break;
    default:
        break;
    }

    if (kind == BUILTIN_BOX) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_get_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_SET) {
        char *box_expr = emit_expression(arg, ctx);
        char *value = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_box_set_%s(&%s, %s)", inner, box_expr, value);
        free(box_expr);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_IS_VALID) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_is_valid_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_ARRAY) {
        /* BoxArray(arr) → pgy_box_new_Array_T(arr) — wraps array in a box */
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: BoxArray requires an array argument");
            return pergyra_strdup("0");
        }
        char *arr_expr = emit_expression(call->data.call.arguments[0], ctx);
        /* Extract inner type from Array<T> */
        const char *elem = slot_inner_type_name(inner);
        char *result = strdup_fmt("pgy_box_new_Array_%s(%s)", elem, arr_expr);
        free(arr_expr);
        return result;
    }

    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported Box builtin kind %d", (int)kind);
    return pergyra_strdup("0");
}

char *
emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    switch (kind) {
    case BUILTIN_ALLOCATOR_SYSTEM:
        return pergyra_strdup("pgy_allocator_system()");
    case BUILTIN_ALLOCATOR_TRACING:
        return pergyra_strdup("pgy_allocator_tracing()");
    case BUILTIN_ALLOCATOR_DEBUG:
        return pergyra_strdup("pgy_allocator_debug()");
    case BUILTIN_ALLOCATOR_POOL:
        if (call->data.call.arg_count != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: AllocatorPool requires exactly one capacity argument");
            return pergyra_strdup("0");
        }
        {
            char *cap = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_allocator_pool(%s)", cap);
            free(cap);
            return result;
        }
    default:
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported allocator builtin kind %d", (int)kind);
        return pergyra_strdup("0");
    }
}

/* -----------------------------------------------------------------
 * Token operator helpers
 * ----------------------------------------------------------------- */

static const char *
binary_op_to_c(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "+";
    case TOKEN_MINUS:         return "-";
    case TOKEN_STAR:          return "*";
    case TOKEN_SLASH:         return "/";
    case TOKEN_PERCENT:       return "%";
    case TOKEN_EQUAL:         return "==";
    case TOKEN_NOT_EQUAL:     return "!=";
    case TOKEN_LESS:          return "<";
    case TOKEN_LESS_EQUAL:    return "<=";
    case TOKEN_GREATER:       return ">";
    case TOKEN_GREATER_EQUAL: return ">=";
    case TOKEN_AND:           return "&&";
    case TOKEN_OR:            return "||";
    default:                  return "?";
    }
}

/* -----------------------------------------------------------------
 * Expression emitters
 * ----------------------------------------------------------------- */

char *
emit_binary(ASTNode *expr, TranspilerCtx *ctx)
{
    /* String concatenation: detect String anywhere in the + chain so
     * parameter/local-backed cases like `name + "..."` lower correctly. */
    if (expr->data.binary.op.type == TOKEN_PLUS) {
        bool is_string_literal_chain = false;
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *rt = infer_expression_type_name(ctx, expr->data.binary.right);
        bool is_string =
            (lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0);
        if (expr->data.binary.left != NULL && expr->data.binary.left->type == AST_STRING)
            is_string_literal_chain = true;
        if (expr->data.binary.right != NULL && expr->data.binary.right->type == AST_STRING)
            is_string_literal_chain = true;

        if (!is_string) {
            ASTNode *cursor = expr;
            while (cursor != NULL && cursor->type == AST_BINARY
                   && cursor->data.binary.op.type == TOKEN_PLUS) {
                const char *left_leaf_t = infer_expression_type_name(ctx, cursor->data.binary.left);
                const char *right_leaf_t = infer_expression_type_name(ctx, cursor->data.binary.right);
                if ((cursor->data.binary.left != NULL
                        && cursor->data.binary.left->type == AST_STRING)
                    || (cursor->data.binary.right != NULL
                        && cursor->data.binary.right->type == AST_STRING)) {
                    is_string_literal_chain = true;
                }
                if ((left_leaf_t != NULL && strcmp(left_leaf_t, "String") == 0)
                    || (right_leaf_t != NULL && strcmp(right_leaf_t, "String") == 0)) {
                    is_string = true;
                    break;
                }
                cursor = cursor->data.binary.left;
            }
        }
        if (is_string || is_string_literal_chain) {
            char *left  = emit_expression(expr->data.binary.left,  ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    if (expr->data.binary.op.type == TOKEN_EQUAL
        || expr->data.binary.op.type == TOKEN_NOT_EQUAL) {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *rt = infer_expression_type_name(ctx, expr->data.binary.right);
        if ((lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0)) {
            char *left = emit_expression(expr->data.binary.left, ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            char *result = NULL;
            if (expr->data.binary.op.type == TOKEN_EQUAL)
                result = strdup_fmt("pgy_string_equals(%s, %s)", left, right);
            else
                result = strdup_fmt("(!pgy_string_equals(%s, %s))", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        char lt_buf[128];
        const char *stable_lt = lt;
        ASTNode *overload;

        if (lt != NULL) {
            snprintf(lt_buf, sizeof(lt_buf), "%s", lt);
            stable_lt = lt_buf;
        }

        overload = find_operator_overload_decl(ctx, stable_lt,
            expr->data.binary.op.type);
        if (overload != NULL) {
            char *left  = emit_expression(expr->data.binary.left, ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            const char *suffix = operator_overload_suffix(expr->data.binary.op.type);
            char *result = strdup_fmt("operator_%s_%s(%s, %s)",
                suffix, stable_lt, left, right);
            free(left);
            free(right);
            return result;
        }
    }

    char *left  = emit_expression(expr->data.binary.left,  ctx);
    char *right = emit_expression(expr->data.binary.right, ctx);
    const char *op = binary_op_to_c(expr->data.binary.op.type);
    char *result = strdup_fmt("(%s %s %s)", left, op, right);
    free(left);
    free(right);
    return result;
}

char *
emit_unary(ASTNode *expr, TranspilerCtx *ctx)
{
    /* Postfix ? — try/propagate: expr? → early return on error */
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

char *
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = call->data.call.callee;
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        const char *callee_name = callee->data.identifier.name;
        bk = builtin_resolve(callee_name);
        if ((bk == BUILTIN_BOX || bk == BUILTIN_RC_NEW)
            && find_class_decl(ctx, callee_name) != NULL) {
            bk = BUILTIN_NOT_BUILTIN;
        }
    }

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
                state_decl = find_zone_state_decl(zone_decl, state_name);
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
                if (world_has_zone_slot(world_decl, name))
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
                zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_name);
                slot_decl = zone_decl != NULL && slot_name != NULL
                    ? find_zone_domain_slot(zone_decl, slot_name)
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
                zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_name);
                if (zone_decl != NULL && layer_name != NULL
                    && find_zone_layer_slot(zone_decl, layer_name) != NULL) {
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
                zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_name);
                if (zone_decl != NULL && state_name != NULL
                    && find_zone_state_decl(zone_decl, state_name) != NULL) {
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
        /* FileOpen(path, mode) → pgy_file_open(path, mode) */
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
        /* FileRead(fd) → pgy_file_read(fd) */
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
        /* FileWrite(fd, data) → pgy_file_write(fd, data) */
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
        /* FileClose(fd) → pgy_file_close(fd) */
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
        /* ReadFile(path) → pgy_read_file(path) */
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
        /* WriteFile(path, data) → pgy_write_file(path, data) */
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
        /* Input(prompt) → pgy_input(prompt) */
        char *prompt = (call->data.call.arg_count >= 1)
            ? emit_expression(call->data.call.arguments[0], ctx)
            : pergyra_strdup("\"\"");
        char *result = strdup_fmt("pgy_input(%s)", prompt);
        free(prompt);
        return result;
    }
    case BUILTIN_PRINT: {
        /* Print(msg) → pgy_print(msg) */
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
        break;
    }

    /* Tagged union variant constructors: Circle(42) → Shape_Circle(42) */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        ASTNode *class_decl = find_class_decl(ctx, fn);
        if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
            const char *ctor_type = fn;
            size_t argc = call->data.call.arg_count;
            CodeBuf *fields = codebuf_create();
            if (class_has_generic_params(class_decl)) {
                ASTNode synthetic_type = {0};
                synthetic_type.type = AST_TYPE;
                synthetic_type.data.type.name = (char *)fn;
                synthetic_type.data.type.generic_args = NULL;
                {
                    const char *spec_name =
                        ensure_generic_class_specialization(ctx, class_decl, &synthetic_type);
                    if (spec_name != NULL)
                        ctor_type = spec_name;
                }
            }
            for (size_t i = 0; i < argc && i < class_decl->data.class_decl.field_count; i++) {
                ClassField *field = class_decl->data.class_decl.fields[i];
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".%s = %s",
                    field != NULL && field->name != NULL ? field->name : "field",
                    arg != NULL ? arg : "0");
                free(arg);
            }
            char *result;
            if (fields->len > 0)
                result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
            else
                result = strdup_fmt("(%s){0}", ctor_type);
            codebuf_destroy(fields);
            return result;
        }
        {
            ASTNode *party_decl = find_party_decl(ctx, fn);
            if (party_decl != NULL && party_decl->type == AST_PARTY_DECL) {
                size_t argc = call->data.call.arg_count;
                size_t shared_count = party_decl->data.party_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < shared_count; i++) {
                    ASTNode *shared = party_decl->data.party_decl.shared_fields[i];
                    const char *field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < shared_count; i++) {
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (i < argc)
                        continue;
                    shared = party_decl->data.party_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared->data.party_shared.name;
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *roster_decl = find_roster_decl(ctx, fn);
            if (roster_decl != NULL && roster_decl->type == AST_ROSTER_DECL) {
                size_t argc = call->data.call.arg_count;
                size_t exposed = roster_decl->data.roster_decl.party_count
                    + roster_decl->data.roster_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < roster_decl->data.roster_decl.party_count) {
                        ASTNode *slot = roster_decl->data.roster_decl.party_slots[i];
                        field_name = slot != NULL ? slot->data.roster_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = roster_decl->data.roster_decl.shared_fields[
                            i - roster_decl->data.roster_decl.party_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < roster_decl->data.roster_decl.shared_count; i++) {
                    size_t absolute_index = roster_decl->data.roster_decl.party_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = roster_decl->data.roster_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared->data.party_shared.name;
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *relation_decl = find_relation_decl(ctx, fn);
            ASTNode *effect_decl = find_effect_decl(ctx, fn);
            ASTNode *overlay_decl = relation_decl != NULL ? relation_decl : effect_decl;
            if (overlay_decl != NULL) {
                size_t argc = call->data.call.arg_count;
                size_t slot_count = overlay_decl->type == AST_RELATION_DECL
                    ? overlay_decl->data.relation_decl.slot_count
                    : overlay_decl->data.effect_decl.slot_count;
                size_t shared_count = overlay_decl->type == AST_RELATION_DECL
                    ? overlay_decl->data.relation_decl.shared_count
                    : overlay_decl->data.effect_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < slot_count) {
                        ASTNode *slot = overlay_decl->type == AST_RELATION_DECL
                            ? overlay_decl->data.relation_decl.slots[i]
                            : overlay_decl->data.effect_decl.slots[i];
                        field_name = slot != NULL ? slot->data.domain_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = overlay_decl->type == AST_RELATION_DECL
                            ? overlay_decl->data.relation_decl.shared_fields[i - slot_count]
                            : overlay_decl->data.effect_decl.shared_fields[i - slot_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < shared_count; i++) {
                    size_t absolute_index = slot_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = overlay_decl->type == AST_RELATION_DECL
                        ? overlay_decl->data.relation_decl.shared_fields[i]
                        : overlay_decl->data.effect_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared->data.party_shared.name;
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                for (size_t i = 0; i < slot_count; i++) {
                    ASTNode *slot = overlay_decl->type == AST_RELATION_DECL
                        ? overlay_decl->data.relation_decl.slots[i]
                        : overlay_decl->data.effect_decl.slots[i];
                    const char *slot_name = slot != NULL
                        ? slot->data.domain_slot.slot_name
                        : NULL;
                    bool projection_slot = slot != NULL
                        && (slot->data.domain_slot.is_tobject
                            || domain_slot_is_projection_target_local(
                                slot,
                                overlay_decl->type == AST_RELATION_DECL
                                    ? overlay_decl->data.relation_decl.refreshes
                                    : overlay_decl->data.effect_decl.refreshes,
                                overlay_decl->type == AST_RELATION_DECL
                                    ? overlay_decl->data.relation_decl.refresh_count
                                    : overlay_decl->data.effect_decl.refresh_count));
                    if (!projection_slot || slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *zone_decl = find_zone_decl(ctx, fn);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL) {
                size_t argc = call->data.call.arg_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc
                        && i < zone_decl->data.zone_decl.slot_count
                               + zone_decl->data.zone_decl.shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < zone_decl->data.zone_decl.slot_count) {
                        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                        field_name = slot != NULL ? slot->data.domain_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = zone_decl->data.zone_decl.shared_fields[
                            i - zone_decl->data.zone_decl.slot_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < zone_decl->data.zone_decl.shared_count; i++) {
                    size_t absolute_index = zone_decl->data.zone_decl.slot_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = zone_decl->data.zone_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
                    ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                    const char *slot_name = slot != NULL
                        ? slot->data.domain_slot.slot_name
                        : NULL;
                    bool projection_slot = slot != NULL
                        && (slot->data.domain_slot.is_tobject
                            || domain_slot_is_projection_target_local(
                                slot,
                                zone_decl->data.zone_decl.refreshes,
                                zone_decl->data.zone_decl.refresh_count));
                    if (!projection_slot || slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *world_decl = find_world_decl(ctx, fn);
            if (world_decl != NULL && world_decl->type == AST_WORLD_DECL) {
                size_t argc = call->data.call.arg_count;
                CodeBuf *fields = codebuf_create();
                size_t exposed = world_decl->data.world_decl.roster_count
                    + world_decl->data.world_decl.zone_count
                    + world_decl->data.world_decl.shared_count;
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < world_decl->data.world_decl.roster_count) {
                        ASTNode *slot = world_decl->data.world_decl.rosters[i];
                        field_name = slot != NULL ? slot->data.world_roster.slot_name : "field";
                    } else if (i < world_decl->data.world_decl.roster_count
                                   + world_decl->data.world_decl.zone_count) {
                        ASTNode *slot = world_decl->data.world_decl.zones[
                            i - world_decl->data.world_decl.roster_count];
                        field_name = slot != NULL ? slot->data.world_zone.slot_name : "field";
                    } else {
                        ASTNode *shared = world_decl->data.world_decl.shared_fields[
                            i - world_decl->data.world_decl.roster_count
                              - world_decl->data.world_decl.zone_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < world_decl->data.world_decl.shared_count; i++) {
                    size_t absolute_index = world_decl->data.world_decl.roster_count
                        + world_decl->data.world_decl.zone_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = world_decl->data.world_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
                    ASTNode *zone = world_decl->data.world_decl.zones[i];
                    const char *slot_name = zone != NULL
                        ? zone->data.world_zone.slot_name
                        : NULL;
                    if (slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__zone_dirty_%s = true", slot_name);
                }
                if (fields->len > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".__world_derived_dirty = true");
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }

        const char *qualified = lookup_enum_variant_qualified_name(ctx, fn);
        if (qualified != NULL) {
            /* Emit: EnumName_VariantName(args...) */
            size_t argc = call->data.call.arg_count;
            char **arg_strs = calloc(argc > 0 ? argc : 1, sizeof(char *));
            for (size_t i = 0; i < argc; i++)
                arg_strs[i] = emit_expression(call->data.call.arguments[i], ctx);
            /* Build argument list string */
            size_t buf_len = strlen(qualified) + 3;
            for (size_t i = 0; i < argc; i++) {
                if (arg_strs[i] == NULL) {
                    for (size_t j = 0; j < i; j++)
                        free(arg_strs[j]);
                    free(arg_strs);
                    return NULL;
                }
                buf_len += strlen(arg_strs[i]) + 2;
            }
            char *result = malloc(buf_len);
            if (result == NULL) {
                for (size_t i = 0; i < argc; i++)
                    free(arg_strs[i]);
                free(arg_strs);
                return NULL;
            }
            {
                size_t offset = 0;
                size_t qual_len = strlen(qualified);
                memcpy(result + offset, qualified, qual_len);
                offset += qual_len;
                result[offset++] = '(';
                for (size_t i = 0; i < argc; i++) {
                    if (i > 0) {
                        result[offset++] = ',';
                        result[offset++] = ' ';
                    }
                    {
                        size_t arg_len = strlen(arg_strs[i]);
                        memcpy(result + offset, arg_strs[i], arg_len);
                        offset += arg_len;
                    }
                    free(arg_strs[i]);
                }
                result[offset++] = ')';
                result[offset] = '\0';
            }
            free(arg_strs);
            return result;
        }
    }

    /* Result<T, E> built-in functions:
     * - Ok/Err require explicit Result context from the surrounding type.
     * - IsOk/IsErr/Unwrap/UnwrapOr may also derive suffix from their Result
     *   operand when the surrounding expression context is not specific. */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        bool is_result_ctor = false;
        bool is_result_consumer = false;
        char result_suffix[128] = {0};
        bool have_result_suffix = transpiler_result_suffix_from_context(
            ctx, result_suffix, sizeof(result_suffix));
        is_result_ctor = strcmp(fn, "Ok") == 0 || strcmp(fn, "Err") == 0;
        is_result_consumer = strcmp(fn, "IsOk") == 0
            || strcmp(fn, "IsErr") == 0
            || strcmp(fn, "Unwrap") == 0
            || strcmp(fn, "UnwrapOr") == 0;

        if (!have_result_suffix && is_result_consumer
            && call->data.call.arg_count >= 1
            && call->data.call.arguments[0] != NULL) {
            const char *arg_type = infer_expression_type_name(
                ctx, call->data.call.arguments[0]);
            have_result_suffix = transpiler_result_suffix_from_type_name(
                arg_type, result_suffix, sizeof(result_suffix));
        }

        if ((is_result_ctor || is_result_consumer) && !have_result_suffix) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot derive Result<T, E> specialization for %s(); add explicit Result<T, E> type context",
                fn);
            return pergyra_strdup("0");
        }

        if (strcmp(fn, "Ok") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Ok_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Err") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Err_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsOk") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("IsOk_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsErr") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("IsErr_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Unwrap") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Unwrap_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOr") == 0 && call->data.call.arg_count == 2) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *fallback = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("UnwrapOr_%s(%s, %s)", result_suffix, arg, fallback);
            free(arg);
            free(fallback);
            return result;
        }
        if (strcmp(fn, "Some") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char *result = strdup_fmt("Some_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "None") == 0 && call->data.call.arg_count == 0) {
            /* Use expected target type from enclosing let declaration if available */
            const char *inner = "Int";
            if (ctx->expected_type != NULL && strncmp(ctx->expected_type, "Option<", 7) == 0) {
                inner = slot_inner_type_name(ctx->expected_type);
            }
            return strdup_fmt("None_%s()", inner);
        }
        if (strcmp(fn, "IsSome") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("IsSome_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsNone") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("IsNone_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOption") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("UnwrapOption_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
    }

    /* Standard library built-in functions */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;

        /* Math functions */
        if (strcmp(fn, "Abs") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Min") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Max") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        /* String functions */
        if (strcmp(fn, "StringLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Contains") == 0 || strcmp(fn, "StringContains") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringContains(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Replace") == 0 || strcmp(fn, "StringReplace") == 0)
            && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *old_s = emit_expression(call->data.call.arguments[1], ctx);
            char *new_s = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
            free(s); free(old_s); free(new_s);
            return result;
        }
        if (strcmp(fn, "Substring") == 0 && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *start = emit_expression(call->data.call.arguments[1], ctx);
            char *len = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
            free(s); free(start); free(len);
            return result;
        }
        if ((strcmp(fn, "Trim") == 0 || strcmp(fn, "StringTrim") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("StringTrim(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Upper") == 0 || strcmp(fn, "ToUpper") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToUpper(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Lower") == 0 || strcmp(fn, "ToLower") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToLower(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Concat") == 0 || strcmp(fn, "StringConcat") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        /* StringSplit / StringJoin / ToInt / ToFloat / Math */
        if ((strcmp(fn, "StringSplit") == 0 || strcmp(fn, "Split") == 0)
            && call->data.call.arg_count == 2) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
            free(s); free(d);
            return result;
        }
        if ((strcmp(fn, "StringJoin") == 0 || strcmp(fn, "Join") == 0)
            && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *sep = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
            free(arr); free(sep);
            return result;
        }
        if (strcmp(fn, "ToInt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToInt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ToFloat") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToFloat(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Sqrt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Sqrt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Pow") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Pow(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Floor") == 0 || strcmp(fn, "Ceil") == 0
            || strcmp(fn, "Round") == 0
            || strcmp(fn, "Sin") == 0 || strcmp(fn, "Cos") == 0
            || strcmp(fn, "Tan") == 0 || strcmp(fn, "Asin") == 0
            || strcmp(fn, "Acos") == 0 || strcmp(fn, "Atan") == 0
            || strcmp(fn, "Exp") == 0 || strcmp(fn, "MathLog") == 0
            || strcmp(fn, "Log10") == 0 || strcmp(fn, "Log2") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("%s(%s)", fn, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Atan2") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Atan2(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Clamp") == 0 && call->data.call.arg_count == 3) {
            char *val = emit_expression(call->data.call.arguments[0], ctx);
            char *lo = emit_expression(call->data.call.arguments[1], ctx);
            char *hi = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Clamp(%s, %s, %s)", val, lo, hi);
            free(val); free(lo); free(hi);
            return result;
        }
        if (strcmp(fn, "PI") == 0) return pergyra_strdup("PGY_PI");
        if (strcmp(fn, "E") == 0 && call->data.call.arg_count == 0)
            return pergyra_strdup("PGY_E");
        if (strcmp(fn, "Random") == 0) {
            if (call->data.call.arg_count >= 1) {
                char *arg = emit_expression(call->data.call.arguments[0], ctx);
                char *result = strdup_fmt("Random(%s)", arg);
                free(arg);
                return result;
            }
            return pergyra_strdup("Random(100)");
        }
        if (strcmp(fn, "SeedRandom") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("SeedRandom(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)(%s.length))", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayPush") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *suffix = infer_expression_type_name(
                ctx, call->data.call.arguments[1]);
            char *result = strdup_fmt(
                "pgy_array_push_%s(&%s, %s)", suffix, arr, val);
            free(arr); free(val);
            return result;
        }
        if (strcmp(fn, "ArraySet") == 0 && call->data.call.arg_count == 3) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *idx = emit_expression(call->data.call.arguments[1], ctx);
            char *val = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt(
                "((%s).data[%s] = %s)", arr, idx, val);
            free(arr); free(idx); free(val);
            return result;
        }
        if (strcmp(fn, "ArrayPop") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s).length > 0 ? (%s).length-- : 0)",
                arr, arr);
            free(arr);
            return result;
        }
        /* ArraySort — hybrid sort using AlphaDev kernels for small arrays */
        if (strcmp(fn, "ArraySort") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            char *result = strdup_fmt(
                "({ pgy_array_sort_%s((%s).data, (%s).length); %s; })",
                inner, arr, arr, arr);
            free(arr);
            return result;
        }
        /* ArrayMap — apply function to each element, return new array */
        if (strcmp(fn, "ArrayMap") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_map_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _mi = 0; _mi < (%s).length; _mi++) "
                "pgy_array_push_%s(&_pgy_map_%d, %s((%s).data[_mi])); "
                "_pgy_map_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                inner, tmp_id, fn_arg, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        /* ArrayFilter — keep elements where predicate returns true */
        if (strcmp(fn, "ArrayFilter") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_filt_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _fi = 0; _fi < (%s).length; _fi++) "
                "if (%s((%s).data[_fi])) "
                "pgy_array_push_%s(&_pgy_filt_%d, (%s).data[_fi]); "
                "_pgy_filt_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                fn_arg, arr,
                inner, tmp_id, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        /* ArrayReverse — in-place reverse */
        if (strcmp(fn, "ArrayReverse") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = NULL;
            const char *c_type = NULL;
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0) {
                inner = slot_inner_type_name(arr_type);
                c_type = pergyra_type_to_c(inner);
            }
            if (inner == NULL || c_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine element type for ArrayReverse; explicit concrete Array<T> input is required");
                free(arr);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "({ for (size_t _ri = 0; _ri < (%s).length / 2; _ri++) { "
                "%s _tmp = (%s).data[_ri]; "
                "(%s).data[_ri] = (%s).data[(%s).length - 1 - _ri]; "
                "(%s).data[(%s).length - 1 - _ri] = _tmp; } %s; })",
                arr, c_type, arr, arr, arr, arr, arr, arr, arr);
            free(arr);
            return result;
        }
        /* Channel builtins */
        if (strcmp(fn, "TryRecv") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_try_recv_%s(&%s, &_pgy_recv_tmp) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, inner, inner);
            free(ch);
            return result;
        }
        if (strcmp(fn, "RecvTimeout") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *timeout = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_recv_timeout_%s(&%s, &_pgy_recv_tmp, (uint64_t)(%s)) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, timeout, inner, inner);
            free(ch);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "TrySend") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "TrySendStatus") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_status_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "SendTimeout") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "SendTimeoutStatus") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_status_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "Cancel") == 0 && call->data.call.arg_count == 1) {
            char *task = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_task_cancel(%s)", task);
            free(task);
            return result;
        }
        if (strcmp(fn, "IsCancelled") == 0 && call->data.call.arg_count == 0) {
            return strdup_fmt("pgy_task_is_cancelled()");
        }
        if (strcmp(fn, "ChannelClose") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_close_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelReady") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_ready_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelLength") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_length_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelCapacity") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_capacity_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelSpace") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_space_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelFull") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_full_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelClosed") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_closed_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        /* Clone: explicit copy of Slot */
        if (strcmp(fn, "Clone") == 0 && call->data.call.arg_count == 1) {
            char *src = emit_expression(call->data.call.arguments[0], ctx);
            const char *tn = infer_expression_type_name(
                ctx, call->data.call.arguments[0]);
            if (tn != NULL && strncmp(tn, "Slot<", 5) == 0) {
                const char *inner = slot_inner_type_name(tn);
                char *result = strdup_fmt(
                    "({ PgySlot_%s _c = pgy_claim_%s(); "
                    "pgy_write_%s(&_c, pgy_read_%s(&%s)); _c; })",
                    inner, inner, inner, inner, src);
                free(src);
                return result;
            }
            return src;
        }
        /* Print (no newline) vs Log (with newline) */
        if (strcmp(fn, "Print") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("printf(\"%%s\", %s)", arg);
            free(arg);
            return result;
        }
        /* ToString */
        if (strcmp(fn, "ToString") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *arg_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = NULL;
            if (arg_type != NULL && strcmp(arg_type, "String") == 0) {
                result = pergyra_strdup(arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Bool") == 0) {
                result = strdup_fmt("pgy_bool_to_string(%s)", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Float") == 0) {
                result = strdup_fmt("pgy_float_to_string(%s)", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Double") == 0) {
                result = strdup_fmt("pgy_float_to_string((float)(%s))", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Long") == 0) {
                result = strdup_fmt("pgy_int_to_string((int32_t)(%s))", arg);
            } else {
                result = strdup_fmt("pgy_int_to_string(%s)", arg);
            }
            free(arg);
            return result;
        }
        /* HashMap builtins */
        if (strcmp(fn, "MapNew") == 0) {
            const char *hint = ctx->active_type_hint;
            const char *value = "Int";
            if (hint != NULL && strncmp(hint, "HashMap<", 8) == 0) {
                const char *hint_value = constructed_arg_name_at(hint, 1);
                if (hint_value != NULL)
                    value = hint_value;
            }
            ensure_collection_specialization(ctx, "Map", value);
            return strdup_fmt("pgy_map_new_%s()", collection_runtime_suffix(value));
        }
        if (strcmp(fn, "MapSet") == 0 && call->data.call.arg_count == 3) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            char *v = emit_expression(call->data.call.arguments[2], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_set_i32_%s(&%s, %s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_set_i64_%s(&%s, %s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_set_bool_%s(&%s, %s, %s)"
                : "pgy_map_set_%s(&%s, %s, %s)",
                collection_runtime_suffix(value), m, k, v);
            free(m); free(k); free(v);
            return result;
        }
        if (strcmp(fn, "MapGet") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_get_i32_%s(&%s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_get_i64_%s(&%s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_get_bool_%s(&%s, %s)"
                : "pgy_map_get_%s(&%s, %s)",
                collection_runtime_suffix(value), m, k);
            free(m); free(k);
            return result;
        }
        if (strcmp(fn, "MapHas") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_has_i32_%s(&%s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_has_i64_%s(&%s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_has_bool_%s(&%s, %s)"
                : "pgy_map_has_%s(&%s, %s)",
                collection_runtime_suffix(value), m, k);
            free(m); free(k);
            return result;
        }
        if (strcmp(fn, "MapRemove") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_remove_i32_%s(&%s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_remove_i64_%s(&%s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_remove_bool_%s(&%s, %s)"
                : "pgy_map_remove_%s(&%s, %s)",
                collection_runtime_suffix(value), m, k);
            free(m); free(k);
            return result;
        }
        if (strcmp(fn, "MapSize") == 0 && call->data.call.arg_count == 1) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *value = (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0)
                ? constructed_arg_name_at(map_type, 1)
                : "Int";
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt("pgy_map_size_%s(&%s)",
                collection_runtime_suffix(value), m);
            free(m);
            return result;
        }
        if (strcmp(fn, "MapKeys") == 0 && call->data.call.arg_count == 1) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_keys_i32_%s(&%s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_keys_i64_%s(&%s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_keys_bool_%s(&%s)"
                : "pgy_map_keys_%s(&%s)",
                collection_runtime_suffix(value), m);
            free(m);
            return result;
        }
        /* List builtins */
        if (strcmp(fn, "ListNew") == 0)
        {
            const char *hint = ctx->active_type_hint;
            const char *inner = (hint != NULL && strncmp(hint, "List<", 5) == 0)
                ? slot_inner_type_name(hint)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            return strdup_fmt("pgy_list_new_%s()", collection_runtime_suffix(inner));
        }
        if (strcmp(fn, "ListPush") == 0 && call->data.call.arg_count == 2) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *v = emit_expression(call->data.call.arguments[1], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_push_%s(&%s, %s)",
                collection_runtime_suffix(inner), l, v);
            free(l); free(v); return r;
        }
        if (strcmp(fn, "ListGet") == 0 && call->data.call.arg_count == 2) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_get_%s(&%s, %s)",
                collection_runtime_suffix(inner), l, i);
            free(l); free(i); return r;
        }
        if (strcmp(fn, "ListSet") == 0 && call->data.call.arg_count == 3) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            char *v = emit_expression(call->data.call.arguments[2], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_set_%s(&%s, %s, %s)",
                collection_runtime_suffix(inner), l, i, v);
            free(l); free(i); free(v); return r;
        }
        if (strcmp(fn, "ListSize") == 0 && call->data.call.arg_count == 1) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_size_%s(&%s)",
                collection_runtime_suffix(inner), l);
            free(l); return r;
        }
        if (strcmp(fn, "ListRemove") == 0 && call->data.call.arg_count == 2) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_remove_%s(&%s, %s)",
                collection_runtime_suffix(inner), l, i);
            free(l); free(i); return r;
        }
        /* Set builtins — generic (type inferred from Set<T> annotation) */
        #define SET_INFER_INNER(arg0) do { \
            const char *_st = infer_expression_type_name(ctx, arg0); \
            if (_st != NULL && strncmp(_st, "Set<", 4) == 0) \
                set_inner = slot_inner_type_name(_st); \
        } while (0)

        if (strcmp(fn, "SetNew") == 0) {
            const char *hint = ctx->active_type_hint;
            const char *set_inner = (hint != NULL && strncmp(hint, "Set<", 4) == 0)
                ? slot_inner_type_name(hint)
                : "String";
            ensure_collection_specialization(ctx, "Set", set_inner);
            return strdup_fmt("pgy_set_new_%s()", collection_runtime_suffix(set_inner));
        }
        if (strcmp(fn, "SetAdd") == 0 && call->data.call.arg_count == 2) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_add_%s(&%s, %s)",
                collection_runtime_suffix(set_inner), s, k);
            free(s); free(k); return r;
        }
        if (strcmp(fn, "SetHas") == 0 && call->data.call.arg_count == 2) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_has_%s(&%s, %s)",
                collection_runtime_suffix(set_inner), s, k);
            free(s); free(k); return r;
        }
        if (strcmp(fn, "SetRemove") == 0 && call->data.call.arg_count == 2) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_remove_%s(&%s, %s)",
                collection_runtime_suffix(set_inner), s, k);
            free(s); free(k); return r;
        }
        if (strcmp(fn, "SetSize") == 0 && call->data.call.arg_count == 1) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_size_%s(&%s)",
                collection_runtime_suffix(set_inner), s);
            free(s); return r;
        }
        #undef SET_INFER_INNER
        /* Queue builtins */
        if (strcmp(fn, "QueueNew") == 0)
        {
            const char *hint = ctx->active_type_hint;
            const char *inner = (hint != NULL && strncmp(hint, "Queue<", 6) == 0)
                ? slot_inner_type_name(hint)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            return strdup_fmt("pgy_queue_new_%s()", collection_runtime_suffix(inner));
        }
        if (strcmp(fn, "QueuePush") == 0 && call->data.call.arg_count == 2) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            char *v = emit_expression(call->data.call.arguments[1], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_push_%s(&%s, %s)",
                collection_runtime_suffix(inner), q, v);
            free(q); free(v); return r;
        }
        if (strcmp(fn, "QueuePop") == 0 && call->data.call.arg_count == 1) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_pop_%s(&%s)",
                collection_runtime_suffix(inner), q);
            free(q); return r;
        }
        if (strcmp(fn, "QueueSize") == 0 && call->data.call.arg_count == 1) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_size_%s(&%s)",
                collection_runtime_suffix(inner), q);
            free(q); return r;
        }
        if (strcmp(fn, "QueueEmpty") == 0 && call->data.call.arg_count == 1) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_empty_%s(&%s)",
                collection_runtime_suffix(inner), q);
            free(q); return r;
        }
        /* FSM builtins */
        if (strcmp(fn, "FsmNew") == 0)
            return pergyra_strdup("pgy_fsm_new()");
        if (strcmp(fn, "FsmAddState") == 0 && call->data.call.arg_count == 2) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *n = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_fsm_add_state(&%s, %s)", f, n);
            free(f); free(n); return r;
        }
        if (strcmp(fn, "FsmTransition") == 0 && call->data.call.arg_count == 4) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *from = emit_expression(call->data.call.arguments[1], ctx);
            char *inp = emit_expression(call->data.call.arguments[2], ctx);
            char *to = emit_expression(call->data.call.arguments[3], ctx);
            char *r = strdup_fmt("pgy_fsm_add_transition(&%s, %s, %s, %s)", f, from, inp, to);
            free(f); free(from); free(inp); free(to); return r;
        }
        if (strcmp(fn, "FsmStep") == 0 && call->data.call.arg_count == 2) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_fsm_step(&%s, %s)", f, i);
            free(f); free(i); return r;
        }
        if (strcmp(fn, "FsmCurrent") == 0 && call->data.call.arg_count == 1) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_fsm_current(&%s)", f);
            free(f); return r;
        }
        if (strcmp(fn, "FsmCurrentName") == 0 && call->data.call.arg_count == 1) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_fsm_current_name(&%s)", f);
            free(f); return r;
        }
        /* Timer builtins */
        if (strcmp(fn, "TimerNew") == 0 && call->data.call.arg_count == 1) {
            char *d = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_new(%s)", d);
            free(d); return r;
        }
        if (strcmp(fn, "TimerTick") == 0 && call->data.call.arg_count == 2) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_timer_tick(&%s, %s)", t, d);
            free(t); free(d); return r;
        }
        if (strcmp(fn, "TimerRemaining") == 0 && call->data.call.arg_count == 1) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_remaining(&%s)", t);
            free(t); return r;
        }
        if (strcmp(fn, "TimerDone") == 0 && call->data.call.arg_count == 1) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_done(&%s)", t);
            free(t); return r;
        }
        if (strcmp(fn, "TimerReset") == 0 && call->data.call.arg_count == 1) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_reset(&%s)", t);
            free(t); return r;
        }
        if (strcmp(fn, "CooldownNew") == 0 && call->data.call.arg_count == 1) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_cooldown_new(%s)", c);
            free(c); return r;
        }
        if (strcmp(fn, "CooldownTick") == 0 && call->data.call.arg_count == 2) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_cooldown_tick(&%s, %s)", c, d);
            free(c); free(d); return r;
        }
        if (strcmp(fn, "CooldownReady") == 0 && call->data.call.arg_count == 1) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_cooldown_ready(&%s)", c);
            free(c); return r;
        }
        if (strcmp(fn, "CooldownTrigger") == 0 && call->data.call.arg_count == 1) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_cooldown_trigger(&%s)", c);
            free(c); return r;
        }
        if (strcmp(fn, "MapSetStr") == 0 && call->data.call.arg_count == 3) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            char *v = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("pgy_map_set_string(&%s, %s, %s)", m, k, v);
            free(m); free(k); free(v);
            return result;
        }
        if (strcmp(fn, "MapGetStr") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("pgy_map_get_string(&%s, %s)", m, k);
            free(m); free(k);
            return result;
        }
    }

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        if (find_event_decl(ctx, name) != NULL) {
            CodeBuf *args_buf = codebuf_create();
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                codebuf_write(args_buf, "%s", arg);
                free(arg);
            }

            char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                      name, name,
                                      args_buf->len > 0 ? ", " : "",
                                      args_buf->data);
            codebuf_destroy(args_buf);
            return result;
        }
    }

    /* Method-call style slot operations: slot.Write(val), slot.Read(), slot.Release() */
    if (callee->type == AST_MEMBER_ACCESS) {
        const char *method = callee->data.member.name;
        ASTNode *obj = callee->data.member.object;

        if (obj != NULL && obj->type == AST_MEMBER_ACCESS && method != NULL) {
            ASTNode *party_node = obj->data.member.object;
            const char *slot_name = obj->data.member.name;

            if (party_node != NULL && party_node->type == AST_IDENTIFIER
                && slot_name != NULL) {
                const char *party_var = party_node->data.identifier.name;
                const char *party_type = lookup_typed_var(ctx, party_var);
                ASTNode *party_decl = find_party_decl(ctx, party_type);
                char *ability_name = NULL;

                if (party_decl != NULL) {
                    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
                        ASTNode *rs = party_decl->data.party_decl.role_slots[i];
                        if (rs == NULL || strcmp(rs->data.role_slot.slot_name, slot_name) != 0)
                            continue;
                        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
                            ASTNode *ab = rs->data.role_slot.required_abilities[j];
                            ASTNode *ability_decl;
                            bool has_method = false;
                            char *ability_tag = NULL;
                            if (ab == NULL || ab->data.type.name == NULL)
                                continue;
                            ability_decl = find_ability_decl(ctx, ab->data.type.name);
                            if (ability_decl != NULL) {
                                for (size_t mi = 0; mi < ability_decl->data.ability_decl.method_count; mi++) {
                                    ASTNode *m = ability_decl->data.ability_decl.methods[mi];
                                    if (m != NULL && m->type == AST_FUNC_DECL
                                        && m->data.func_decl.name != NULL
                                        && strcmp(m->data.func_decl.name, method) == 0) {
                                        has_method = true;
                                        break;
                                    }
                                }
                            }
                            if (has_method || ability_name == NULL) {
                                free(ability_name);
                                ability_tag = render_ability_ref_vtable_tag(ab);
                                ability_name = ability_tag;
                            } else {
                                free(ability_tag);
                            }
                            if (has_method)
                                break;
                        }
                        break;
                    }
                }

                if (ability_name != NULL) {
                    CodeBuf *args_buf = codebuf_create();
                    codebuf_write(args_buf, "%s.%s", party_var, slot_name);
                    for (size_t i = 0; i < call->data.call.arg_count; i++) {
                        char *arg = emit_expression(call->data.call.arguments[i], ctx);
                        codebuf_write(args_buf, ", %s", arg);
                        free(arg);
                    }

                    char *result = strdup_fmt("%s.%s_%s_vt->%s(%s)",
                                              party_var, slot_name, ability_name,
                                              method, args_buf->data);
                    codebuf_destroy(args_buf);
                    free(ability_name);
                    return result;
                }
            }
        }

        bool is_slot_method = (strcmp(method, "Write") == 0
                            || strcmp(method, "Read") == 0
                            || strcmp(method, "Release") == 0);

        if (obj != NULL && method != NULL) {
            const char *type_name = resolve_nominal_host_expr_type_name(ctx, obj);
            if (type_name != NULL && is_nominal_host_type_name(ctx, type_name)) {
                CodeBuf *args_buf = codebuf_create();
                const char *owned_type_name = type_name;
                bool use_self_cell = is_pointer_self_host_type_name(ctx, owned_type_name);
                bool is_self_ident = (obj->type == AST_IDENTIFIER
                    && obj->data.identifier.name != NULL
                    && strcmp(obj->data.identifier.name, "self") == 0);
                ASTNode *method_decl = find_nominal_host_method_decl(ctx, owned_type_name, method);

                if (is_self_ident && use_self_cell) {
                    codebuf_write(args_buf, "self");
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    /* Check if receiver is already a pointer (subject-ref param) */
                    bool already_pointer = false;
                    if (obj->type == AST_IDENTIFIER) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx,
                            obj->data.identifier.name);
                        if (entry != NULL && entry->is_subject_ref)
                            already_pointer = true;
                    }
                    if (use_self_cell && !already_pointer)
                        codebuf_write(args_buf, "&%s", obj_expr);
                    else
                        codebuf_write(args_buf, "%s", obj_expr);
                    free(obj_expr);
                }

                for (size_t i = 0; i < call->data.call.arg_count; i++) {
                    ASTNode *arg_node = call->data.call.arguments[i];
                    char *arg = emit_expression(arg_node, ctx);
                    bool pass_by_ptr = false;
                    if (method_decl != NULL) {
                        size_t param_index = i;
                        if (method_decl->data.func_decl.param_count > 0) {
                            FuncParam *first = method_decl->data.func_decl.params[0];
                            if (first != NULL && first->name != NULL
                                && strcmp(first->name, "self") == 0)
                                param_index++;
                        }
                        if (param_index < method_decl->data.func_decl.param_count) {
                            FuncParam *param = method_decl->data.func_decl.params[param_index];
                            char *ptn = (param != NULL && param->type != NULL)
                                ? render_type_name(param->type)
                                : NULL;
                            if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                                pass_by_ptr = true;
                            free(ptn);
                        }
                    }
                    if (pass_by_ptr) {
                        bool already_pointer = false;
                        bool is_self_arg = (arg_node != NULL
                            && arg_node->type == AST_IDENTIFIER
                            && arg_node->data.identifier.name != NULL
                            && strcmp(arg_node->data.identifier.name, "self") == 0);
                        if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                            TypedVarEntry *entry = lookup_typed_entry(ctx,
                                arg_node->data.identifier.name);
                            if (entry != NULL && entry->is_subject_ref)
                                already_pointer = true;
                        }
                        if (is_self_arg && current_class_uses_self_cell(ctx))
                            codebuf_write(args_buf, ", self");
                        else if (already_pointer)
                            codebuf_write(args_buf, ", %s", arg);
                        else
                            codebuf_write(args_buf, ", &%s", arg);
                    } else {
                        codebuf_write(args_buf, ", %s", arg);
                    }
                    free(arg);
                }

                {
                    char *result = strdup_fmt("%s_%s(%s)",
                        owned_type_name, method, args_buf->data);
                    const char *source_slot_name =
                        assignment_target_root_slot_name(obj);
                    char *invalidation = NULL;
                    ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
                    const char *saved_receiver_expr = ctx->current_overlay_receiver_expr;

                    if (saved_host_decl != NULL && saved_host_decl->type == AST_WORLD_DECL) {
                        const char *zone_slot_name = NULL;
                        const char *zone_type_name = NULL;
                        const char *zone_subject_slot_name = NULL;
                        const char *zone_subject_type_name = NULL;

                        if (resolve_world_zone_subject_receiver(ctx, obj,
                                &zone_slot_name, &zone_type_name,
                                &zone_subject_slot_name, &zone_subject_type_name)
                            && zone_slot_name != NULL
                            && zone_type_name != NULL
                            && zone_subject_slot_name != NULL
                            && zone_subject_type_name != NULL
                            && strcmp(zone_subject_type_name, owned_type_name) == 0) {
                            ASTNode *zone_decl = find_zone_decl(ctx, zone_type_name);
                            if (zone_decl != NULL)
                                transpiler_bind_current_host_decl_local(ctx, zone_decl);
                            ctx->current_overlay_receiver_expr =
                                strdup_fmt("(&self->%s)", zone_slot_name);
                            source_slot_name = zone_subject_slot_name;
                        }
                    }

                    invalidation =
                        emit_current_overlay_method_projection_invalidation(
                            ctx, source_slot_name, owned_type_name, method_decl);
                    if (ctx->current_overlay_receiver_expr != NULL
                        && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                        free((char *)ctx->current_overlay_receiver_expr);
                    }
                    ctx->current_overlay_receiver_expr = saved_receiver_expr;
                    transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                    codebuf_destroy(args_buf);
                    if (invalidation != NULL) {
                        char *ret_type_name = NULL;
                        char *wrapped = NULL;

                        if (method_decl->data.func_decl.return_type != NULL)
                            ret_type_name = render_type_name(
                                method_decl->data.func_decl.return_type);

                        if (ret_type_name != NULL
                            && strcmp(ret_type_name, "Void") != 0) {
                            int tmp_id = ++ctx->tmp_counter;
                            wrapped = strdup_fmt(
                                "({ %s _pgy_call_%d = %s; %s_pgy_call_%d; })",
                                pergyra_type_to_c(ret_type_name), tmp_id,
                                result, invalidation, tmp_id);
                        } else {
                            wrapped = strdup_fmt("({ %s; %s})",
                                result, invalidation);
                        }

                        free(ret_type_name);
                        free(invalidation);
                        free(result);
                        return wrapped;
                    }
                    return result;
                }
            }
        }

        if (is_slot_method && obj->type == AST_IDENTIFIER) {
            const char *inner = lookup_slot_type(ctx, obj->data.identifier.name);
            bool is_secure = lookup_slot_is_secure(ctx, obj->data.identifier.name);
            bool saved_suppress = ctx->suppress_slot_auto_read;
            ctx->suppress_slot_auto_read = true;
            char *obj_expr = emit_expression(obj, ctx);
            ctx->suppress_slot_auto_read = saved_suppress;
            if (inner == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for method call on '%s'",
                    obj->data.identifier.name != NULL
                        ? obj->data.identifier.name
                        : "<slot>");
                free(obj_expr);
                return pergyra_strdup("0");
            }
            char *slot_ref = slot_ref_expr(ctx, obj->data.identifier.name, obj_expr);

            if (strcmp(method, "Write") == 0 && call->data.call.arg_count >= 1) {
                char *val_expr = emit_expression(call->data.call.arguments[0], ctx);
                char *result;
                if (is_secure && call->data.call.arg_count >= 2) {
                    char *tok = emit_expression(call->data.call.arguments[1], ctx);
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, tok);
                    free(tok);
                } else if (is_secure) {
                    const char *token_name =
                        lookup_slot_token_name(ctx, obj->data.identifier.name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", obj->data.identifier.name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, token_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                                        inner, slot_ref, val_expr);
                }
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Read") == 0) {
                char *result;
                if (is_secure) {
                    const char *token_name =
                        lookup_slot_token_name(ctx, obj->data.identifier.name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", obj->data.identifier.name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_read_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Release") == 0) {
                char *result;
                if (is_secure) {
                    const char *token_name =
                        lookup_slot_token_name(ctx, obj->data.identifier.name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", obj->data.identifier.name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_release_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
                }
                /* Mark as released */
                for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                    if (strcmp(ctx->slot_vars[ri].name,
                              obj->data.identifier.name) == 0) {
                        ctx->slot_vars[ri].released = true;
                        break;
                    }
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            }
            free(slot_ref);
            free(obj_expr);
        }

        {
            const char *receiver_type = infer_expression_type_name(ctx, obj);
            if (method != NULL
                && strcmp(method, "Slice") == 0
                && receiver_type != NULL
                && (strncmp(receiver_type, "Array<", 6) == 0
                    || strncmp(receiver_type, "Slice<", 6) == 0)
                && call->data.call.arg_count == 2) {
                const char *inner = slot_inner_type_name(receiver_type);
                char *start_expr = emit_expression(call->data.call.arguments[0], ctx);
                char *len_expr = emit_expression(call->data.call.arguments[1], ctx);
                char *result = NULL;
                int tmp_id = ++ctx->tmp_counter;

                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slice element type for receiver '%s'",
                        receiver_type);
                    free(start_expr);
                    free(len_expr);
                    return pergyra_strdup("0");
                }

                if (strncmp(receiver_type, "Array<", 6) == 0) {
                    char *obj_expr = emit_expression(obj, ctx);
                    result = strdup_fmt(
                        "({ PgyArray_%s _pgy_arr_%d = %s; pgy_array_slice_%s(&_pgy_arr_%d, (size_t)(%s), (size_t)(%s)); })",
                        inner, tmp_id, obj_expr, inner, tmp_id, start_expr, len_expr);
                    free(obj_expr);
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    result = strdup_fmt(
                        "({ PgySlice_%s _pgy_slice_%d = %s; size_t _pgy_start_%d = (size_t)(%s); size_t _pgy_len_%d = (size_t)(%s); if (_pgy_start_%d + _pgy_len_%d > _pgy_slice_%d.length) PGY_PANIC(\"Slice out of bounds\"); (PgySlice_%s){ _pgy_slice_%d.data + _pgy_start_%d, _pgy_len_%d }; })",
                        inner, tmp_id, obj_expr,
                        tmp_id, start_expr,
                        tmp_id, len_expr,
                        tmp_id, tmp_id, tmp_id,
                        inner, tmp_id, tmp_id, tmp_id);
                    free(obj_expr);
                }

                free(start_expr);
                free(len_expr);
                return result;
            }
        }
    }

    /* User function call */
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
        ASTNode *host_method = current_host_method_decl(ctx, callee->data.identifier.name);
        const char *host_name = transpiler_decl_name_local(host_decl);
        if (host_method != NULL && host_name != NULL) {
            CodeBuf *args_buf = codebuf_create();
            codebuf_write(args_buf, "self");
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                codebuf_write(args_buf, ", %s", arg);
                free(arg);
            }
            {
                char *result = strdup_fmt("%s_%s(%s)",
                    host_name, callee->data.identifier.name, args_buf->data);
                codebuf_destroy(args_buf);
                return result;
            }
        }
    }

    char *callee_str = NULL;
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *decl = find_function_decl(ctx, callee->data.identifier.name);
        if (func_has_generic_params(decl)) {
            const char *specialized_name = ensure_generic_specialization(ctx, decl, call);
            if (specialized_name != NULL)
                callee_str = pergyra_strdup(specialized_name);
        }
    }
    if (callee_str == NULL)
        callee_str = emit_expression(callee, ctx);

    /* Build argument list */
    CodeBuf *args_buf = codebuf_create();
    ASTNode *decl = (callee->type == AST_IDENTIFIER)
        ? find_callable_decl(ctx, callee->data.identifier.name) : NULL;
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        FuncParam *param = (decl != NULL && decl->type == AST_FUNC_DECL
                            && i < decl->data.func_decl.param_count)
            ? decl->data.func_decl.params[i] : NULL;
        ASTNode *intent_param_type = NULL;
        bool handled = false;
        char *arg = NULL;

        if (decl != NULL && decl->type == AST_INTENT_DECL) {
            size_t binding_count = decl->data.intent_decl.binding_count > 0
                ? decl->data.intent_decl.binding_count
                : (decl->data.intent_decl.involve_count
                    + decl->data.intent_decl.value_count);
            if (i < binding_count) {
                ASTNode *binding = decl->data.intent_decl.binding_count > 0
                    ? decl->data.intent_decl.bindings[i]
                    : (i < decl->data.intent_decl.involve_count
                        ? decl->data.intent_decl.involves[i]
                        : decl->data.intent_decl.values[i - decl->data.intent_decl.involve_count]);
                if (binding != NULL && binding->type == AST_INTENT_INVOLVES)
                    intent_param_type = binding->data.intent_involves.subject_type;
                else if (binding != NULL && binding->type == AST_INTENT_VALUE)
                    intent_param_type = binding->data.intent_value.value_type;
            }
        }

        if (param != NULL && param->type != NULL
            && (param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_REF)) {
            char *param_type = render_type_name(param->type);
            bool slot_param = param_type != NULL
                && (strncmp(param_type, "Slot<", 5) == 0
                    || strncmp(param_type, "SecureSlot<", 11) == 0);
            bool secure_param = param_type != NULL
                && strncmp(param_type, "SecureSlot<", 11) == 0;
            if (slot_param) {
                const char *inner = NULL;
                const char *slot_name = NULL;
                bool secure = false;
                bool saved_suppress = ctx->suppress_slot_auto_read;
                ctx->suppress_slot_auto_read = true;
                arg = emit_expression(call->data.call.arguments[i], ctx);
                ctx->suppress_slot_auto_read = saved_suppress;
                resolve_slot_target(ctx, call->data.call.arguments[i],
                    &inner, &slot_name, &secure);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                if (slot_name != NULL) {
                    char *slot_ref = slot_ref_expr(ctx, slot_name, arg);
                    codebuf_write(args_buf, "%s", slot_ref);
                    free(slot_ref);
                    if (secure_param)
                        codebuf_write(args_buf, ", %s_token", slot_name);
                    handled = true;
                }
            }
            free(param_type);
        }

        if (!handled)
            arg = emit_expression(call->data.call.arguments[i], ctx);
        if (!handled && i > 0)
            codebuf_write(args_buf, ", ");
        if (!handled) {
            /* Subject arguments: pass by pointer (reference semantics) */
            bool is_subject_arg = false;
            if (param != NULL && param->type != NULL
                && param->name != NULL && strcmp(param->name, "self") != 0) {
                char *ptn = render_type_name(param->type);
                if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                    is_subject_arg = true;
                free(ptn);
            }
            if (!is_subject_arg && intent_param_type != NULL) {
                char *ptn = render_type_name(intent_param_type);
                if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                    is_subject_arg = true;
                free(ptn);
            }
            if (is_subject_arg) {
                /* Don't add & if already a pointer (subject-ref param) */
                bool already_ptr = false;
                ASTNode *arg_node = call->data.call.arguments[i];
                if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                    TypedVarEntry *entry = lookup_typed_entry(ctx,
                        arg_node->data.identifier.name);
                    if (entry != NULL && entry->is_subject_ref)
                        already_ptr = true;
                }
                if (already_ptr)
                    codebuf_write(args_buf, "%s", arg);
                else
                    codebuf_write(args_buf, "&%s", arg);
            } else {
                codebuf_write(args_buf, "%s", arg);
            }
        }
        free(arg);
    }

    char *result = strdup_fmt("%s(%s)", callee_str, args_buf->data);
    free(callee_str);
    codebuf_destroy(args_buf);
    return result;
}

char *
emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *function_name = NULL;
    const char *emitted_function_name = NULL;
    ASTNode *decl = NULL;
    size_t arg_count = 0;
    int wrapper_id = ++ctx->tmp_counter;
    char *wrapper_name = strdup_fmt("pgy_spawn_wrapper_%d", wrapper_id);
    char *args_type_name = NULL;
    char *return_type_name = infer_spawn_return_type_name(ctx, node);
    char *return_c_type = pergyra_strdup(pergyra_type_to_c(return_type_name));
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (target == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        arg_count = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        function_name = callee->data.identifier.name;
    if (function_name == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported spawn target at line %d", target->line);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    decl = find_function_decl(ctx, function_name);
    emitted_function_name = function_name;
    if (call != NULL && func_has_generic_params(decl)
        && infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count)) {
        const char *specialized = ensure_generic_specialization(ctx, decl, call);
        if (specialized != NULL)
            emitted_function_name = specialized;
    }
    if (arg_count > 0)
        args_type_name = strdup_fmt("PgySpawnArgs_%d", wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            const char *arg_type = NULL;
            if (decl != NULL && i < decl->data.func_decl.param_count
                && decl->data.func_decl.params[i] != NULL
                && decl->data.func_decl.params[i]->type != NULL) {
                if (binding_count > 0) {
                    char *bound_type = render_type_name_with_bindings(ctx,
                        decl->data.func_decl.params[i]->type, bindings, binding_count);
                    arg_type = pergyra_type_to_c(bound_type);
                    codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
                    free(bound_type);
                    continue;
                }
                arg_type = pergyra_ast_type_to_c(decl->data.func_decl.params[i]->type);
            } else if (call != NULL) {
                arg_type = pergyra_type_to_c(infer_expression_type_name(
                    ctx, call->data.call.arguments[i]));
            }
            if (arg_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine spawn wrapper argument type for call '%s' at argument %llu",
                    function_name != NULL ? function_name : "<function>",
                    (unsigned long long) i);
                free(args_type_name);
                return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
            }
            codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
        }
        codebuf_write(ctx->decls, "} %s;\n", args_type_name);
    }

    codebuf_write(ctx->decls, "static void *%s(void *raw);\n", wrapper_name);
    codebuf_write(ctx->helpers, "\nstatic void *%s(void *raw)\n{\n", wrapper_name);
    if (args_type_name != NULL) {
        codebuf_write(ctx->helpers, "    %s *args = (%s *)raw;\n",
            args_type_name, args_type_name);
    } else {
        codebuf_write(ctx->helpers, "    (void)raw;\n");
    }

    if (strcmp(return_type_name, "Void") == 0) {
        codebuf_write(ctx->helpers, "    %s(", emitted_function_name);
    } else {
        codebuf_write(ctx->helpers,
            "    %s *result = (%s *)malloc(sizeof(%s));\n",
            return_c_type, return_c_type, return_c_type);
        codebuf_write(ctx->helpers,
            "    if (result == NULL) {\n"
            "        PGY_PANIC(\"spawn result allocation failed\");\n"
            "    }\n"
            "    *result = %s(",
            emitted_function_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "args->arg%zu", i);
    }
    codebuf_write(ctx->helpers, ");\n");

    if (args_type_name != NULL)
        codebuf_write(ctx->helpers, "    free(args);\n");

    if (strcmp(return_type_name, "Void") == 0)
        codebuf_write(ctx->helpers, "    return NULL;\n");
    else
        codebuf_write(ctx->helpers, "    return result;\n");
    codebuf_write(ctx->helpers, "}\n");

    CodeBuf *expr = codebuf_create();
    if (expr == NULL) {
        free(wrapper_name);
        free(args_type_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* spawn alloc failed */");
    }

    {
        /* spawn blocking → offload to dedicated blocking pool */
        const char *spawn_fn = node->data.spawn_expr.is_blocking
            ? "pgy_spawn_blocking" : "pgy_async_spawn";
        if (args_type_name == NULL) {
            codebuf_write(expr, "%s(%s, NULL)", spawn_fn, wrapper_name);
        } else {
            codebuf_write(expr,
                "({ %s *_pgy_args = (%s *)malloc(sizeof(%s)); "
                "if (_pgy_args == NULL) { PGY_PANIC(\"spawn arg allocation failed\"); } ",
                args_type_name, args_type_name, args_type_name);
            for (size_t i = 0; i < arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                codebuf_write(expr, "_pgy_args->arg%zu = %s; ", i, arg);
                free(arg);
            }
            codebuf_write(expr, "%s(%s, _pgy_args); })", spawn_fn, wrapper_name);
        }
    }

    char *result = pergyra_strdup(expr->data);
    codebuf_destroy(expr);
    free(wrapper_name);
    free(args_type_name);
    free(return_type_name);
    free(return_c_type);
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch  = emit_expression(node->data.channel_send.channel, ctx);
    char *val = emit_expression(node->data.channel_send.value, ctx);
    const char *inner = "Int";

    if (node->data.channel_send.channel != NULL
        && node->data.channel_send.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_send.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_send_%s(&%s, %s)", inner, ch, val);
    free(ch);
    free(val);
    return result;
}

char *
emit_channel_recv(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch = emit_expression(node->data.channel_recv.channel, ctx);
    const char *inner = "Int";

    if (node->data.channel_recv.channel != NULL
        && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_recv.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_recv_val_%s(&%s)", inner, ch);
    free(ch);
    return result;
}

char *
emit_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return pergyra_strdup("0");

    switch (node->type) {
    case AST_NUMBER:
        if (node->data.number.is_long)
            return strdup_fmt("%lldLL", (long long)(int64_t)node->data.number.value);
        if (node->data.number.value == (int64_t)node->data.number.value)
            return strdup_fmt("%lld", (long long)(int64_t)node->data.number.value);
        return strdup_fmt("%g", node->data.number.value);

    case AST_STRING:
    {
        char *escaped = escape_c_string_literal(node->data.string.value);
        char *result = strdup_fmt("\"%s\"", escaped);
        free(escaped);
        return result;
    }

    case AST_BOOLEAN:
        return pergyra_strdup(node->data.boolean.value ? "true" : "false");

    case AST_IDENTIFIER: {
        const char *id_name = node->data.identifier.name;
        /* Handle None used without parentheses as Option<Int> fallback */
        if (strcmp(id_name, "None") == 0) {
            const char *inner = "Int";
            if (ctx->expected_type != NULL && strncmp(ctx->expected_type, "Option<", 7) == 0) {
                inner = slot_inner_type_name(ctx->expected_type);
            }
            return strdup_fmt("None_%s()", inner);
        }
        /* Inside parallel wrapper: captured outer variables are accessed
         * through the context struct pointer.  (*_pctx->x) yields the
         * value, and &(*_pctx->x) collapses to _pctx->x (a pointer). */
        if (ctx->in_parallel_wrapper) {
            for (int i = 0; i < ctx->par_capture_slot_count; i++) {
                if (strcmp(ctx->par_capture_slot_names[i], id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
            for (int i = 0; i < ctx->par_capture_typed_count; i++) {
                if (strcmp(ctx->par_capture_typed_names[i], id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_class_has_field(ctx, id_name)) {
            return strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_relation_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_effect_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_zone_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_world_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        const char *ssa_name = transpiler_resolve_active_ssa_name(ctx, id_name);
        if (ssa_name != NULL) {
            char *c_ssa_name = transpiler_make_c_ssa_name(ssa_name);
            const char *slot_type = lookup_typed_var(ctx, id_name);
            if (slot_type == NULL) {
                char base_name[128];
                size_t version = 0;
                if (transpiler_parse_versioned_name(id_name, base_name, sizeof(base_name), &version))
                    slot_type = lookup_typed_var(ctx, base_name);
            }
            if (slot_type != NULL && transpiler_type_name_is_slot_like(slot_type)
                && !ctx->suppress_slot_auto_read) {
                const char *inner = slot_inner_type_name(slot_type);
                bool secure = strncmp(slot_type, "SecureSlot<", 11) == 0;
                const char *token_name = secure
                    ? lookup_slot_token_name(ctx, id_name)
                    : NULL;
                char fallback_token[96];
                if (secure && token_name == NULL) {
                    snprintf(fallback_token, sizeof(fallback_token), "%s_token", id_name);
                    token_name = fallback_token;
                }
                char *result = secure
                    ? strdup_fmt("pgy_secure_read_%s(&%s, &%s)",
                                  inner, c_ssa_name, token_name)
                    : strdup_fmt("pgy_read_%s(&%s)", inner, c_ssa_name);
                free(c_ssa_name);
                return result;
            }
            return c_ssa_name;
        }
        {
            ASTNode *alias_expr = lookup_alias_expr(ctx, id_name);
            if (alias_expr != NULL)
                return emit_expression(alias_expr, ctx);
        }
        TypedVarEntry *projection_entry = lookup_typed_entry(ctx, id_name);
        /* Slot sugar: auto-Read — emit pgy_read_T(&x) instead of x */
        if (!ctx->suppress_slot_auto_read && is_slot_var(ctx, id_name)) {
            const char *inner = lookup_slot_type(ctx, id_name);
            bool secure = lookup_slot_is_secure(ctx, id_name);
            if (inner == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for auto-read of '%s'",
                    id_name != NULL ? id_name : "<slot>");
                return pergyra_strdup("0");
            }
            char *slot_ref = slot_ref_expr(ctx, id_name, id_name);
            const char *token_name = secure
                ? lookup_slot_token_name(ctx, id_name)
                : NULL;
            char fallback_token[96];
            if (secure && token_name == NULL) {
                snprintf(fallback_token, sizeof(fallback_token), "%s_token", id_name);
                token_name = fallback_token;
            }
            char *result = secure
                ? strdup_fmt("pgy_secure_read_%s(%s, &%s)",
                              inner, slot_ref, token_name)
                : strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
            free(slot_ref);
            return result;
        }
        {
            const char *enum_variant = lookup_enum_variant_qualified_name(ctx, id_name);
            if (enum_variant != NULL)
                return pergyra_strdup(enum_variant);
        }
        if (projection_entry != NULL && projection_entry->is_projection_borrow) {
            const char *source_type = lookup_typed_var(ctx, projection_entry->source_slot);
            ASTNode *target_decl = find_class_decl(ctx, projection_entry->type_name);
            ASTNode *source_decl = source_type != NULL
                ? find_class_decl(ctx, source_type)
                : NULL;
            if (target_decl != NULL && source_decl != NULL) {
                return emit_projection_literal(ctx, target_decl, source_decl,
                    NULL, projection_entry->type_name, projection_entry->source_slot);
            }
        }
        return pergyra_strdup(id_name);
    }

    case AST_BINARY:
        return emit_binary(node, ctx);

    case AST_UNARY:
        return emit_unary(node, ctx);

    case AST_CALL:
        return emit_call(node, ctx);

    case AST_MEMBER_ACCESS: {
        if (node->data.member.object != NULL
            && node->data.member.object->type == AST_IDENTIFIER
            && node->data.member.name != NULL) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                node->data.member.object->data.identifier.name);
            if (entry != NULL && entry->is_projection_borrow) {
                const char *source_type = lookup_typed_var(ctx, entry->source_slot);
                ASTNode *source_decl = source_type != NULL
                    ? find_class_decl(ctx, source_type)
                    : NULL;
                char *source_path = NULL;
                int source_status = 0;
                if (source_decl != NULL) {
                    source_status = resolve_projection_source_path_rec(
                        ctx, source_decl, node->data.member.name, 0, &source_path);
                }
                if (source_status == 1 && source_path != NULL) {
                    TypedVarEntry *source_entry = lookup_typed_entry(ctx, entry->source_slot);
                    char *result = strdup_fmt(
                        (source_entry != NULL && source_entry->is_subject_ref)
                            ? "%s->%s"
                            : "%s.%s",
                        entry->source_slot, source_path);
                    free(source_path);
                    return result;
                }
                free(source_path);
            }
        }
        char *obj = emit_expression(node->data.member.object, ctx);
        /* Enum variant access: Color.Red → Color_Red */
        if (node->data.member.object->type == AST_IDENTIFIER
            && node->data.member.object->data.identifier.name[0] >= 'A'
            && node->data.member.object->data.identifier.name[0] <= 'Z') {
            char *result = strdup_fmt("%s_%s", obj, node->data.member.name);
            free(obj);
            return result;
        }
        if (node->data.member.object->type == AST_IDENTIFIER
            && strcmp(node->data.member.object->data.identifier.name, "self") == 0) {
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            bool self_is_pointer = current_class_uses_self_cell(ctx)
                || (host_decl != NULL
                    && (host_decl->type == AST_RELATION_DECL
                        || host_decl->type == AST_EFFECT_DECL
                        || host_decl->type == AST_ZONE_DECL
                        || host_decl->type == AST_WORLD_DECL));
            char *result = strdup_fmt(self_is_pointer
                ? "%s->%s"
                : "%s.%s", obj, node->data.member.name);
            free(obj);
            return result;
        }
        /* Subject-ref parameter: use -> for member access */
        if (node->data.member.object->type == AST_IDENTIFIER) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                node->data.member.object->data.identifier.name);
            if (entry != NULL && entry->is_subject_ref) {
                char *result = strdup_fmt("%s->%s", obj, node->data.member.name);
                free(obj);
                return result;
            }
        }
        char *result = strdup_fmt("%s.%s", obj, node->data.member.name);
        free(obj);
        return result;
    }

    case AST_ARRAY_ACCESS: {
        char *array = emit_expression(node->data.array_access.array, ctx);
        char *index = emit_expression(node->data.array_access.index, ctx);
        const char *array_type = infer_expression_type_name(ctx, node->data.array_access.array);
        char *result;
        if (strncmp(array_type, "Array<", 6) == 0 || strncmp(array_type, "Slice<", 6) == 0)
            result = strdup_fmt("%s.data[%s]", array, index);
        else
            result = strdup_fmt("%s[%s]", array, index);
        free(array);
        free(index);
        return result;
    }

    case AST_TUPLE_LITERAL: {
        /* Determine tuple type name from context. Priority:
         *   1) ctx->expected_type (let binding with annotation)
         *   2) ctx->current_return_type (inside `return` in tuple-returning fn)
         *   3) Build from element type names (best-effort fallback).
         * All three must start with '(' for tuple classification. */
        char tuple_name_buf[256];
        const char *tuple_name = NULL;
        if (ctx->expected_type != NULL && ctx->expected_type[0] == '(')
            tuple_name = ctx->expected_type;
        else if (ctx->current_return_type[0] == '(')
            tuple_name = ctx->current_return_type;
        if (tuple_name == NULL) {
            size_t off = 0;
            off += (size_t)snprintf(tuple_name_buf + off,
                sizeof(tuple_name_buf) - off, "(");
            for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
                const char *et =
                    infer_expression_type_name(ctx, node->data.tuple_literal.elements[i]);
                if (et == NULL || et[0] == '\0') et = "Int";
                if (i > 0) {
                    off += (size_t)snprintf(tuple_name_buf + off,
                        sizeof(tuple_name_buf) - off, ", ");
                }
                off += (size_t)snprintf(tuple_name_buf + off,
                    sizeof(tuple_name_buf) - off, "%s", et);
            }
            (void)snprintf(tuple_name_buf + off,
                sizeof(tuple_name_buf) - off, ")");
            tuple_name = tuple_name_buf;
        }
        const char *ctype = pergyra_type_to_c(tuple_name);
        CodeBuf *out = codebuf_create();
        codebuf_write(out, "((%s){", ctype);
        for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
            char *v = emit_expression(node->data.tuple_literal.elements[i], ctx);
            if (i > 0)
                codebuf_write(out, ", ");
            codebuf_write(out, ".f%zu = %s", i, v);
            free(v);
        }
        codebuf_write(out, "})");
        char *result = pergyra_strdup(out->data);
        codebuf_destroy(out);
        return result;
    }

    case AST_ARRAY_LITERAL: {
        const char *array_type = infer_expression_type_name(ctx, node);
        const char *inner = slot_inner_type_name(array_type);
        int tmp_id = ++ctx->tmp_counter;
        CodeBuf *buf = codebuf_create();
        codebuf_write(buf, "({ PgyArray_%s _pgy_arr_%d = pgy_array_new_%s(%zu); ",
            inner, tmp_id, inner, node->data.array_literal.count);
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            char *elem = emit_expression(node->data.array_literal.elements[i], ctx);
            codebuf_write(buf, "pgy_array_push_%s(&_pgy_arr_%d, %s); ",
                inner, tmp_id, elem);
            free(elem);
        }
        codebuf_write(buf, "_pgy_arr_%d; })", tmp_id);
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }

    case AST_ASSIGNMENT: {
        /* Slot sugar: x = 5 → pgy_write_T(&x, 5) */
        if (node->data.assignment.target->type == AST_IDENTIFIER) {
            const char *tgt_name = node->data.assignment.target->data.identifier.name;
            if (is_slot_var(ctx, tgt_name)) {
                const char *inner = lookup_slot_type(ctx, tgt_name);
                bool secure = lookup_slot_is_secure(ctx, tgt_name);
                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for assignment to '%s'",
                        tgt_name != NULL ? tgt_name : "<slot>");
                    return pergyra_strdup("0");
                }
                char *value = emit_expression(node->data.assignment.value, ctx);
                char *slot_ref = slot_ref_expr(ctx, tgt_name, tgt_name);
                char *result;
                if (secure) {
                    const char *token_name = lookup_slot_token_name(ctx, tgt_name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", tgt_name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                        inner, slot_ref, value, token_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                        inner, slot_ref, value);
                }
                free(slot_ref);
                free(value);
                return result;
            }
        }
        char *target = emit_expression(node->data.assignment.target, ctx);
        char *value  = emit_expression(node->data.assignment.value,  ctx);
        const char *root_slot_name = assignment_target_root_slot_name(
            node->data.assignment.target);
        const char *root_subfield_name = assignment_target_root_subfield_name(
            node->data.assignment.target);
        char *invalidation = emit_current_overlay_projection_invalidation(
            ctx, root_slot_name, root_subfield_name);
        char *result;
        if (invalidation != NULL)
            result = strdup_fmt("({ %s%s = %s; })", invalidation, target, value);
        else
            result = strdup_fmt("%s = %s", target, value);
        free(invalidation);
        free(target);
        free(value);
        return result;
    }

    case AST_AWAIT_EXPR:
        {
            char *expr = emit_expression(node->data.await_expr.expression, ctx);
            const char *inner = lookup_future_inner_type(ctx,
                node->data.await_expr.expression);
            bool is_remote = is_remote_future_expr(ctx,
                node->data.await_expr.expression);
            char *result;
            if (strcmp(inner, "Void") == 0) {
                result = strdup_fmt("pgy_await_void(%s)", expr);
            } else if (is_remote) {
                /* RemoteFuture<T> → Result<T>: wrap in PgyResult */
                result = strdup_fmt("pgy_await_result_take(%s, %s, %s)",
                    expr, inner, pergyra_type_to_c(inner));
            } else {
                result = strdup_fmt("pgy_await_take(%s, %s)",
                    expr, pergyra_type_to_c(inner));
            }
            free(expr);
            return result;
        }

    case AST_SPAWN_EXPR:
        return emit_spawn_expr(node, ctx);

    case AST_CHANNEL_SEND:
        return emit_channel_send(node, ctx);

    case AST_CHANNEL_RECV:
        return emit_channel_recv(node, ctx);

    case AST_EVENT_INVOKE: {
        char *event = emit_expression(node->data.event_invoke.event, ctx);
        CodeBuf *args = codebuf_create();
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            char *arg = emit_expression(node->data.event_invoke.arguments[i], ctx);
            if (i > 0)
                codebuf_write(args, ", ");
            codebuf_write(args, "%s", arg);
            free(arg);
        }
        char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                  event,
                                  event,
                                  args->len > 0 ? ", " : "",
                                  args->data);
        free(event);
        codebuf_destroy(args);
        return result;
    }

    case AST_CONTEXT_ACCESS:
        if (node->data.context_access.role_slot_name != NULL) {
            return strdup_fmt("self->%s", node->data.context_access.role_slot_name);
        }
        return pergyra_strdup("self");

    case AST_PARTY_INSTANCE: {
        CodeBuf *assignments = codebuf_create();
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            char *value = emit_expression(node->data.party_instance.assignments[i].value, ctx);
            if (i > 0)
                codebuf_write(assignments, ", ");
            codebuf_write(assignments, ".%s = %s",
                          node->data.party_instance.assignments[i].slot_name,
                          value);
            free(value);
        }
        char *result = strdup_fmt("(%s){%s}",
                                  node->data.party_instance.party_type,
                                  assignments->data);
        codebuf_destroy(assignments);
        return result;
    }

    case AST_LAMBDA_EXPR:
        return emit_lambda_expr(node, ctx);

    default:
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported expression node type %d at line %d",
            (int)node->type, node->line);
        return pergyra_strdup("0");
    }
}
