                    }
                }
            }
        }
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}

void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    bool opened_body = false;
    if (ctx != NULL && ctx->mir != NULL) {
        char reason[256];
        if (transpiler_can_emit_function_from_mir_with_reason(
                ctx, node, &mir_routine, reason, sizeof(reason))) {
            emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        } else if (ctx->backend_error == NULL) {
            const char *func_name = (node != NULL && node->type == AST_FUNC_DECL
                                     && node->data.func_decl.name != NULL)
                ? node->data.func_decl.name
                : "(anonymous)";
            /* Classify the failure by inspecting the reason prefix so
             * downstream consumers can route on a stable code. The reason
             * text itself is preserved as the human-readable message. */
            const char *reason_code  = NULL;
            const char *reason_cause = NULL;
            const char *reason_fix   = NULL;
            if (strstr(reason, "unresolved identifier") != NULL) {
                reason_code  = PGY_CODE_MIR_UNRESOLVED_LOCAL;
                reason_cause = PGY_CAUSE_MIR_LOCAL_UNRESOLVED;
                reason_fix   = PGY_FIX_REPORT_COMPILER_BUG;
            } else if (strstr(reason, "topology") != NULL
                       || strstr(reason, "no routine") != NULL
                       || strstr(reason, "no declaration AST") != NULL) {
                reason_code  = PGY_CODE_MIR_TOPOLOGY_INVALID;
                reason_cause = PGY_CAUSE_MIR_TOPOLOGY_INVALID;
                reason_fix   = PGY_FIX_REPORT_COMPILER_BUG;
            } else if (strstr(reason, "unsupported MIR signature") != NULL
                       || strstr(reason, "wrong MIR kind") != NULL) {
                reason_code  = PGY_CODE_MIR_SIGNATURE_UNSUPPORTED;
                reason_cause = PGY_CAUSE_MIR_SIGNATURE_UNSUPPORTED;
                reason_fix   = PGY_FIX_SIMPLIFY_FUNCTION_SIGNATURE;
            } else if (strstr(reason, "too many MIR SSA locals") != NULL) {
                reason_code  = PGY_CODE_MIR_SSA_LIMIT;
                reason_cause = PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED;
                reason_fix   = PGY_FIX_REFACTOR_OR_RAISE_LIMIT;
            }
            transpiler_set_backend_error_with_hints(ctx, reason_code,
                reason_cause, reason_fix,
                "MIR-only C path missing routine for function '%s': %s",
                func_name,
                reason[0] != '\0' ? reason : "unknown reason");
        }
        return;
    }
    if (transpiler_can_emit_function_from_mir(ctx, node, &mir_routine)) {
        emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        return;
    }
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerMirEmitState saved_emit_state;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    ctx->out = buf;
    g_type_render_ctx = ctx;
    if (node->data.func_decl.return_type != NULL) {
        {
            char *rendered = render_type_name(node->data.func_decl.return_type);
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", rendered);
            free(rendered);
        }
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "function parameter '%s' of '%s'",
            p != NULL && p->name != NULL ? p->name : "(anonymous)",
            name != NULL ? name : "(anonymous)");
        pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
        if (pt == NULL)
            goto emit_func_decl_named_fail;
        if (i > 0) codebuf_write(params_sig, ", ");
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
            /* Subject parameters are passed by pointer (reference semantics) */
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    opened_body = true;
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name(p->type);
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            /* Mark pointer-self parameters (subject/relation/effect/zone/world) for -> access */
            if (p->name != NULL && strcmp(p->name, "self") != 0
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }
    if (node->data.func_decl.body != NULL)
        emit_block(node->data.func_decl.body, ctx);
    if (ctx->backend_error != NULL)
        goto emit_func_decl_named_fail;

    /* If the function returns a non-void type and the body does not end
     * with an explicit return (e.g. ends with a match/if/while), emit a
     * fallback return to silence C compiler warnings about missing return. */
    if (strcmp(ctx->current_return_type, "Void") != 0
        && strcmp(ctx->current_return_type, "void") != 0) {
        const char *ret_type = ctx->current_return_type;
        /* Simple zero-initialised return for non-void functions that may
         * fall off the end (e.g. match without default, or if/else with
         * only one branch returning). */
        write_indent(ctx);
        if (strcmp(ret_type, "Bool") == 0) {
            codebuf_write(ctx->out, "return false;  /* fallback return */\n");
        } else if (strcmp(ret_type, "String") == 0
                   || strncmp(ret_type, "char *", 6) == 0) {
            codebuf_write(ctx->out, "return NULL;  /* fallback return */\n");
        } else if (strncmp(ret_type, "Option<", 7) == 0) {
            const char *inner = slot_inner_type_name(ret_type);
            codebuf_write(ctx->out, "return None_%s();  /* fallback return */\n", inner);
        } else if (strncmp(ret_type, "Result<", 7) == 0) {
            const char *inner = slot_inner_type_name(ret_type);
            codebuf_write(ctx->out, "return Err_%s(0);  /* fallback return */\n", inner);
        } else {
            codebuf_write(ctx->out, "return 0;  /* fallback return */\n");
        }
    }

    ctx->indent--;

    codebuf_write(ctx->out, "}\n");
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
    return;

