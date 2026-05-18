#ifndef PGY_TRANSPILER_ENUM_DECL_EMIT_H
#define PGY_TRANSPILER_ENUM_DECL_EMIT_H

static bool
transpiler_enum_method_emit_name(char *out, size_t out_size,
                                 const char *enum_name,
                                 const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_%s",
        enum_name != NULL ? enum_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_enum_method_surface_desc(char *out, size_t out_size,
                                    const char *enum_name,
                                    const char *method_name,
                                    const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "enum method parameter '%s.%s(%s)'",
        enum_name != NULL ? enum_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)",
        param_name != NULL ? param_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_enum_format_too_long(TranspilerCtx *ctx, const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "enum generated string");
}

static void
emit_enum_decl_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ename = ast_enum_name(node);
    size_t variant_count = 0;
    char **variants = ast_enum_variants(node, &variant_count);
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, ename, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for enum methods '%s'",
            ename != NULL ? ename : "(anonymous-enum)");
        return;
    }

        /* Check if any variant has data ??tagged union */
        bool has_data = false;
        for (size_t i = 0; i < variant_count; i++) {
            if (ast_enum_variant_param_count(node, i) > 0) {
                has_data = true;
                break;
            }
        }

        if (!has_data) {
            /* Simple enum: typedef enum { Color_Red=0, ... } Color; */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < variant_count; i++) {
                codebuf_write(ctx->out, "    %s_%s = %zu",
                    ename, variants != NULL ? variants[i] : NULL, i);
                if (i + 1 < variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s;\n\n", ename);
        } else {
            /* Tagged union:
             * typedef enum { Shape_TAG_Circle, Shape_TAG_Rect, Shape_TAG_None } Shape_Tag;
             * typedef struct {
             *     Shape_Tag tag;
             *     union {
             *         struct { int32_t _0; } Circle;
             *         struct { int32_t _0; int32_t _1; } Rect;
             *     };
             * } Shape; */

            /* Tag enum */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < variant_count; i++) {
                codebuf_write(ctx->out, "    %s_TAG_%s = %zu",
                    ename, variants != NULL ? variants[i] : NULL, i);
                if (i + 1 < variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s_Tag;\n\n", ename);

            /* Tagged union struct */
            codebuf_write(ctx->out, "typedef struct {\n");
            codebuf_write(ctx->out, "    %s_Tag tag;\n", ename);
            codebuf_write(ctx->out, "    union {\n");
            for (size_t i = 0; i < variant_count; i++) {
                size_t pc = ast_enum_variant_param_count(node, i);
                if (pc == 0) continue;
                codebuf_write(ctx->out, "        struct { ");
                for (size_t p = 0; p < pc; p++) {
                    ASTNode *pt = ast_enum_variant_param(node, i, p);
                    char ctype[256];
                    if (!transpiler_require_ast_c_type_copy(
                            ctx,
                            pt,
                            "enum variant payload field",
                            ctype,
                            sizeof(ctype))) {
                        return;
                    }
                    codebuf_write(ctx->out, "%s _%zu; ", ctype, p);
                }
                codebuf_write(ctx->out, "} %s;\n",
                    variants != NULL ? variants[i] : NULL);
            }
            codebuf_write(ctx->out, "    };\n");
            codebuf_write(ctx->out, "} %s;\n\n", ename);

            /* Constructor functions:
             * static inline Shape Shape_Circle(int32_t _0) {
             *     Shape v; v.tag = Shape_TAG_Circle; v.Circle._0 = _0; return v;
             * } */
            for (size_t i = 0; i < variant_count; i++) {
                size_t pc = ast_enum_variant_param_count(node, i);
                const char *vname = variants != NULL ? variants[i] : NULL;
                if (pc == 0) {
                    /* No-data variant: macro constant */
                    codebuf_write(ctx->out,
                        "#define %s_%s() ((%s){ .tag = %s_TAG_%s })\n",
                        ename, vname, ename, ename, vname);
                } else {
                    /* Data variant: constructor function */
                    codebuf_write(ctx->out,
                        "static inline %s %s_%s(", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++) {
                        ASTNode *pt = ast_enum_variant_param(node, i, p);
                        char ctype[256];
                        if (!transpiler_require_ast_c_type_copy(
                                ctx,
                                pt,
                                "enum variant constructor parameter",
                                ctype,
                                sizeof(ctype))) {
                            return;
                        }
                        if (p > 0) codebuf_write(ctx->out, ", ");
                        codebuf_write(ctx->out, "%s _%zu", ctype, p);
                    }
                    codebuf_write(ctx->out, ") {\n");
                    codebuf_write(ctx->out,
                        "    %s _v; _v.tag = %s_TAG_%s;\n", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++)
                        codebuf_write(ctx->out,
                            "    _v.%s._%zu = _%zu;\n", vname, p, p);
                    codebuf_write(ctx->out, "    return _v;\n}\n");
                }
            }
            codebuf_write(ctx->out, "\n");
        }

        for (size_t i = 0; i < method_view.count; i++) {
            const MIRDeclMethod *method_meta =
                transpiler_hosted_method_view_metadata(&method_view, i);
            ASTNode *method =
                transpiler_hosted_method_view_source_ast(&method_view, i);
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            emit_hosted_method_forward_decl_from_metadata(ename, method_meta,
                method, false, ctx->out, ctx);
        }

        for (size_t i = 0; i < method_view.count; i++) {
            const MIRDeclMethod *method_meta =
                transpiler_hosted_method_view_metadata(&method_view, i);
            ASTNode *method =
                transpiler_hosted_method_view_source_ast(&method_view, i);
            const MIRRoutine *mir_method;
            const char *method_name;
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            method_name = transpiler_mir_decl_method_name(method_meta);
            if (method_name == NULL)
                method_name = ast_declaration_name(method);
            mir_method = transpiler_hosted_method_view_routine(ctx, &method_view, i);
            if (transpiler_active_has_mir(ctx) && mir_method == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing routine for enum method '%s.%s'",
                    ename != NULL ? ename : "(anonymous-enum)",
                    method_name != NULL ? method_name : "(anonymous)");
                return;
            }
            if (mir_method != NULL) {
                char emitted_name[256];
                if (!transpiler_enum_method_emit_name(emitted_name,
                        sizeof(emitted_name), ename, method_name)) {
                    transpiler_enum_format_too_long(
                        ctx, "enum method emitted name");
                    return;
                }
                emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
                continue;
            }

            char ret_type_buf[256];
            const char *ret_type = "void";
            if (ast_func_return_type(method) != NULL
                && pergyra_ast_type_to_c_copy(ast_func_return_type(method),
                    ret_type_buf,
                    sizeof(ret_type_buf))) {
                ret_type = ret_type_buf;
            }

            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, ename, method_name, ename);
            for (size_t j = 0; j < ast_func_param_count(method); j++) {
                FuncParam *p = ast_func_param(method, j);
                if (p == NULL || p->name == NULL || strcmp(p->name, "self") == 0)
                    continue;
                char pt[256];
                char surface_desc[256];
                if (!transpiler_enum_method_surface_desc(surface_desc,
                        sizeof(surface_desc), ename, method_name,
                        p != NULL ? p->name : NULL)) {
                    transpiler_enum_format_too_long(
                        ctx, "enum method parameter diagnostic surface");
                    return;
                }
                if (!transpiler_require_ast_c_type_copy(ctx,
                        p != NULL ? p->type : NULL,
                        surface_desc,
                        pt,
                        sizeof(pt))) {
                    return;
                }
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");
            transpiler_emit_host_method_body_local(
                ctx,
                transpiler_find_decl_in_inventory_local(ctx, AST_ENUM_DECL, ename),
                ename,
                method,
                NULL,
                false);
            codebuf_write(ctx->out, "}\n");
        }
}

#endif /* PGY_TRANSPILER_ENUM_DECL_EMIT_H */
