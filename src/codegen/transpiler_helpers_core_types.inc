/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

const char *
pergyra_primitive_to_c(const char *name)
{
    if (strcmp(name, "Int")    == 0) return "int32_t";
    if (strcmp(name, "Long")   == 0) return "int64_t";
    if (strcmp(name, "Float")  == 0) return "float";
    if (strcmp(name, "Double") == 0) return "double";
    if (strcmp(name, "Bool")   == 0) return "bool";
    if (strcmp(name, "String") == 0) return "char*";
    if (strcmp(name, "QubitSlot") == 0) return "int32_t";
    if (strcmp(name, "Void")   == 0) return "void";
    /* Single uppercase letter = generic type parameter → void* (type erasure) */
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
        return "void*";
    return name; /* user-defined type — pass through */
}

/*
 * slot_inner_type_name("Slot<Int>")       → "Int"
 * slot_inner_type_name("SecureSlot<Int>") → "Int"
 */
const char *
slot_inner_type_name(const char *slot_type_name)
{
    const char *open = strchr(slot_type_name, '<');
    if (open == NULL)
        return slot_type_name;
    open++; /* skip '<' */

    /* Trim trailing '>' — return a static buffer (fine for our uses) */
    static char buf[128];
    const char *close = strrchr(slot_type_name, '>');
    if (close == NULL || close <= open)
        return "Unknown";

    size_t len = (size_t)(close - open);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, open, len);
    buf[len] = '\0';
    return buf;
}

/*
 * For multi-arg generics like Result<T, E>, split the inner body at the
 * top-level comma and sanitize each piece into a C-identifier fragment.
 * "Int, NetError"        → "Int_NetError"
 * "Map<Int, String>, E"  → "Map_Int_String__E"  (handles nested generics)
 *
 * Commas / angle brackets / spaces collapse to single underscores.
 * Returns a static buffer.
 */
static const char *
generic_args_to_c_suffix(const char *inner_body)
{
    static char buf[256];
    size_t pos = 0;
    bool prev_was_sep = false;

    if (inner_body == NULL) {
        buf[0] = '\0';
        return buf;
    }

    for (const char *p = inner_body; *p != '\0' && pos + 1 < sizeof(buf); p++) {
        char c = *p;
        bool is_sep = (c == ',' || c == '<' || c == '>' || c == ' ' || c == '\t');
        if (is_sep) {
            if (!prev_was_sep && pos > 0) {
                buf[pos++] = '_';
                prev_was_sep = true;
            }
        } else {
            buf[pos++] = c;
            prev_was_sep = false;
        }
    }
    /* Strip trailing underscore(s) */
    while (pos > 0 && buf[pos - 1] == '_')
        pos--;
    buf[pos] = '\0';
    return buf;
}

static void
copy_capped_string(char *dst, size_t dst_size, const char *src);

static bool
transpiler_result_suffix_from_type_name(const char *type_name,
                                        char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (type_name == NULL || strncmp(type_name, "Result<", 7) != 0) {
        return false;
    }

    const char *open = type_name + 7;
    const char *close = strrchr(type_name, '>');
    if (close == NULL || close <= open) {
        return false;
    }

    char inner[128];
    size_t n = (size_t)(close - open);
    if (n >= sizeof(inner))
        n = sizeof(inner) - 1;
    memcpy(inner, open, n);
    inner[n] = '\0';

    if (strchr(inner, ',') == NULL) {
        copy_capped_string(out, out_size, inner);
    } else {
        const char *sanitized = generic_args_to_c_suffix(inner);
        copy_capped_string(out, out_size, sanitized);
    }

    return out[0] != '\0';
}

static bool
transpiler_result_suffix_from_context(TranspilerCtx *ctx,
                                      char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (ctx == NULL) {
        return false;
    }

    if (ctx->expected_type != NULL
        && transpiler_result_suffix_from_type_name(ctx->expected_type,
                                                   out, out_size)) {
        return true;
    }

    if (ctx->current_return_type[0] != '\0'
        && transpiler_result_suffix_from_type_name(ctx->current_return_type,
                                                   out, out_size)) {
        return true;
    }

    return false;
}