emit_func_decl_named_fail:
    if (opened_body)
        ctx->indent--;
    free(header_decl);
    if (params_sig != NULL)
        codebuf_destroy(params_sig);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}

void
emit_func_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_func_decl_named(node, node->data.func_decl.name, ctx->out, ctx);
}

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  node->data.extern_block.abi != NULL
                    ? node->data.extern_block.abi : "");

    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = decl->data.func_decl.name;
        const char *ret_type = "void";
        if (decl->data.func_decl.return_type != NULL) {
            ret_type = pergyra_ast_type_to_c(decl->data.func_decl.return_type);
        }

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < decl->data.func_decl.param_count; j++) {
            FuncParam *p = decl->data.func_decl.params[j];
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "extern parameter '%s' of '%s'",
                p != NULL && p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
            if (j > 0) {
                codebuf_write(ctx->out, ", ");
            }
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }

        codebuf_write(ctx->out, ");\n");
    }
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
 * Generic class monomorphization
 * ----------------------------------------------------------------- */

static bool
class_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_CLASS_DECL
        && node->data.class_decl.generic_params != NULL
        && node->data.class_decl.generic_params->count > 0;
}

/* Ensure a monomorphized specialization of a generic class exists.
 * Returns the specialized name (e.g. "Node_Int") that should be used
 * as the C struct type name.  The struct + methods are emitted into
 * ctx->helpers on first invocation.
 *
 * `ann` is the AST_TYPE node for the annotation (e.g. Node<Int>).
 * We extract generic_args from it and match them to class_decl's
 * generic_params to build the bindings. */
