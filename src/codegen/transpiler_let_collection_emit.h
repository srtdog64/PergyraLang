#ifndef PGY_TRANSPILER_LET_COLLECTION_EMIT_H
#define PGY_TRANSPILER_LET_COLLECTION_EMIT_H

static bool
transpiler_try_emit_option_let(TranspilerCtx *ctx,
                               const char *name,
                               ASTNode *init,
                               char **ann_type_name_io)
{
    char inner_buf[128];
    const char *inner = NULL;
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;

    if (ann_type_name == NULL || strncmp(ann_type_name, "Option<", 7) != 0)
        return false;

    if (slot_inner_type_name_copy(ann_type_name, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;
    if (inner == NULL || inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Option binding '%s' requires concrete Option<T> annotation",
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (init == NULL
        || init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }

    ASTNode *callee = ast_call_callee(init);
    const char *callee_name = ast_identifier_name(callee);
    if (strcmp(callee_name, "Some") == 0 && ast_call_arg_count(init) == 1) {
        char *arg = emit_expression(ast_call_argument(init, 0), ctx);
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyOption_%s %s = Some_%s(%s);\n",
            inner, name, inner, arg);
        register_typed_var(ctx, name, ann_type_name);
        free(arg);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (strcmp(callee_name, "None") == 0 && ast_call_arg_count(init) == 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyOption_%s %s = None_%s();\n",
            inner, name, inner);
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }

    return false;
}

static bool
transpiler_try_emit_map_new_let(TranspilerCtx *ctx,
                                const char *name,
                                ASTNode *resolved_ann,
                                char **ann_type_name_io)
{
    GenericParams *resolved_generic_args = ast_type_generic_args(resolved_ann);
    GenericParam *key_param = ast_generic_param_at(resolved_generic_args, 0);
    GenericParam *value_param = ast_generic_param_at(resolved_generic_args, 1);
    ASTNode *key_constraint = ast_generic_param_constraint(key_param);
    ASTNode *value_constraint = ast_generic_param_constraint(value_param);
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;
    char *key = key_constraint != NULL
        ? render_type_name(key_constraint)
        : (ast_generic_param_name(key_param) != NULL
            ? pergyra_strdup(ast_generic_param_name(key_param)) : NULL);
    char *value = value_constraint != NULL
        ? render_type_name(value_constraint)
        : (ast_generic_param_name(value_param) != NULL
            ? pergyra_strdup(ast_generic_param_name(value_param)) : NULL);

    if (key == NULL || key[0] == '\0' || value == NULL || value[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: HashMap binding '%s' requires explicit concrete HashMap<K, V> annotation",
            name != NULL ? name : "<binding>");
        free(key);
        free(value);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (strcmp(key, "String") == 0 && value != NULL) {
        char map_c_type_buf[256];
        char suffix_buf[128];
        const char *map_c_type = NULL;
        if (pergyra_type_to_c_copy(ann_type_name, map_c_type_buf,
                sizeof(map_c_type_buf))) {
            map_c_type = map_c_type_buf;
        }
        if (map_c_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: HashMap binding '%s' annotation cannot be rendered as a stable C type",
                name != NULL ? name : "<binding>");
            free(key);
            free(value);
            free(ann_type_name);
            *ann_type_name_io = NULL;
            return true;
        }
        ensure_collection_specialization(ctx, "Map", value);
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = pgy_map_new_%s();\n",
            map_c_type, name, suffix_buf);
        register_typed_var(ctx, name, ann_type_name);
        free(key);
        free(value);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }

    free(key);
    free(value);
    return false;
}

static bool
transpiler_try_emit_list_or_queue_new_let(TranspilerCtx *ctx,
                                          const char *name,
                                          const char *callee_name,
                                          const char *type_name,
                                          const char *inner,
                                          char **ann_type_name_io)
{
    char c_type_buf[256];
    char suffix_buf[128];
    const char *c_type = NULL;
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;
    const char *collection = NULL;
    const char *runtime_prefix = NULL;

    if (strcmp(callee_name, "ListNew") == 0 && strcmp(type_name, "List") == 0) {
        collection = "List";
        runtime_prefix = "pgy_list_new";
    } else if (strcmp(callee_name, "QueueNew") == 0
               && strcmp(type_name, "Queue") == 0) {
        collection = "Queue";
        runtime_prefix = "pgy_queue_new";
    } else {
        return false;
    }

    if (pergyra_type_to_c_copy(ann_type_name, c_type_buf, sizeof(c_type_buf)))
        c_type = c_type_buf;
    if (c_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: %s binding '%s' annotation cannot be rendered as a stable C type",
            collection,
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    ensure_collection_specialization(ctx, collection, inner);
    collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
    write_indent(ctx);
    codebuf_write(ctx->out, "%s %s = %s_%s();\n",
        c_type, name, runtime_prefix, suffix_buf);
    register_typed_var(ctx, name, ann_type_name);
    free(ann_type_name);
    *ann_type_name_io = NULL;
    return true;
}

static bool
transpiler_try_emit_collection_ctor_let(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *resolved_ann,
                                        const char *resolved_ann_type_name,
                                        char **ann_type_name_io)
{
    const char *callee_name;
    const char *type_name = resolved_ann_type_name;
    char inner_buf[128];
    const char *inner = NULL;
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;

    if (ann_type_name == NULL
        || resolved_ann == NULL
        || resolved_ann->type != AST_TYPE
        || resolved_ann_type_name == NULL
        || init == NULL
        || init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }
    if (strcmp(resolved_ann_type_name, "HashMap") != 0
        && strcmp(resolved_ann_type_name, "List") != 0
        && strcmp(resolved_ann_type_name, "Queue") != 0) {
        return false;
    }

    callee_name = ast_identifier_name(ast_call_callee(init));
    if (slot_inner_type_name_copy(ann_type_name, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;

    if (strcmp(callee_name, "MapNew") == 0
        && strcmp(type_name, "HashMap") == 0
        && ast_generic_param_count(ast_type_generic_args(resolved_ann)) == 2) {
        return transpiler_try_emit_map_new_let(ctx, name, resolved_ann,
                                               ann_type_name_io);
    }
    return transpiler_try_emit_list_or_queue_new_let(ctx,
                                                     name,
                                                     callee_name,
                                                     type_name,
                                                     inner,
                                                     ann_type_name_io);
}

#endif /* PGY_TRANSPILER_LET_COLLECTION_EMIT_H */
