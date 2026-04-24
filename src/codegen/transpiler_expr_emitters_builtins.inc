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

    /* Mark slot as explicitly released ??prevents auto-release at scope exit */
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
        /* BoxArray(arr) ??Box<Array<T>> wrapper around an existing array */
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
        /* BoxArray(arr) ??pgy_box_new_Array_T(arr) ??wraps array in a box */
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