static const char *
ensure_generic_class_specialization(TranspilerCtx *ctx,
                                     ASTNode *class_decl,
                                     ASTNode *ann)
{
    GenericParams *gp = class_decl->data.class_decl.generic_params;
    GenericParams *ga = ann->data.type.generic_args;
    bool has_effective_args = false;

    if (gp == NULL)
        return class_decl->data.class_decl.name;

    /* Build specialized name: ClassName_Arg1_Arg2 */
    CodeBuf *nbuf = codebuf_create();
    codebuf_write(nbuf, "%s", class_decl->data.class_decl.name);
    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *formal = gp->params[i];
        GenericParam *garg = (ga != NULL && i < ga->count) ? ga->params[i] : NULL;
        const char *effective_name = NULL;

        if (garg != NULL && garg->name != NULL)
            effective_name = garg->name;
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            effective_name = garg->constraint->data.type.name;
        else if (formal != NULL && formal->default_type != NULL
                 && formal->default_type->type == AST_TYPE
                 && formal->default_type->data.type.name != NULL)
            effective_name = formal->default_type->data.type.name;
        else
            return class_decl->data.class_decl.name;

        has_effective_args = true;
        codebuf_write(nbuf, "_");
        append_mangled_type_name(nbuf, effective_name);
    }

    if (!has_effective_args) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    /* Check if already emitted */
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, nbuf->data) == 0) {
            const char *result = ctx->generic_class_specs[i].specialized_name;
            codebuf_destroy(nbuf);
            return result;
        }
    }

    /* Register new specialization */
    if (ctx->generic_class_spec_count >= MAX_GENERIC_CLASS_SPECIALIZATIONS) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    GenericClassSpecEntry *entry = &ctx->generic_class_specs[ctx->generic_class_spec_count++];
    entry->class_decl = class_decl;
    snprintf(entry->specialized_name, sizeof(entry->specialized_name), "%s", nbuf->data);
    entry->emitted = true;
    const char *spec_name = entry->specialized_name;

    /* Build bindings: T -> Int, U -> String, etc. */
    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *formal = gp->params[i];
        GenericParam *garg = (ga != NULL && i < ga->count) ? ga->params[i] : NULL;
        const char *effective_name = NULL;

        if (ctx->generic_binding_count >= MAX_GENERIC_BINDINGS)
            break;
        GenericBindingEntry *b = &ctx->generic_bindings[ctx->generic_binding_count++];
        snprintf(b->name, sizeof(b->name), "%s",
                 gp->params[i] != NULL ? gp->params[i]->name : "T");
        /* The concrete type name comes from the annotation's generic_args.
         * ga->params[i] is a GenericParam whose 'name' field holds the
         * actual type name (e.g. "Int") when used as a type argument. */
        if (garg != NULL && garg->name != NULL)
            effective_name = garg->name;
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            effective_name = garg->constraint->data.type.name;
        else if (formal != NULL && formal->default_type != NULL
                 && formal->default_type->type == AST_TYPE
                 && formal->default_type->data.type.name != NULL)
            effective_name = formal->default_type->data.type.name;

        if (effective_name != NULL)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     effective_name);
        else {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot resolve generic class binding '%s' for specialization '%s'",
                    gp->params[i] != NULL && gp->params[i]->name != NULL
                        ? gp->params[i]->name
                        : "(anonymous)",
                    class_decl != NULL && class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name
                        : "(anonymous)");
            }
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }

        entry->bindings[i] = *b;
    }
    entry->binding_count = gp->count;

    /* Set g_type_render_ctx so pergyra_ast_type_to_c resolves T → Int */
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    g_type_render_ctx = ctx;

    /* Emit struct typedef into helpers buffer */
    codebuf_write(ctx->helpers, "\ntypedef struct %s\n{\n", spec_name);
    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *f = class_decl->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "generic class field '%s.%s'",
            spec_name != NULL ? spec_name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL) {
            g_type_render_ctx = saved_render_ctx;
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, f->name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    /* Container types — suppress unused function warnings for Slot/Box helpers */
    codebuf_write(ctx->helpers,
        "\n#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        spec_name, spec_name,
        spec_name, spec_name,
        spec_name, spec_name);

    /* Methods */
    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        emit_hosted_method_forward_decl_named(spec_name, method, use_self_cell,
                                              ctx->helpers, ctx);
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        const MIRRoutine *mir_method = transpiler_find_mir_method(ctx,
            class_decl->data.class_decl.name, method);
        if (method->type != AST_FUNC_DECL)
            continue;

        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing routine for generic class method '%s.%s' specialization '%s'",
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name
                        : "(anonymous-class)",
                    method->data.func_decl.name != NULL
                        ? method->data.func_decl.name
                        : "(anonymous)",
                    spec_name != NULL ? spec_name : "(anonymous-specialization)");
            }
            g_type_render_ctx = saved_render_ctx;
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }

        if (mir_method != NULL) {
            char emitted_name[256];
            snprintf(emitted_name, sizeof(emitted_name), "%s_%s", spec_name,
                method->data.func_decl.name != NULL
                    ? method->data.func_decl.name
                    : "(anonymous)");
            emit_func_decl_from_mir_named(method, mir_method, emitted_name,
                ctx->helpers, ctx);
            continue;
        }

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s *self",
                          ret_type, spec_name, method_name, spec_name);
        } else {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s self",
                          ret_type, spec_name, method_name, spec_name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "generic class method parameter '%s.%s(%s)'",
                spec_name != NULL ? spec_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL) {
                g_type_render_ctx = saved_render_ctx;
                ctx->generic_binding_count = saved_binding_count;
                codebuf_destroy(nbuf);
                return NULL;
            }
            codebuf_write(ctx->helpers, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->helpers, ")\n{\n");

        transpiler_emit_host_method_body_local(
            ctx, class_decl, spec_name, method, ctx->helpers, false);

        codebuf_write(ctx->helpers, "}\n");
    }

    /* Restore bindings and render context */
    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    codebuf_destroy(nbuf);

    return spec_name;
}

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    /* Generic classes are emitted lazily when first used (monomorphized). */
    if (class_has_generic_params(node))
        return;

    const char *name = node->data.class_decl.name;

    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        if (f != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out, f->type);
    }
    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            node->data.class_decl.methods[i]);

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "class field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Auto-generate Slot and Result container types for this struct.
     * This allows Slot<MyStruct>, Result<MyStruct> in user code. */
    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for %s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods become free functions over a subject self-cell or class value. */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        emit_hosted_method_forward_decl_named(name, method, use_self_cell,
                                              ctx->out, ctx);
    }

    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        const MIRRoutine *mir_method = transpiler_find_mir_method(ctx, name, method);
        if (method->type != AST_FUNC_DECL)
            continue;
        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing routine for class method '%s.%s'",
                    name != NULL ? name : "(anonymous-class)",
                    method->data.func_decl.name != NULL
                        ? method->data.func_decl.name
                        : "(anonymous)");
            }
            return;
        }
        if (mir_method != NULL) {
            char emitted_name[256];
            snprintf(emitted_name, sizeof(emitted_name), "%s_%s", name,
                method->data.func_decl.name);
            emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
            continue;
        }

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                          ret_type, name, method_name, name);
        } else {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, name, method_name, name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "class method parameter '%s.%s(%s)'",
                name != NULL ? name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
            {
                char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
                bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
                if (subj_param)
                    codebuf_write(ctx->out, ", %s *%s", pt, p->name);
                else
                    codebuf_write(ctx->out, ", %s %s", pt, p->name);
                free(ptn);
            }
        }
        codebuf_write(ctx->out, ")\n{\n");

        transpiler_emit_host_method_body_local(
            ctx,
            transpiler_find_decl_in_inventory_local(ctx, AST_CLASS_DECL, name),
            name,
            method,
            NULL,
            true);

        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * with slot<T> as s { }
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;

    const char *inner = NULL;
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;
    if (inner == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit with-slot alias '%s': missing explicit slot type",
                alias != NULL ? alias : "(anonymous)");
        }
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    /* Register alias in slot variable table */
    register_slot_var(ctx, alias, inner, is_secure, false);

    if (is_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyToken_%s %s_token;\n", inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            inner, alias, inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", alias);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            inner, alias, inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", alias);
    }

    if (node->data.with_stmt.body != NULL)
        emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    write_indent(ctx);
    if (is_secure) {
        codebuf_write(ctx->out,
            "pgy_secure_release_%s(&%s, &%s_token);\n",
            inner, alias, alias);
    } else {
        codebuf_write(ctx->out,
            "pgy_release_%s(&%s);\n", inner, alias);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                  saved_typed_count, -1);
}

