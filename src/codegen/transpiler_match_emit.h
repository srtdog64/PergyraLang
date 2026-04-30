/* Check if a match-case pattern is a destructor like Ok(x), Err(x),
 * or a tagged union variant like Circle(r), Rect(w, h) */
static bool
is_result_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    if (pat == NULL || pat->type != AST_CALL)
        return false;
    if (pat->data.call.callee == NULL || pat->data.call.callee->type != AST_IDENTIFIER)
        return false;
    const char *name = pat->data.call.callee->data.identifier.name;
    if (strcmp(name, "Ok") != 0 && strcmp(name, "Err") != 0)
        return false;
    *kind = name;
    if (pat->data.call.arg_count > 0
        && pat->data.call.arguments[0] != NULL
        && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
        *binding = pat->data.call.arguments[0]->data.identifier.name;
    } else {
        *binding = NULL;
    }
    return true;
}

static bool
is_option_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        *kind = "Some";
        if (pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

/* Check if pattern is a tagged union variant destructor: Circle(r), Rect(w, h), None */
static bool
is_enum_variant_destructor(ASTNode *pat, TranspilerCtx *ctx,
                           const char **variant_name_out,
                           const char **enum_name_out,
                           const char ***bindings_out,
                           ASTNode ***binding_types_out,
                           size_t *binding_count_out)
{
    const char *name = NULL;
    size_t argc = 0;

    if (pat == NULL) return false;

    if (pat->type == AST_CALL
        && pat->data.call.callee != NULL
        && pat->data.call.callee->type == AST_IDENTIFIER) {
        name = pat->data.call.callee->data.identifier.name;
        argc = pat->data.call.arg_count;
    } else if (pat->type == AST_IDENTIFIER) {
        name = pat->data.identifier.name;
        argc = 0;
    } else {
        return false;
    }

    if (name == NULL) return false;

    size_t type_count = 0;
    ASTNode **types = NULL;
    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL) {
        return false;
    }
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        bool has_data = false;
        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (stmt->data.enum_decl.variant_param_counts != NULL
                && stmt->data.enum_decl.variant_param_counts[j] > 0) {
                has_data = true;
                break;
            }
        }
        if (!has_data) continue;

        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (strcmp(stmt->data.enum_decl.variants[j], name) == 0) {
                *variant_name_out = name;
                *enum_name_out = stmt->data.enum_decl.name;
                static const char *bindings_buf[8];
                static ASTNode *binding_types_buf[8];
                *binding_count_out = 0;
                for (size_t k = 0; k < argc && k < 8; k++) {
                    ASTNode *arg = pat->data.call.arguments[k];
                    if (arg != NULL && arg->type == AST_IDENTIFIER)
                        bindings_buf[k] = arg->data.identifier.name;
                    else
                        bindings_buf[k] = NULL;
                    if (stmt->data.enum_decl.variant_params != NULL
                        && stmt->data.enum_decl.variant_params[j] != NULL
                        && stmt->data.enum_decl.variant_param_counts != NULL
                        && k < stmt->data.enum_decl.variant_param_counts[j]) {
                        binding_types_buf[k] = stmt->data.enum_decl.variant_params[j][k];
                    } else {
                        binding_types_buf[k] = NULL;
                    }
                    (*binding_count_out)++;
                }
                *bindings_out = bindings_buf;
                if (binding_types_out != NULL)
                    *binding_types_out = binding_types_buf;
                return true;
            }
        }
    }
    return false;
}

