#ifndef PERGYRA_TRANSPILER_MIR_DESTRUCTURE_EMIT_H
#define PERGYRA_TRANSPILER_MIR_DESTRUCTURE_EMIT_H

/* C backend MIR destructuring statement emission owner. */
static bool
transpiler_emit_mir_let_destructure_stmt(CodeBuf *buf,
                                         const MIRBasicBlock *block,
                                         const ASTNode *stmt,
                                         TranspilerCtx *ctx,
                                         TranspilerSSANameMap *ssa_map_out,
                                         char *reason,
                                         size_t reason_cap)
{
    ASTNode *init;
    const char *init_type_name;
    const char *c_init_type;
    const char *elem_inner = NULL;
    const char *elem_c_type = NULL;

    if (buf == NULL || block == NULL || stmt == NULL || ctx == NULL
        || ssa_map_out == NULL || stmt->type != AST_LET_DESTRUCTURE) {
        return false;
    }

    init = stmt->data.let_destructure.initializer;
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.callee->data.identifier.name != NULL
        && stmt->data.let_destructure.name_count == 2) {
        const char *cname = init->data.call.callee->data.identifier.name;
        if (strcmp(cname, "ClaimSecureSlot") == 0) {
            const char *inner = NULL;
            const char *slot_name;
            const char *token_name;
            char typed_tok[64];
            char typed_slot[64];

            if (init->data.call.generic_args != NULL
                && init->data.call.generic_args->count > 0
                && init->data.call.generic_args->params[0] != NULL) {
                inner = transpiler_let_slot_inner_from_call_type_arg(init);
            }
            if (inner == NULL || inner[0] == '\0') {
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                        "ClaimSecureSlot destructuring requires concrete generic type");
                }
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: ClaimSecureSlot destructuring requires concrete SecureSlot<T> metadata");
                return false;
            }
            slot_name = stmt->data.let_destructure.names[0];
            token_name = stmt->data.let_destructure.names[1];
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "PgyToken_%s %s;\n", inner, token_name);
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf,
                "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s);\n",
                inner, slot_name, inner, token_name);
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "(void)%s;\n", slot_name);
            register_slot_var(ctx, slot_name, inner, true, false);
            set_slot_token_name(ctx, slot_name, token_name);
            snprintf(typed_tok, sizeof(typed_tok), "Token<%s>", inner);
            register_typed_var(ctx, token_name, typed_tok);
            snprintf(typed_slot, sizeof(typed_slot), "SecureSlot<%s>", inner);
            register_typed_var(ctx, slot_name, typed_slot);
            transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
            transpiler_ssa_name_map_set(ssa_map_out, token_name, token_name);
            return true;
        }
    }

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.callee->data.identifier.name != NULL
        && stmt->data.let_destructure.name_count == 1
        && strcmp(init->data.call.callee->data.identifier.name,
                  "ClaimSlot") == 0) {
        const char *inner = NULL;
        const char *slot_name;
        char typed_slot[64];

        if (init->data.call.generic_args != NULL
            && init->data.call.generic_args->count > 0
            && init->data.call.generic_args->params[0] != NULL) {
            inner = transpiler_let_slot_inner_from_call_type_arg(init);
        }
        if (inner == NULL || inner[0] == '\0') {
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                    "ClaimSlot destructuring requires concrete generic type");
            }
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: ClaimSlot destructuring requires concrete Slot<T> metadata");
            return false;
        }
        slot_name = stmt->data.let_destructure.names[0];
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "PgySlot_%s %s = pgy_claim_%s();\n",
                      inner, slot_name, inner);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "(void)%s;\n", slot_name);
        register_slot_var(ctx, slot_name, inner, false, false);
        snprintf(typed_slot, sizeof(typed_slot), "Slot<%s>", inner);
        register_typed_var(ctx, slot_name, typed_slot);
        transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
        return true;
    }

    init_type_name = infer_expression_type_name(ctx, init);
    if ((init_type_name == NULL || strcmp(init_type_name, "Unknown") == 0)
        && init != NULL
        && init->type == AST_IDENTIFIER
        && init->data.identifier.name != NULL
        && ctx->current_func_decl != NULL) {
        const char *resolved = transpiler_find_local_type_name(
            ctx, ctx->current_func_decl, init->data.identifier.name);
        if (resolved != NULL)
            init_type_name = resolved;
    }
    c_init_type = pergyra_type_to_c(init_type_name);

    if (init_type_name != NULL && init_type_name[0] == '(') {
        char elem_names[8][64];
        size_t arity = 0;
        char *rhs_t;
        int tmp_id;

        {
            size_t ti = 1;
            size_t tlen = strlen(init_type_name);
            while (ti < tlen && init_type_name[ti] != ')' && arity < 8) {
                size_t eo = 0;
                int depth = 0;
                while (ti < tlen
                       && (init_type_name[ti] == ' '
                           || init_type_name[ti] == '\t')) {
                    ti++;
                }
                while (ti < tlen && eo + 1 < sizeof(elem_names[0])) {
                    char c = init_type_name[ti];
                    if (depth == 0 && (c == ',' || c == ')'))
                        break;
                    if (c == '<' || c == '(')
                        depth++;
                    if (c == '>' || c == ')')
                        depth--;
                    elem_names[arity][eo++] = c;
                    ti++;
                }
                elem_names[arity][eo] = '\0';
                while (eo > 0
                       && (elem_names[arity][eo - 1] == ' '
                           || elem_names[arity][eo - 1] == '\t')) {
                    elem_names[arity][--eo] = '\0';
                }
                arity++;
                if (ti < tlen && init_type_name[ti] == ',')
                    ti++;
            }
        }
        if (arity != stmt->data.let_destructure.name_count) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                    (unsigned long long) stmt->data.let_destructure.name_count,
                    (unsigned long long) arity);
            }
            return false;
        }
        rhs_t = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
        if (rhs_t == NULL)
            return false;
        tmp_id = ++ctx->tmp_counter;
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                      c_init_type, tmp_id, rhs_t);
        free(rhs_t);
        for (size_t dn = 0; dn < stmt->data.let_destructure.name_count; dn++) {
            const char *bname = stmt->data.let_destructure.names[dn];
            char ssa_versioned_tmp[192];
            char *ssa_versioned;
            char *ssa_lhs;

            if (bname == NULL)
                continue;
            snprintf(ssa_versioned_tmp, sizeof(ssa_versioned_tmp),
                     "%s.1", bname);
            ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
            if (ssa_versioned == NULL)
                return false;
            ssa_lhs = transpiler_render_ssa_name(ctx, ssa_versioned);
            if (ssa_lhs == NULL) {
                free(ssa_versioned);
                continue;
            }
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = _pgy_destr_%d.f%zu;\n",
                          ssa_lhs, tmp_id, dn);
            free(ssa_lhs);
            register_typed_var(ctx, bname, elem_names[dn]);
            transpiler_ssa_name_map_set(ssa_map_out, bname, ssa_versioned);
        }
        return true;
    }

    if (init_type_name != NULL
        && (strncmp(init_type_name, "Array<", 6) == 0
            || strncmp(init_type_name, "Slice<", 6) == 0)) {
        elem_inner = slot_inner_type_name(init_type_name);
        elem_c_type = pergyra_type_to_c(elem_inner);
    }
    if (c_init_type == NULL || elem_c_type == NULL || elem_inner == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                init_type_name != NULL ? init_type_name : "(unknown)");
        }
        return false;
    }

    {
        char *rhs = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
        int tmp_id;

        if (rhs == NULL) {
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %llu emission failed: unable to render destructuring initializer",
                         (unsigned long long) block->id);
            }
            return false;
        }
        tmp_id = ++ctx->tmp_counter;
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                      c_init_type, tmp_id, rhs);
        free(rhs);
        for (size_t dn = 0; dn < stmt->data.let_destructure.name_count; dn++) {
            const char *bname = stmt->data.let_destructure.names[dn];
            char ssa_versioned_tmp[192];
            char *ssa_versioned;
            char *ssa_lhs;

            if (bname == NULL)
                continue;
            snprintf(ssa_versioned_tmp, sizeof(ssa_versioned_tmp),
                     "%s.1", bname);
            ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
            if (ssa_versioned == NULL)
                return false;
            ssa_lhs = transpiler_render_ssa_name(ctx, ssa_versioned);
            if (ssa_lhs == NULL) {
                free(ssa_versioned);
                continue;
            }
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = _pgy_destr_%d.data[%zu];\n",
                          ssa_lhs, tmp_id, dn);
            free(ssa_lhs);
            register_typed_var(ctx, bname, elem_inner);
            transpiler_ssa_name_map_set(ssa_map_out, bname, ssa_versioned);
        }
    }
    return true;
}

#endif /* PERGYRA_TRANSPILER_MIR_DESTRUCTURE_EMIT_H */
