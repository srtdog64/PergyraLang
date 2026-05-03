static void
emit_enum_decl_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ename = node->data.enum_decl.name;
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, ename, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_MIR_TOPOLOGY_INVALID,
            PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING,
            PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
            "MIR-only C path missing declaration metadata for enum methods '%s'",
            ename != NULL ? ename : "(anonymous-enum)");
        return;
    }

        /* Check if any variant has data → tagged union */
        bool has_data = false;
        for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
            if (node->data.enum_decl.variant_param_counts != NULL
                && node->data.enum_decl.variant_param_counts[i] > 0) {
                has_data = true;
                break;
            }
        }

        if (!has_data) {
            /* Simple enum: typedef enum { Color_Red=0, ... } Color; */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
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
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_TAG_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s_Tag;\n\n", ename);

            /* Tagged union struct */
            codebuf_write(ctx->out, "typedef struct {\n");
            codebuf_write(ctx->out, "    %s_Tag tag;\n", ename);
            codebuf_write(ctx->out, "    union {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                if (pc == 0) continue;
                codebuf_write(ctx->out, "        struct { ");
                for (size_t p = 0; p < pc; p++) {
                    ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                    const char *ctype = transpiler_require_ast_c_type(
                        ctx,
                        pt,
                        "enum variant payload field");
                    if (ctype == NULL)
                        return;
                    codebuf_write(ctx->out, "%s _%zu; ", ctype, p);
                }
                codebuf_write(ctx->out, "} %s;\n",
                    node->data.enum_decl.variants[i]);
            }
            codebuf_write(ctx->out, "    };\n");
            codebuf_write(ctx->out, "} %s;\n\n", ename);

            /* Constructor functions:
             * static inline Shape Shape_Circle(int32_t _0) {
             *     Shape v; v.tag = Shape_TAG_Circle; v.Circle._0 = _0; return v;
             * } */
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                const char *vname = node->data.enum_decl.variants[i];
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
                        ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                        const char *ctype = transpiler_require_ast_c_type(
                            ctx,
                            pt,
                            "enum variant constructor parameter");
                        if (ctype == NULL)
                            return;
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
            ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            emit_hosted_method_forward_decl_from_metadata(ename, method_meta,
                method, false, ctx->out, ctx);
        }

        for (size_t i = 0; i < method_view.count; i++) {
            const MIRDeclMethod *method_meta =
                transpiler_hosted_method_view_metadata(&method_view, i);
            ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
            const MIRRoutine *mir_method;
            const char *method_name;
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            method_name = transpiler_mir_decl_method_name(method_meta);
            if (method_name == NULL)
                method_name = method->data.func_decl.name;
            mir_method = transpiler_hosted_method_view_routine(ctx, &method_view, i);
            if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "MIR-only C path missing routine for enum method '%s.%s'",
                        ename != NULL ? ename : "(anonymous-enum)",
                        method_name != NULL ? method_name : "(anonymous)");
                }
                return;
            }
            if (mir_method != NULL) {
                char emitted_name[256];
                snprintf(emitted_name, sizeof(emitted_name), "%s_%s", ename,
                    method_name != NULL ? method_name : "(anonymous)");
                emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
                continue;
            }

            const char *ret_type = "void";
            if (method->data.func_decl.return_type != NULL)
                ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, ename, method_name, ename);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                if (p == NULL || p->name == NULL || strcmp(p->name, "self") == 0)
                    continue;
                const char *pt = NULL;
                char surface_desc[256];
                snprintf(surface_desc, sizeof(surface_desc),
                    "enum method parameter '%s.%s(%s)'",
                    ename != NULL ? ename : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)",
                    p != NULL && p->name != NULL ? p->name : "(anonymous)");
                pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
                if (pt == NULL)
                    return;
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