void
emit_match_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *subj = emit_expression(node->data.match_stmt.subject, ctx);
    int tmp_id = ctx->tmp_counter++;
    const char *subject_type = infer_expression_type_name(ctx, node->data.match_stmt.subject);
    const char *subject_c_type;
    bool subject_is_option = subject_type != NULL && strncmp(subject_type, "Option<", 7) == 0;

    if (subject_type == NULL || subject_type[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C match lowering requires a concrete subject type; implicit Int match fallback is disabled");
        subject_type = "Unknown";
    }
    subject_c_type = pergyra_type_to_c(subject_type);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "%s __match_%d = %s;\n", subject_c_type, tmp_id, subj);
    free(subj);

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        const char *kind = NULL, *binding = NULL;
        bool option_case = subject_is_option
            && is_option_destructor(mc->data.match_case.pattern, &kind, &binding);

        write_indent(ctx);

        if (mc->data.match_case.patterns != NULL
            && mc->data.match_case.pattern_count > 1) {
            if (i == 0)
                codebuf_write(ctx->out, "if (");
            else
                codebuf_write(ctx->out, "else if (");
            for (size_t p = 0; p < mc->data.match_case.pattern_count; p++) {
                char *pat = emit_expression(mc->data.match_case.patterns[p], ctx);
                if (p > 0)
                    codebuf_write(ctx->out, " || ");
                codebuf_write(ctx->out, "__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        } else if (option_case) {
            const char *tag_val = (strcmp(kind, "Some") == 0)
                ? "PgyOptionSome" : "PgyOptionNone";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else if (is_result_destructor(mc->data.match_case.pattern, &kind, &binding)) {
            const char *tag_val = (strcmp(kind, "Ok") == 0)
                ? "PgyResultOk" : "PgyResultErr";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else {
            const char *vname = NULL, *ename = NULL;
            const char **bindings = NULL;
            ASTNode **binding_types = NULL;
            size_t bind_count = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vname, &ename, &bindings,
                                            &binding_types, &bind_count)) {
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                else
                    codebuf_write(ctx->out, "else if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                kind = vname;
            } else {
                char *pat = emit_expression(mc->data.match_case.pattern, ctx);
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d == %s", tmp_id, pat);
                else
                    codebuf_write(ctx->out, "else if (__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        }

        if (mc->data.match_case.guard != NULL) {
            char *guard = emit_expression(mc->data.match_case.guard, ctx);
            codebuf_write(ctx->out, " && %s", guard);
            free(guard);
        }
        codebuf_write(ctx->out, ")\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;

        if (binding != NULL && kind != NULL) {
            write_indent(ctx);
            if (strcmp(kind, "Some") == 0) {
                const char *inner = subject_is_option
                    ? slot_inner_type_name(subject_type) : NULL;
                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "C match lowering cannot bind Some(%s) without Option<T> subject type",
                        binding);
                    inner = "Unknown";
                }
                codebuf_write(ctx->out, "%s %s = __match_%d.value;\n",
                    pergyra_type_to_c(inner), binding, tmp_id);
            } else if (strcmp(kind, "Ok") == 0) {
                const char *ok_type = strncmp(subject_type, "Result<", 7) == 0
                    ? constructed_arg_name_at(subject_type, 0) : NULL;
                if (ok_type == NULL || strcmp(ok_type, "Unknown") == 0) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "C match lowering cannot bind Ok(%s) without Result<T,E> subject type",
                        binding);
                    ok_type = "Unknown";
                }
                codebuf_write(ctx->out, "%s %s = __match_%d.ok;\n",
                    pergyra_type_to_c(ok_type), binding, tmp_id);
            } else if (strcmp(kind, "Err") == 0) {
                const char *err_type = "PgyError";
                if (strncmp(subject_type, "Result<", 7) != 0) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "C match lowering cannot bind Err(%s) without Result<T,E> subject type",
                        binding);
                    err_type = "Unknown";
                } else if (strchr(slot_inner_type_name(subject_type), ',') != NULL) {
                    err_type = constructed_arg_name_at(subject_type, 1);
                }
                codebuf_write(ctx->out, "%s %s = __match_%d.err;\n",
                    pergyra_type_to_c(err_type), binding, tmp_id);
            }
        }
        if (kind != NULL
            && strcmp(kind, "Some") != 0
            && strcmp(kind, "None") != 0
            && strcmp(kind, "Ok") != 0
            && strcmp(kind, "Err") != 0) {
            const char *vn2 = NULL, *en2 = NULL;
            const char **bs2 = NULL;
            ASTNode **bt2 = NULL;
            size_t bc2 = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vn2, &en2, &bs2, &bt2, &bc2)) {
                for (size_t b = 0; b < bc2; b++) {
                    if (bs2[b] != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "%s %s = __match_%d.%s._%zu;\n",
                            bt2 != NULL && bt2[b] != NULL
                                ? pergyra_ast_type_to_c(bt2[b]) : "int32_t",
                            bs2[b], tmp_id, vn2, b);
                    }
                }
            }
        }

        emit_block(mc->data.match_case.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    if (node->data.match_stmt.default_body != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(node->data.match_stmt.default_body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}