static void
ensure_result_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                const char *ok_type, const char *err_type);

static void
ensure_result_specialization_from_type_name_to(TranspilerCtx *ctx, CodeBuf *dst,
                                               const char *type_name)
{
    if (ctx == NULL || dst == NULL || type_name == NULL
        || strncmp(type_name, "Result<", 7) != 0) {
        return;
    }

    const char *open = type_name + 7;
    const char *close = strrchr(type_name, '>');
    if (close == NULL || close <= open) {
        return;
    }

    char inner[256];
    size_t n = (size_t)(close - open);
    if (n >= sizeof(inner))
        n = sizeof(inner) - 1;
    memcpy(inner, open, n);
    inner[n] = '\0';

    const char *comma = strchr(inner, ',');
    if (comma == NULL) {
        return;
    }

    char ok_type[128];
    char err_type[128];
    size_t ok_len = (size_t)(comma - inner);
    while (ok_len > 0
           && (inner[ok_len - 1] == ' ' || inner[ok_len - 1] == '\t')) {
        ok_len--;
    }
    if (ok_len >= sizeof(ok_type))
        ok_len = sizeof(ok_type) - 1;
    memcpy(ok_type, inner, ok_len);
    ok_type[ok_len] = '\0';

    const char *err_start = comma + 1;
    while (*err_start == ' ' || *err_start == '\t')
        err_start++;
    {
        size_t err_len = strlen(err_start);
        if (err_len >= sizeof(err_type))
            err_len = sizeof(err_type) - 1;
        memcpy(err_type, err_start, err_len);
        err_type[err_len] = '\0';
    }

    if (ok_type[0] == '\0' || err_type[0] == '\0') {
        return;
    }

    ensure_result_specialization_to(ctx, dst, ok_type, err_type);
}

static const char *
channel_inner_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    const char *type_name = infer_expression_type_name(ctx, expr);
    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
        return slot_inner_type_name(type_name);
    return "Unknown";
}