/* -----------------------------------------------------------------
 * Parallel block
 * ----------------------------------------------------------------- */

/* Walk a function body looking for a top-level-or-nested `let NAME: T = ...`
 * matching base_name, and return the AST type node T (or NULL). Used by
 * parallel-capture emission to detect function-typed locals so the context
 * struct field can be declared as pointer-to-function-pointer instead of
 * plain `type *name`. */
static ASTNode *
transpiler_find_local_let_type_node(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_local_let_type_node(
                body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        return body->data.let_decl.type;
    }
    return NULL;
}

static const char *
transpiler_current_local_type_name(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || ctx->current_func_decl == NULL)
        return NULL;
    return transpiler_find_local_type_name(ctx, ctx->current_func_decl, name);
}

static bool
transpiler_parallel_capture_has_name(char names[MAX_SLOT_VARS][64],
                                     int count,
                                     const char *name)
{
    if (name == NULL)
        return false;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void
transpiler_parallel_add_capture_name(TranspilerCtx *ctx,
                                     const char *name,
                                     char slot_names[MAX_SLOT_VARS][64],
                                     int *slot_count,
                                     char typed_names[MAX_SLOT_VARS][64],
                                     int *typed_count)
{
    if (ctx == NULL || name == NULL || name[0] == '\0'
        || strcmp(name, "self") == 0) {
        return;
    }

    if (is_slot_var(ctx, name)) {