static void
sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size)
{
    size_t out = 0;
    bool last_was_underscore = false;

    if (buf_size == 0)
        return;

    if (type_name == NULL || *type_name == '\0') {
        snprintf(buf, buf_size, "Int");
        return;
    }

    for (size_t i = 0; type_name[i] != '\0' && out + 1 < buf_size; i++) {
        unsigned char ch = (unsigned char)type_name[i];
        if ((ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')) {
            buf[out++] = (char)ch;
            last_was_underscore = false;
        } else {
            if (out == 0 || last_was_underscore)
                continue;
            buf[out++] = '_';
            last_was_underscore = true;
        }
    }
    while (out > 0 && buf[out - 1] == '_')
        out--;
    if (out == 0) {
        snprintf(buf, buf_size, "Int");
        return;
    }
    if (buf[0] >= '0' && buf[0] <= '9') {
        if (out + 2 >= buf_size)
            out = buf_size > 3 ? buf_size - 3 : 0;
        if (out > 0)
            memmove(buf + 2, buf, out);
        buf[0] = 'T';
        buf[1] = '_';
        out += 2;
    }
    buf[out] = '\0';
}

static void
copy_capped_string(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst == NULL || dst_size == 0)
        return;

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    len = strlen(src);
    if (len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static const char *
constructed_arg_name_at(const char *type_name, int arg_index)
{
    static char buf[128];
    const char *lt;
    int current = 0;
    int depth = 0;
    size_t out = 0;

    if (type_name == NULL || arg_index < 0)
        return "Unknown";

    lt = strchr(type_name, '<');
    if (lt == NULL)
        return "Unknown";
    lt++;

    for (const char *p = lt; *p != '\0'; p++) {
        char ch = *p;
        if (ch == '<') {
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth++;
            continue;
        }
        if (ch == '>') {
            if (depth == 0)
                break;
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth--;
            continue;
        }
        if (ch == ',' && depth == 0) {
            if (current == arg_index)
                break;
            current++;
            continue;
        }
        if (current == arg_index) {
            if (!(out == 0 && ch == ' ') && out + 1 < sizeof(buf))
                buf[out++] = ch;
        }
    }

    while (out > 0 && buf[out - 1] == ' ')
        out--;
    buf[out] = '\0';
    return out > 0 ? buf : "Int";
}

static void
copy_constructed_arg_name_at(const char *type_name, int arg_index,
                             char *buf, size_t buf_size)
{
    const char *arg_name;

    if (buf == NULL || buf_size == 0)
        return;

    arg_name = constructed_arg_name_at(type_name, arg_index);
    if (arg_name == NULL)
        arg_name = "Unknown";
    snprintf(buf, buf_size, "%s", arg_name);
}

const char *
pergyra_type_to_c(const char *name)
{
    static char buf[128];
    char suffix[96];

    if (strcmp(name, "HashMap") == 0)
        return "PgyHashMap_Int";
    if (strcmp(name, "HashMapStr") == 0)
        return "PgyHashMap_String";
    if (strcmp(name, "List") == 0)
        return "PgyList_Int";
    if (strcmp(name, "ListStr") == 0)
        return "PgyList_String";
    if (strcmp(name, "Set") == 0)
        return "PgySet_String";
    if (strcmp(name, "Queue") == 0)
        return "PgyQueue_Int";
    if (strcmp(name, "Fsm") == 0)
        return "PgyFsm";
    if (strcmp(name, "Timer") == 0)
        return "PgyTimer";
    if (strcmp(name, "Cooldown") == 0)
        return "PgyCooldown";
    if (strcmp(name, "Allocator") == 0)
        return "PgyAllocator";
    if (strncmp(name, "List<", 5) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyList_%s", suffix);
        return buf;
    }
    if (strncmp(name, "HashMap<", 8) == 0) {
        sanitize_c_suffix(constructed_arg_name_at(name, 1), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyHashMap_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Queue<", 6) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyQueue_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Set<", 4) == 0) {
        const char *inner = slot_inner_type_name(name);
        if (inner != NULL && strcmp(inner, "String") == 0) {
            snprintf(buf, sizeof(buf), "PgySet_String");
        } else if (inner != NULL && strcmp(inner, "Int") == 0) {
            snprintf(buf, sizeof(buf), "PgySet_int");
        } else {
            sanitize_c_suffix(inner, suffix, sizeof(suffix));
            snprintf(buf, sizeof(buf), "PgySet_%s", suffix);
        }
        return buf;
    }
    if (strncmp(name, "RemoteFuture<", 13) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Future<", 7) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Channel<", 8) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyChannel_%s", inner);
        return buf;
    }
    if (strncmp(name, "Weak<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyWeak_%s", inner);
        return buf;
    }
    if (strncmp(name, "Rc<", 3) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyRc_%s", inner);
        return buf;
    }
    if (strncmp(name, "Box<Array<", 10) == 0) {
        static char buf[128];
        const char *inner = name + 10;
        const char *close = strstr(inner, ">>");
        size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
        if (len > 63) len = 63;
        char inner_buf[64];
        memcpy(inner_buf, inner, len);
        inner_buf[len] = '\0';
        snprintf(buf, sizeof(buf), "PgyBoxArray_%s", inner_buf);
        return buf;
    }
    if (strncmp(name, "Box<", 4) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyBox_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slice<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlice_%s", inner);
        return buf;
    }
    if (strncmp(name, "Array<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyArray_%s", inner);
        return buf;
    }
    if (strncmp(name, "SecureSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySecureSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "ReadView<", 9) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "WriteView<", 10) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "DeviceSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyDeviceSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slot<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Result<", 7) == 0) {
        static char buf[256];
        const char *inner = slot_inner_type_name(name);
        /* Result<T> = default error type (PgyError/String). Keep legacy
         * PgyResult_<T> suffix so existing typedefs work. */
        if (strchr(inner, ',') == NULL) {
            snprintf(buf, sizeof(buf), "PgyResult_%s", inner);
            return buf;
        }
        /* Result<T, E> = custom error. Sanitize "T, E" to "T_E". */
        const char *suffix = generic_args_to_c_suffix(inner);
        size_t prefix_len = strlen("PgyResult_");
        memcpy(buf, "PgyResult_", prefix_len);
        copy_capped_string(buf + prefix_len, sizeof(buf) - prefix_len, suffix);
        return buf;
    }
    if (strncmp(name, "Option<", 7) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyOption_%s", inner);
        return buf;
    }
    /* Tuple type: "(T0, T1, ...)" → "PgyTuple_T0_T1_..._t" */
    if (name[0] == '(') {
        static char buf[256];
        size_t off = 0;
        off += (size_t)snprintf(buf + off, sizeof(buf) - off, "PgyTuple_");
        size_t i = 1;
        size_t n = strlen(name);
        bool first = true;
        while (i < n && name[i] != ')') {
            /* skip whitespace */
            while (i < n && (name[i] == ' ' || name[i] == '\t'))
                i++;
            /* read one element up to ',' or ')' (depth-tracking) */
            char elem[96];
            size_t eo = 0;
            int depth = 0;
            while (i < n && eo + 1 < sizeof(elem)) {
                char c = name[i];
                if (depth == 0 && (c == ',' || c == ')'))
                    break;
                if (c == '<' || c == '(') depth++;
                if (c == '>' || c == ')') depth--;
                elem[eo++] = c;
                i++;
            }
            elem[eo] = '\0';
            /* trim trailing space */
            while (eo > 0 && (elem[eo-1] == ' ' || elem[eo-1] == '\t'))
                elem[--eo] = '\0';
            char sane[96];
            sanitize_c_suffix(elem, sane, sizeof(sane));
            if (!first) {
                off += (size_t)snprintf(buf + off, sizeof(buf) - off, "_");
            }
            off += (size_t)snprintf(buf + off, sizeof(buf) - off, "%s", sane);
            first = false;
            if (i < n && name[i] == ',') i++;
        }
        snprintf(buf + off, sizeof(buf) - off, "_t");
        return buf;
    }
    return pergyra_primitive_to_c(name);
}

static void
append_type_name(CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    /* Tuple type: (T0, T1, ...) */
    if (type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        codebuf_write(buf, "(");
        for (size_t i = 0; i < type_node->data.type.tuple_element_count; i++) {
            if (i > 0)
                codebuf_write(buf, ", ");
            append_type_name(buf, type_node->data.type.tuple_elements[i]);
        }
        codebuf_write(buf, ")");
        return;
    }

    {
        const char *bound = NULL;
        if (g_type_render_ctx != NULL)
            bound = lookup_generic_binding(g_type_render_ctx, type_node->data.type.name);
        codebuf_write(buf, "%s", bound != NULL ? bound : type_node->data.type.name);
    }
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        codebuf_write(buf, "<");
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *param = type_node->data.type.generic_args->params[i];
            if (i > 0)
                codebuf_write(buf, ", ");
            if (param != NULL && param->constraint != NULL) {
                append_type_name(buf, param->constraint);
            } else if (param != NULL && param->name != NULL) {
                codebuf_write(buf, "%s", param->name);
            } else {
                codebuf_write(buf, "Int");
            }
        }
        codebuf_write(buf, ">");
    }
}

static bool resolve_slot_target(TranspilerCtx *ctx, ASTNode *slot_arg,
                                const char **inner_out, const char **slot_name_out,
                                bool *secure_out);

static char *
render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name(type_node->data.channel_type.element_type);
        char *result = strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(type_node->data.future_type.value_type);
        char *result = strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("Int");
    append_type_name(buf, type_node);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
