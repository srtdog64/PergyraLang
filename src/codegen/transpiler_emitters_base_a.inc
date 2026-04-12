static int transpiler_find_loop_label_depth(const TranspilerCtx *ctx,
                                            const char *label);
static bool transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                              const ASTNode *expr,
                                              const TranspilerSSANameMap *ssa_map,
                                              const char *routine_name,
                                              char *reason,
                                              size_t reason_cap);
static bool transpiler_emit_mir_phi_copies(CodeBuf *buf,
                                           int indent,
                                           size_t pred_block_index,
                                           const MIRBasicBlock *pred_block,
                                           const MIRBasicBlock *target_block);
static bool transpiler_emit_mir_block_statements(CodeBuf *buf,
                                                 const ASTNode *func_decl,
                                                 const MIRRoutine *mir_routine,
                                                 const MIRBasicBlock *block,
                                                 TranspilerCtx *ctx,
                                                 TranspilerSSANameMap *out_ssa_map,
                                                 char *reason,
                                                 size_t reason_cap);
static bool transpiler_can_emit_function_from_mir(const TranspilerCtx *ctx,
                                                  const ASTNode *func_decl,
                                                  const MIRRoutine **mir_routine_out);
static bool transpiler_can_emit_function_from_mir_with_reason(const TranspilerCtx *ctx,
                                                             const ASTNode *func_decl,
                                                             const MIRRoutine **mir_routine_out,
                                                             char *reason,
                                                             size_t reason_cap);
static bool transpiler_can_emit_intent_cleanup_from_mir(const TranspilerCtx *ctx,
                                                        const ASTNode *intent_decl,
                                                        const MIRRoutine **mir_routine_out);
static bool transpiler_can_emit_intent_cleanup_from_mir_with_reason(const TranspilerCtx *ctx,
                                                                  const ASTNode *intent_decl,
                                                                  const MIRRoutine **mir_routine_out,
                                                                  char *reason,
                                                                  size_t reason_cap);
static bool transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                              CodeBuf *out,
                                              int indent,
                                              const MIRInstruction *inst,
                                              const char *handle_expr,
                                              bool cleanup_hook);
static bool transpiler_validate_mir_emission_block_shape(const MIRBasicBlock *block,
                                                        const char *routine_name,
                                                        bool require_cleanup,
                                                        char *reason,
                                                        size_t reason_cap);
static void emit_func_decl_from_mir_named(ASTNode *node,
                                          const MIRRoutine *mir_routine,
                                          const char *emitted_name,
                                          CodeBuf *buf,
                                          TranspilerCtx *ctx);
/* Forward declarations for generic class monomorphization */
static bool class_has_generic_params(ASTNode *node);
static const char *ensure_generic_class_specialization(
    TranspilerCtx *ctx, ASTNode *class_decl, ASTNode *ann);

/* -----------------------------------------------------------------
 * Let declaration emitter
 * ----------------------------------------------------------------- */

static const char *
transpiler_require_ast_c_type(TranspilerCtx *ctx,
                              ASTNode *type_ast,
                              const char *surface_desc)
{
    const char *c_type;
    if (type_ast == NULL) {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit %s in C backend: missing explicit type",
                surface_desc != NULL ? surface_desc : "declaration");
        }
        return NULL;
    }
    c_type = pergyra_ast_type_to_c(type_ast);
    if (c_type == NULL || c_type[0] == '\0' || strcmp(c_type, "Unknown") == 0) {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot lower %s to a concrete C type",
                surface_desc != NULL ? surface_desc : "declaration");
        }
        return NULL;
    }
    return c_type;
}

static const char *
transpiler_require_type_name_c_type(TranspilerCtx *ctx,
                                    const char *type_name,
                                    const char *surface_desc)
{
    const char *c_type;
    if (type_name == NULL || type_name[0] == '\0') {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit %s in C backend: missing concrete type name",
                surface_desc != NULL ? surface_desc : "declaration");
        }
        return NULL;
    }
    c_type = pergyra_type_to_c(type_name);
    if (c_type == NULL || c_type[0] == '\0' || strcmp(c_type, "Unknown") == 0) {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot lower %s type '%s' to a concrete C type",
                surface_desc != NULL ? surface_desc : "declaration",
                type_name);
        }
        return NULL;
    }
    return c_type;
}

void
emit_let_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;
    ASTNode    *resolved_ann = resolve_type_alias_target(ctx, ann);
    char       *ann_type_name = ann != NULL ? render_type_name(ann) : NULL;
    ASTNode    *callable_type = NULL;
    ASTNode    *callable_decl = NULL;

    /* Generic class monomorphization trigger:
     * If the annotation is a user-defined generic class (e.g. Node<Int>),
     * monomorphize the class and replace ann_type_name with the
     * specialized name (e.g. Node_Int). */
    const char *generic_class_spec_name = NULL;

    if (node->data.let_decl.is_alias) {
        register_alias_var(ctx, name, init);
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
        } else if (init != NULL) {
            const char *inferred = infer_expression_type_name(ctx, init);
            if (inferred != NULL)
                register_typed_var(ctx, name, inferred);
        }
        free(ann_type_name);
        return;
    }

    if (ann != NULL && ann->type == AST_TYPE
        && ann->data.type.generic_args != NULL
        && ann->data.type.generic_args->count > 0
        && ann->data.type.name != NULL) {
        ASTNode *gc_decl = find_class_decl(ctx, ann->data.type.name);
        if (gc_decl != NULL && class_has_generic_params(gc_decl)) {
            generic_class_spec_name =
                ensure_generic_class_specialization(ctx, gc_decl, ann);
            if (generic_class_spec_name == NULL)
                return;
            /* Replace ann_type_name with the specialized name */
            free(ann_type_name);
            ann_type_name = pergyra_strdup(generic_class_spec_name);
        }
    }

    if (ann != NULL && ann->type == AST_EVENT_HANDLER_TYPE) {
        callable_type = ann;
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && init->data.call.callee->data.identifier.name != NULL) {
        ASTNode *decl = find_function_decl(ctx, init->data.call.callee->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && decl->data.func_decl.return_type != NULL
            && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
            callable_type = decl->data.func_decl.return_type;
        }
    } else if (init != NULL && init->type == AST_IDENTIFIER
               && init->data.identifier.name != NULL) {
        ASTNode *decl = find_function_decl(ctx, init->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            callable_decl = decl;
        }
    }

    /* Detect ClaimSlot / ClaimSecureSlot */
    bool is_slot        = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;
    const char *slot_inner = NULL;

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "ClaimSlot") == 0) {
            is_slot   = true;
        } else if (strcmp(callee_name, "ClaimSecureSlot") == 0) {
            is_slot        = true;
            is_secure_slot = true;
        } else if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
            is_device_slot = true;
        }
    }

    if (is_slot) {
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }
        if (slot_inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit slot claim for '%s': missing explicit Slot<T>/SecureSlot<T> annotation",
                    name != NULL ? name : "(anonymous)");
            }
            free(ann_type_name);
            return;
        }

        register_slot_var(ctx, name, slot_inner, is_secure_slot, false);

        write_indent(ctx);
        if (is_secure_slot) {
            codebuf_write(ctx->out,
                "PgyToken_%s %s_token;\n", slot_inner, name);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                slot_inner, name, slot_inner, name);
            write_indent(ctx);
            codebuf_write(ctx->out, "(void)%s;\n", name);
        } else {
            codebuf_write(ctx->out,
                "PgySlot_%s %s = pgy_claim_%s();\n",
                slot_inner, name, slot_inner);
            write_indent(ctx);
            codebuf_write(ctx->out, "(void)%s;\n", name);
        }
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (is_device_slot) {
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }
        if (slot_inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit device slot claim for '%s': missing explicit DeviceSlot<T> annotation",
                    name != NULL ? name : "(anonymous)");
            }
            free(ann_type_name);
            return;
        }

        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyDeviceSlot_%s %s = pgy_claim_device_%s();\n",
            slot_inner, name, slot_inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", name);
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        else {
            char *device_type = strdup_fmt("DeviceSlot<%s>", slot_inner);
            register_typed_var(ctx, name, device_type);
            free(device_type);
        }
        free(ann_type_name);
        return;
    }

    if (ann != NULL && ann->type == AST_TYPE && ann->data.type.name != NULL) {
        const char *ann_name = ann->data.type.name;
        if (ann_name != NULL && ann_name[0] != '\0'
        && (strcmp(ann_name, "ReadView") == 0
            || strncmp(ann_name, "ReadView<", 9) == 0
            || strcmp(ann_name, "WriteView") == 0
            || strncmp(ann_name, "WriteView<", 10) == 0
            || strcmp(ann_name, "MoveToken") == 0
            || strncmp(ann_name, "MoveToken<", 10) == 0)
        && init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.arg_count >= 1
        && init->data.call.arguments[0] != NULL
        && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        const char *source_name = init->data.call.arguments[0]->data.identifier.name;
        bool is_view_decl = (strcmp(callee_name, "ViewRead") == 0
                          || strcmp(callee_name, "ViewWrite") == 0);
        bool is_move_decl = strcmp(callee_name, "Move") == 0;
        if (is_view_decl || is_move_decl) {
            const char *inner = NULL;
            bool source_secure = lookup_slot_is_secure(ctx, source_name);
            const char *ctype = NULL;
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                inner = ann->data.type.generic_args->params[0]->name;
            } else {
                inner = slot_inner_type_name(ann_name);
            }
            if (inner == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot emit %s declaration for '%s': missing explicit inner type",
                        is_move_decl ? "move-token" : "view",
                        name != NULL ? name : "(anonymous)");
                }
                free(ann_type_name);
                return;
            }
            if (source_secure) {
                char secure_name[128];
                snprintf(secure_name, sizeof(secure_name), "SecureSlot<%s>", inner);
                ctype = pergyra_type_to_c(secure_name);
            } else {
                char slot_name_buf[128];
                snprintf(slot_name_buf, sizeof(slot_name_buf), "Slot<%s>", inner);
                ctype = pergyra_type_to_c(slot_name_buf);
            }
            if (ctype == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot lower %s declaration '%s' with inner type '%s' to a concrete slot type",
                        is_move_decl ? "move-token" : "view",
                        name != NULL ? name : "(anonymous)",
                        inner);
                }
                free(ann_type_name);
                return;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = %s;\n", ctype, name, source_name);
            if (source_secure) {
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                    inner, name, source_name);
            }
            register_view_like_var(ctx, name,
                ann_type_name != NULL ? ann_type_name : ann_name,
                source_name, source_secure, is_move_decl);
            if (is_move_decl) {
                for (int i = 0; i < ctx->slot_var_count; i++) {
                    if (strcmp(ctx->slot_vars[i].name, source_name) == 0) {
                        ctx->slot_vars[i].released = true;
                        break;
                    }
                }
            }
            free(ann_type_name);
            return;
        }
    }
    }

    /* Slot sugar: let x: Slot<Int> = 42 → auto Claim + Write */
    if (!is_slot && ann != NULL && ann->type == AST_TYPE) {
        const char *ann_name = ann->data.type.name;
        bool is_slot_sugar = false;
        bool sugar_secure = false;
        if (ann_name != NULL) {
            if (strcmp(ann_name, "Slot") == 0 || strncmp(ann_name, "Slot<", 5) == 0) {
                is_slot_sugar = true;
            } else if (strcmp(ann_name, "SecureSlot") == 0 || strncmp(ann_name, "SecureSlot<", 11) == 0) {
                is_slot_sugar = true;
                sugar_secure = true;
            }
        }
        if (is_slot_sugar) {
            const char *sugar_inner = NULL;
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                sugar_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                sugar_inner = slot_inner_type_name(ann_name);
            }
            if (sugar_inner == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot emit slot sugar declaration for '%s': missing explicit Slot<T>/SecureSlot<T> inner type",
                        name != NULL ? name : "(anonymous)");
                }
                free(ann_type_name);
                return;
            }

            if (init != NULL && init->type == AST_IDENTIFIER) {
                TypedVarEntry *move_entry = lookup_typed_entry(ctx, init->data.identifier.name);
                if (move_entry != NULL && move_entry->is_move_token) {
                    write_indent(ctx);
                    if (sugar_secure) {
                        codebuf_write(ctx->out, "PgySecureSlot_%s %s = %s;\n",
                            sugar_inner, name, init->data.identifier.name);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                            sugar_inner, name, init->data.identifier.name);
                    } else {
                        codebuf_write(ctx->out, "PgySlot_%s %s = %s;\n",
                            sugar_inner, name, init->data.identifier.name);
                    }
                    register_slot_var(ctx, name, sugar_inner, sugar_secure, false);
                    if (ann_type_name != NULL)
                        register_typed_var(ctx, name, ann_type_name);
                    free(ann_type_name);
                    return;
                }
            }

            register_slot_var(ctx, name, sugar_inner, sugar_secure, false);

            write_indent(ctx);
            if (sugar_secure) {
                codebuf_write(ctx->out,
                    "PgyToken_%s %s_token;\n", sugar_inner, name);
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                    sugar_inner, name, sugar_inner, name);
            } else {
                codebuf_write(ctx->out,
                    "PgySlot_%s %s = pgy_claim_%s();\n",
                    sugar_inner, name, sugar_inner);
            }

            /* Auto Write the initializer value */
            if (init != NULL) {
                char *init_expr = emit_expression(init, ctx);
                write_indent(ctx);
                if (sugar_secure) {
                    codebuf_write(ctx->out,
                        "pgy_secure_write_%s(&%s, %s, &%s_token);\n",
                        sugar_inner, name, init_expr, name);
                } else {
                    codebuf_write(ctx->out,
                        "pgy_write_%s(&%s, %s);\n",
                        sugar_inner, name, init_expr);
                }
                free(init_expr);
            }

            if (ann_type_name != NULL)
                register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
    }

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "BoxArray") == 0) {
        const char *inner = NULL;
        if (ann_type_name != NULL && strncmp(ann_type_name, "Box<Array<", 10) == 0) {
            inner = ann_type_name + 10;
            const char *close = strstr(inner, ">>");
            static char inner_buf[64];
            size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
            if (len >= sizeof(inner_buf))
                len = sizeof(inner_buf) - 1;
            memcpy(inner_buf, inner, len);
            inner_buf[len] = '\0';
            inner = inner_buf;
        }
        if (inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit BoxArray declaration for '%s': missing explicit Box<Array<T>> annotation",
                    name != NULL ? name : "(anonymous)");
            }
            free(ann_type_name);
            return;
        }

        char *capacity = (init->data.call.arg_count > 0)
                         ? emit_expression(init->data.call.arguments[0], ctx)
                         : pergyra_strdup("0");
        char *allocator = (init->data.call.arg_count > 1)
                          ? emit_expression(init->data.call.arguments[1], ctx)
                          : pergyra_strdup("NULL");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyBoxArray_%s %s = pgy_box_array_new_%s(%s, %s);\n",
            inner, name, inner, capacity, allocator);
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
        } else {
            char *box_array_type = strdup_fmt("Box<Array<%s>>", inner);
            register_typed_var(ctx, name, box_array_type);
            free(box_array_type);
        }
        free(capacity);
        free(allocator);
        free(ann_type_name);
        return;
    }

    /* Detect Box<T> - Type inference from Box(value) */
    bool is_box = false;
    const char *box_inner = "Int";
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "Box") == 0) {
            is_box = true;
            /* Infer type from annotation or argument */
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                /* Infer from argument type */
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
        /* Rc<T> - Reference counted box */
        else if (strcmp(callee_name, "Rc") == 0) {
            is_box = true;
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
    }

    if (is_box) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyBox_%s %s = pgy_box_new_%s(", 
                      box_inner, name, box_inner);
        if (init->data.call.arg_count > 0) {
            char *arg = emit_expression(init->data.call.arguments[0], ctx);
            codebuf_write(ctx->out, "%s", arg);
            free(arg);
        }
        codebuf_write(ctx->out, ");\n");
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Int>");
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Channel<", 8) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        char *capacity = pergyra_strdup("16");

        if (init != NULL && init->type == AST_CALL
            && init->data.call.arg_count > 0) {
            free(capacity);
            capacity = emit_expression(init->data.call.arguments[0], ctx);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "PgyChannel_%s %s;\n", inner, name);
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_channel_init_%s(&%s, %s);\n",
            inner, name, capacity);
        register_typed_var(ctx, name, ann_type_name);
        free(capacity);
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Option<", 7) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        if (init != NULL
            && init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (strcmp(callee_name, "Some") == 0 && init->data.call.arg_count == 1) {
                char *arg = emit_expression(init->data.call.arguments[0], ctx);
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyOption_%s %s = Some_%s(%s);\n",
                    inner, name, inner, arg);
                register_typed_var(ctx, name, ann_type_name);
                free(arg);
                free(ann_type_name);
                return;
            }
            if (strcmp(callee_name, "None") == 0 && init->data.call.arg_count == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyOption_%s %s = None_%s();\n",
                    inner, name, inner);
                register_typed_var(ctx, name, ann_type_name);
                free(ann_type_name);
                return;
            }
        }
    }

    if (resolved_ann != NULL
        && resolved_ann->type == AST_TYPE
        && resolved_ann->data.type.name != NULL
        && (strcmp(resolved_ann->data.type.name, "HashMap") == 0
            || strcmp(resolved_ann->data.type.name, "List") == 0
            || strcmp(resolved_ann->data.type.name, "Queue") == 0)
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        const char *type_name = resolved_ann->data.type.name;
        const char *inner = slot_inner_type_name(ann_type_name);
        if (strcmp(callee_name, "MapNew") == 0
            && strcmp(type_name, "HashMap") == 0
            && resolved_ann->data.type.generic_args != NULL
            && resolved_ann->data.type.generic_args->count == 2) {
            GenericParam *key_param = resolved_ann->data.type.generic_args->params[0];
            GenericParam *value_param = resolved_ann->data.type.generic_args->params[1];
            char *key = (key_param != NULL && key_param->constraint != NULL)
                ? render_type_name(key_param->constraint)
                : pergyra_strdup((key_param != NULL && key_param->name != NULL)
                    ? key_param->name : "Int");
            char *value = (value_param != NULL && value_param->constraint != NULL)
                ? render_type_name(value_param->constraint)
                : pergyra_strdup((value_param != NULL && value_param->name != NULL)
                    ? value_param->name : "Int");
            if (strcmp(key, "String") == 0 && value != NULL) {
                ensure_collection_specialization(ctx, "Map", value);
                write_indent(ctx);
                codebuf_write(ctx->out, "%s %s = pgy_map_new_%s();\n",
                    pergyra_type_to_c(ann_type_name), name,
                    collection_runtime_suffix(value));
                register_typed_var(ctx, name, ann_type_name);
                free(key);
                free(value);
                free(ann_type_name);
                return;
            }
            free(key);
            free(value);
        }
        if (strcmp(callee_name, "ListNew") == 0
            && strcmp(type_name, "List") == 0) {
            ensure_collection_specialization(ctx, "List", inner);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = pgy_list_new_%s();\n",
                pergyra_type_to_c(ann_type_name), name,
                collection_runtime_suffix(inner));
            register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
        if (strcmp(callee_name, "QueueNew") == 0
            && strcmp(type_name, "Queue") == 0) {
            ensure_collection_specialization(ctx, "Queue", inner);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = pgy_queue_new_%s();\n",
                pergyra_type_to_c(ann_type_name), name,
                collection_runtime_suffix(inner));
            register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
    }

    if (resolved_ann != NULL
        && resolved_ann->type == AST_TYPE
        && resolved_ann->data.type.name != NULL
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "ToObject") == 0
        && init->data.call.arg_count >= 2
        && init->data.call.arguments[1] != NULL
        && init->data.call.arguments[1]->type == AST_IDENTIFIER) {
        ASTNode *target_decl = find_class_decl(ctx, resolved_ann->data.type.name);
        if (target_decl != NULL
            && target_decl->type == AST_CLASS_DECL
            && target_decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT) {
            const char *source_name = init->data.call.arguments[1]->data.identifier.name;
            register_projection_borrow_var(ctx, name,
                ann_type_name != NULL ? ann_type_name : resolved_ann->data.type.name,
                source_name);
            free(ann_type_name);
            return;
        }
    }

    /* Array literal: let arr = [1, 2, 3] → PgyArray_Int arr = ({ ... }); */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        const char *array_type_name = ann_type_name != NULL
            ? ann_type_name
            : infer_expression_type_name(ctx, init);
        const char *array_c_type = pergyra_type_to_c(array_type_name);
        char *init_expr = emit_expression(init, ctx);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", array_c_type, name, init_expr);
        free(init_expr);
        register_typed_var(ctx, name, array_type_name);
        free(ann_type_name);
        return;
    }

    /* Normal variable with type inference */
    const char *c_type = NULL;
    if (ann != NULL) {
        c_type = pergyra_ast_type_to_c(ann);
    } else if (init != NULL) {
        const char *inferred_type = NULL;
        /* Type inference from initializer */
        if (init->type == AST_NUMBER)  c_type = "int32_t";
        else if (init->type == AST_STRING)  c_type = "char*";
        else if (init->type == AST_BOOLEAN) c_type = "bool";
        else if (init->type == AST_SPAWN_EXPR) c_type = "PgyTaskHandle";
        else if (init->type == AST_CHANNEL_RECV) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL)
                c_type = pergyra_type_to_c(inferred_type);
        }
        else if (init->type == AST_CALL || init->type == AST_ARRAY_LITERAL || init != NULL) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL)
                c_type = pergyra_type_to_c(inferred_type);
        }
    }

    if (c_type == NULL) {
        transpiler_set_backend_error(
            ctx,
            "cannot determine C type for let binding '%s'; explicit annotation or resolvable initializer type is required",
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        return;
    }

    /* Collection constructors: let s: Set<Int> = SetNew()
     * Emit the correct type-specific initializer from the annotation. */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && strcmp(init->data.call.callee->data.identifier.name, "SetNew") == 0
        && strncmp(ann_type_name, "Set<", 4) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        const char *c_type = pergyra_type_to_c(ann_type_name);
        const char *suffix = collection_runtime_suffix(inner);
        ensure_collection_specialization(ctx, "Set", inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = pgy_set_new_%s();\n",
                      c_type, name, suffix);
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    /* Struct/class constructor: let p: Point = Point(...)
     * Lower positional constructor args into field-order initialization.
     * Missing fields stay zero-initialized.
     * For generic classes: callee is "Node" but ann_type_name is "Node_Int",
     * so also match against the original class name via generic_class_spec_name. */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && (strcmp(init->data.call.callee->data.identifier.name, ann_type_name) == 0
            || (generic_class_spec_name != NULL
                && ann->data.type.name != NULL
                && strcmp(init->data.call.callee->data.identifier.name, ann->data.type.name) == 0))
        ) {
        ASTNode *class_decl = find_class_decl(ctx, ann_type_name);
        /* For generic classes, find_class_decl won't find "Node_Int" —
         * fall back to the original generic class declaration. */
        if (class_decl == NULL && generic_class_spec_name != NULL
            && ann->data.type.name != NULL)
            class_decl = find_class_decl(ctx, ann->data.type.name);
        write_indent(ctx);
        if (class_decl != NULL
            && class_decl->type == AST_CLASS_DECL
            && class_decl->data.class_decl.field_count > 0
            && init->data.call.arg_count > 0) {
            codebuf_write(ctx->out, "%s %s = { ", ann_type_name, name);
            for (size_t i = 0; i < init->data.call.arg_count; i++) {
                ClassField *field;
                char *arg_expr;
                if (i >= class_decl->data.class_decl.field_count)
                    break;
                field = class_decl->data.class_decl.fields[i];
                if (field == NULL || field->name == NULL)
                    continue;
                arg_expr = emit_expression(init->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(ctx->out, ", ");
                codebuf_write(ctx->out, ".%s = %s", field->name, arg_expr);
                free(arg_expr);
            }
            codebuf_write(ctx->out, " };\n");
        } else {
            /* Domain/runtime constructors carry internal state bits.
             * Reuse expression lowering so zone/world/relation/effect
             * literals preserve dirty/ready/runtime metadata. */
            ASTNode *zone_decl = find_zone_decl(ctx, ann_type_name);
            ASTNode *world_decl = find_world_decl(ctx, ann_type_name);
            ASTNode *relation_decl = find_relation_decl(ctx, ann_type_name);
            ASTNode *effect_decl = find_effect_decl(ctx, ann_type_name);
            ASTNode *party_decl = find_party_decl(ctx, ann_type_name);
            ASTNode *roster_decl = find_roster_decl(ctx, ann_type_name);
            if ((zone_decl != NULL && zone_decl->type == AST_ZONE_DECL)
                || (world_decl != NULL && world_decl->type == AST_WORLD_DECL)
                || (party_decl != NULL && party_decl->type == AST_PARTY_DECL)
                || (roster_decl != NULL && roster_decl->type == AST_ROSTER_DECL)
                || (relation_decl != NULL && relation_decl->type == AST_RELATION_DECL)
                || (effect_decl != NULL && effect_decl->type == AST_EFFECT_DECL)) {
                char *init_expr = emit_expression(init, ctx);
                codebuf_write(ctx->out, "%s %s = %s;\n",
                    ann_type_name, name, init_expr != NULL ? init_expr : "0");
                free(init_expr);
            } else {
                codebuf_write(ctx->out, "%s %s = {0};\n", ann_type_name, name);
            }
        }
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    write_indent(ctx);
    if (callable_type != NULL || callable_decl != NULL) {
        char *decl = callable_type != NULL
            ? pergyra_ast_typed_declarator(callable_type, name)
            : pergyra_func_pointer_declarator_from_decl(callable_decl, name);
        if (init != NULL) {
            ctx->expected_type = ann_type_name;
            char *init_expr = emit_expression(init, ctx);
            ctx->expected_type = NULL;
            codebuf_write(ctx->out, "%s = %s;\n", decl, init_expr);
            free(init_expr);
        } else {
            codebuf_write(ctx->out, "%s = 0;\n", decl);
        }
        free(decl);
    } else if (init != NULL) {
        ctx->expected_type = ann_type_name;
        char *init_expr = emit_expression(init, ctx);
        ctx->expected_type = NULL;
        codebuf_write(ctx->out, "%s %s = %s;\n", c_type, name, init_expr);
        free(init_expr);
    } else {
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    }

    if (ann_type_name != NULL) {
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
    } else if (init != NULL && init->type == AST_CALL) {
        register_typed_var(ctx, name, infer_expression_type_name(ctx, init));
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        char *future_type = infer_spawn_return_type_name(ctx, init);
        char *wrapped = strdup_fmt("Future<%s>", future_type);
        register_typed_var(ctx, name, wrapped);
        free(future_type);
        free(wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_typed_var(ctx, name, inner);
    } else if (init != NULL) {
        /* Fallback: infer from any initializer (string literals, binary exprs, etc.) */
        const char *inferred = infer_expression_type_name(ctx, init);
        if (inferred != NULL) {
            register_typed_var(ctx, name, inferred);
        }
    }
}

/* -----------------------------------------------------------------
 * Function declaration emitter
 * ----------------------------------------------------------------- */

static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    g_type_render_ctx = ctx;
    ensure_type_specializations_from_ast(ctx, node->data.func_decl.return_type);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (pt == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot determine parameter type for forward declaration '%s' at argument %zu",
                name != NULL ? name : "<function>",
                i);
            if (params_sig != NULL)
                codebuf_destroy(params_sig);
            free(header_decl);
            g_type_render_ctx = saved_render_ctx;
            return;
        }
        if (i > 0)
            codebuf_write(params_sig, ", ");
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
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(buf, "%s;\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);
    g_type_render_ctx = saved_render_ctx;
}

static void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, node->data.func_decl.name, buf, ctx);
}

static const MIRRoutine *
transpiler_find_mir_function(const TranspilerCtx *ctx, const ASTNode *func_decl)
{
    if (ctx == NULL || ctx->mir == NULL || func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    const char *target = func_decl->data.func_decl.name;
    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_FUNCTION)
            continue;
        if (routine->name == NULL)
            continue;
        /* Exact match */
        if (strcmp(routine->name, target) == 0)
            return routine;
        /* Specialized name: e.g. "Identity_Int" matches "Identity" */
        size_t name_len = strlen(target);
        if (strncmp(routine->name, target, name_len) == 0
            && (routine->name[name_len] == '_' || routine->name[name_len] == '\0'))
            return routine;
    }

    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_intent(const TranspilerCtx *ctx, const ASTNode *intent_decl)
{
    if (ctx == NULL || ctx->mir == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL
        || intent_decl->data.intent_decl.name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_INTENT
            || routine->name == NULL
            || strcmp(routine->name, intent_decl->data.intent_decl.name) != 0) {
            continue;
        }
        /* Match by name only - pointer comparison may fail across different AST instances */
        return routine;
    }

    return NULL;
}

static const char *
transpiler_find_mir_intent_meta_arg(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name)
{
    if (routine == NULL || inst_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, inst_name) != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->arg0;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                          const char *step_name,
                                          const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentWho") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

static size_t
transpiler_collect_mir_intent_participants(const MIRRoutine *routine,
                                           const char ***aliases_out,
                                           const char ***types_out)
{
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (types_out != NULL)
        *types_out = NULL;
    if (routine == NULL || aliases_out == NULL || types_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown_aliases;
            const char **grown_types;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentParticipant") != 0)
                continue;
            if (inst->arg0 == NULL || inst->arg1 == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown_aliases = malloc(new_capacity * sizeof(const char *));
                grown_types = malloc(new_capacity * sizeof(const char *));
                if (grown_aliases == NULL || grown_types == NULL) {
                    free((void *)grown_aliases);
                    free((void *)grown_types);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                if (count > 0) {
                    memcpy((void *)grown_aliases, (const void *)aliases,
                           count * sizeof(const char *));
                    memcpy((void *)grown_types, (const void *)types,
                           count * sizeof(const char *));
                }
                free((void *)aliases);
                free((void *)types);
                aliases = grown_aliases;
                types = grown_types;
                capacity = new_capacity;
            }
            aliases[count] = inst->arg0;
            types[count] = inst->arg1;
            count++;
        }
    }

    *aliases_out = aliases;
    *types_out = types;
    return count;
}

static size_t
transpiler_collect_mir_intent_steps(const MIRRoutine *routine, ASTNode ***steps_out)
{
    ASTNode **steps = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (steps_out != NULL)
        *steps_out = NULL;
    if (routine == NULL || steps_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL
                || inst->ast->type != AST_INTENT_STEP) {
                continue;
            }
            if (inst->name != NULL && strncmp(inst->name, "Intent", 6) == 0)
                continue;
            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc(steps, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(steps);
                    return 0;
                }
                steps = grown;
                capacity = new_capacity;
            }
            steps[count++] = inst->ast;
        }
    }

    *steps_out = steps;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_check_expr(const MIRRoutine *routine,
                                      const char *step_name,
                                      const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentCheck") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->ast;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                         const char *step_name,
                                         const char *phase_name,
                                         ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentEval") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc(exprs, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(exprs);
                    return 0;
                }
                exprs = grown;
                capacity = new_capacity;
            }
            exprs[count++] = inst->ast;
        }
    }

    *exprs_out = exprs;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_eval_expr(const MIRRoutine *routine,
                                     const char *step_name,
                                     const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = transpiler_collect_mir_intent_eval_exprs(
        routine, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    free(exprs);
    return result;
}

static size_t
transpiler_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                               const char *step_name,
                                               const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentDispatch") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

static bool
transpiler_parse_versioned_name(const char *versioned, char *base, size_t base_size,
                                size_t *version_out)
{
    const char *dot;
    size_t len;
    if (versioned == NULL || base == NULL || base_size == 0 || version_out == NULL)
        return false;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return false;
    len = (size_t)(dot - versioned);
    if (len + 1 > base_size)
        return false;
    memcpy(base, versioned, len);
    base[len] = '\0';
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return true;
}

static size_t
transpiler_ssa_name_bucket_index(const char *key)
{
    size_t hash = 5381u;
    const unsigned char *u = (const unsigned char *)key;
    while (u != NULL && *u != '\0') {
        hash = ((hash << 5) + hash) + (size_t)(*u);
        ++u;
    }
    return hash % TRANSPILE_SSA_NAME_BUCKETS;
}

static void
transpiler_ssa_map_clear(TranspilerSSANameMap *map)
{
    if (map == NULL)
        return;
    for (size_t i = 0; i < TRANSPILE_SSA_NAME_BUCKETS; ++i) {
        if (map->buckets[i].in_use && map->buckets[i].base_name != NULL)
            free((void *)map->buckets[i].base_name);
    }
    memset(map, 0, sizeof(*map));
}

static bool
transpiler_ssa_name_map_set(TranspilerSSANameMap *map,
                            const char *base_name,
                            const char *versioned_name)
{
    size_t idx;
    size_t attempts;

    if (map == NULL || base_name == NULL || versioned_name == NULL)
        return false;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        TranspilerSSANameBucket *bucket = &map->buckets[idx];
        if (!bucket->in_use) {
            bucket->in_use = true;
            bucket->base_name = pergyra_strdup(base_name);
            if (bucket->base_name == NULL) {
                bucket->in_use = false;
                return false;
            }
            bucket->versioned_name = versioned_name;
            return true;
        }
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0) {
            bucket->versioned_name = versioned_name;
            return true;
        }
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return false;
}

static bool
transpiler_collect_ssa_name_entries(const char **versioned_values,
                                   size_t value_count,
                                   const char **base_names,
                                   const char **versioned_names,
                                   size_t max_entries,
                                   size_t *map_count_out)
{
    size_t map_count = 0;

    if (versioned_values == NULL
        || base_names == NULL
        || versioned_names == NULL
        || max_entries == 0) {
        return true;
    }
    for (size_t i = 0; i < value_count; i++) {
        const char *versioned = versioned_values[i];
        char base[128];
        size_t parsed_version = 0;
        bool replaced = false;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned, base, sizeof(base), &parsed_version))
            continue;
        for (size_t j = 0; j < map_count; j++) {
            if (base_names[j] != NULL && strcmp(base_names[j], base) == 0) {
                versioned_names[j] = versioned;
                replaced = true;
                break;
            }
        }
        if (replaced)
            continue;
        if (map_count >= max_entries)
            return false;
        base_names[map_count] = pergyra_strdup(base);
        versioned_names[map_count] = versioned;
        map_count++;
    }
    if (map_count_out != NULL)
        *map_count_out = map_count;
    return true;
}

static void
transpiler_free_ssa_name_entries(const char **base_names, size_t entry_count)
{
    for (size_t i = 0; i < entry_count; i++) {
        free((void *)base_names[i]);
    }
}

static bool
transpiler_rebuild_ssa_map(TranspilerSSANameMap *ssa_map,
                          const char **base_names,
                          const char **versioned_names,
                          size_t map_count)
{
    if (ssa_map == NULL || base_names == NULL || versioned_names == NULL) {
        if (ssa_map != NULL)
            transpiler_ssa_map_clear(ssa_map);
        return false;
    }
    transpiler_ssa_map_clear(ssa_map);
    for (size_t i = 0; i < map_count; i++) {
        if (base_names[i] == NULL || versioned_names[i] == NULL)
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map, base_names[i], versioned_names[i]))
            return false;
    }
    return true;
}

static void
transpiler_emit_mir_block_mapping_comment(CodeBuf *out,
                                          int indent,
                                          const char *routine_name,
                                          const MIRRoutine *routine,
                                          const MIRBasicBlock *block)
{
    const ASTNode *source_stmt = NULL;
    uint32_t line = 0;
    uint32_t column = 0;

    if (out == NULL || routine == NULL || block == NULL)
        return;

    source_stmt = block->source_ast;
    if (source_stmt != NULL) {
        line = source_stmt->line;
        column = source_stmt->column;
    }

    if (source_stmt != NULL) {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%zu (%s) src=%u:%u ast=%p */\n",
            block->id,
            block->source_hir_block_id,
            routine_name != NULL ? routine_name : "<routine>",
            line,
            column,
            (const void *)source_stmt);
    } else {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%s (%s) */\n",
            block->id,
            block->source_hir_block_id == SIZE_MAX ? "<none>" : "mapped",
            routine_name != NULL ? routine_name : "<routine>");
    }
}

static const char *
transpiler_resolve_ssa_name(const TranspilerSSANameMap *ssa_map,
                            const char *base_name)
{
    size_t idx;
    size_t attempts;

    if (ssa_map == NULL || base_name == NULL)
        return NULL;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        const TranspilerSSANameBucket *bucket = &ssa_map->buckets[idx];
        if (!bucket->in_use)
            return NULL;
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0)
            return bucket->versioned_name;
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_method(const TranspilerCtx *ctx,
                           const char *owner_name,
                           const ASTNode *method_decl)
{
    const char *target = NULL;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    target = method_decl->data.func_decl.name;
    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_METHOD)
            continue;
        if (routine->ast == method_decl)
            return routine;
        if (routine->name != NULL
            && routine->owner_name != NULL
            && strcmp(routine->name, target) == 0
            && strcmp(routine->owner_name, owner_name) == 0) {
            return routine;
        }
    }

    return NULL;
}

static bool
transpiler_name_is_token_local(const char *name);

static const char *
transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
                                   const char *base_name)
{
    const char *resolved;
    if (ctx == NULL || ctx->active_ssa_map == NULL || base_name == NULL)
        return NULL;
    resolved = transpiler_resolve_ssa_name(
        (const TranspilerSSANameMap *)ctx->active_ssa_map,
        base_name);
    if (resolved != NULL)
        return resolved;
    if (transpiler_name_is_token_local(base_name))
        return base_name;
    return NULL;
}

static bool
transpiler_name_is_token_local(const char *name)
{
    const char *tag;
    if (name == NULL)
        return false;
    tag = strstr(name, "_token");
    if (tag == NULL)
        return false;
    tag += 6;
    while (*tag != '\0') {
        if (*tag < '0' || *tag > '9')
            return false;
        tag++;
    }
    return true;
}

static bool
transpiler_type_name_is_slot_like(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return strncmp(type_name, "Slot<", 5) == 0
        || strncmp(type_name, "SecureSlot<", 11) == 0
        || strncmp(type_name, "DeviceSlot<", 11) == 0;
}

static bool
transpiler_emit_mir_block_with_ssa_map(TranspilerSSANameMap *ssa_map,
                                       const MIRBasicBlock *block)
{
    const char *base_names[TRANSPILE_SSA_MAP_CAPACITY];
    const char *versioned_names[TRANSPILE_SSA_MAP_CAPACITY];
    size_t map_count = 0;

    transpiler_ssa_map_clear(ssa_map);
    if (block == NULL || block->ssa_entry_values == NULL || block->ssa_entry_value_count == 0)
        return true;
    if (!transpiler_collect_ssa_name_entries(block->ssa_entry_values, block->ssa_entry_value_count,
                                            base_names, versioned_names,
                                            TRANSPILE_SSA_MAP_CAPACITY,
                                            &map_count)) {
        transpiler_free_ssa_name_entries(base_names, map_count);
        return false;
    }
    if (!transpiler_rebuild_ssa_map(ssa_map, base_names, versioned_names, map_count))
        return false;
    transpiler_free_ssa_name_entries(base_names, map_count);
    return true;
}

static char *
transpiler_make_c_ssa_name(const char *versioned_name)
{
    if (versioned_name == NULL)
        return NULL;
    char base[128];
    size_t version = 0;
    if (!transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version))
        return pergyra_strdup(versioned_name);
    return strdup_fmt("_pgy_ssa_%s_%zu", base, version);
}

static bool
transpiler_versioned_name_list_contains(const char **names,
                                        size_t count,
                                        const char *name)
{
    if (names == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static bool
transpiler_versioned_name_list_add(const char **names,
                                   size_t *count,
                                   size_t capacity,
                                   const char *name)
{
    if (names == NULL || count == NULL || name == NULL)
        return false;
    if (transpiler_versioned_name_list_contains(names, *count, name))
        return true;
    if (*count >= capacity)
        return false;
    names[(*count)++] = name;
    return true;
}

static bool
transpiler_c_type_uses_scalar_zero(const char *c_type)
{
    if (c_type == NULL)
        return true;
    if (strchr(c_type, '*') != NULL)
        return true;
    return strcmp(c_type, "int") == 0
           || strcmp(c_type, "int32_t") == 0
           || strcmp(c_type, "int64_t") == 0
           || strcmp(c_type, "size_t") == 0
           || strcmp(c_type, "bool") == 0
           || strcmp(c_type, "float") == 0
           || strcmp(c_type, "double") == 0;
}

static const char *
transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                           const ASTNode *func_decl,
                                           ASTNode *expr)
{
    const char *semantic_type = infer_expression_type_name(ctx, expr);
    if (semantic_type != NULL
        && semantic_type[0] != '\0'
        && strcmp(semantic_type, "Int") != 0)
        return semantic_type;
    if (expr == NULL)
        return NULL;
    switch (expr->type) {
        case AST_NUMBER:
            return "Int";
        case AST_STRING:
            return "String";
        case AST_BOOLEAN:
            return "Bool";
        case AST_IDENTIFIER:
            return transpiler_find_local_type_name(ctx, func_decl, expr->data.identifier.name);
        case AST_MEMBER_ACCESS: {
            const char *resolved = resolve_nominal_host_expr_type_name(ctx, expr);
            if (resolved != NULL && resolved[0] != '\0')
                return resolved;
            if (expr->data.member.object != NULL && expr->data.member.name != NULL) {
                const char *obj_type = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, expr->data.member.object);
                if (obj_type != NULL) {
                    ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                    if (obj_decl != NULL) {
                        for (size_t fi = 0; fi < obj_decl->data.class_decl.field_count; fi++) {
                            ClassField *f = obj_decl->data.class_decl.fields[fi];
                            if (f != NULL && f->name != NULL && f->type != NULL
                                && strcmp(f->name, expr->data.member.name) == 0) {
                                static char *rendered_member = NULL;
                                free(rendered_member);
                                rendered_member = render_type_name(f->type);
                                return rendered_member;
                            }
                        }
                    }
                }
            }
            return semantic_type != NULL && semantic_type[0] != '\0' ? semantic_type : NULL;
        }
        case AST_BINARY:
            switch (expr->data.binary.op.type) {
                case TOKEN_EQUAL:
                case TOKEN_NOT_EQUAL:
                case TOKEN_LESS:
                case TOKEN_GREATER:
                case TOKEN_LESS_EQUAL:
                case TOKEN_GREATER_EQUAL:
                case TOKEN_AND:
                case TOKEN_OR:
                    return "Bool";
                default:
                    return transpiler_infer_local_type_name_from_expr(ctx, func_decl, expr->data.binary.left);
            }
        case AST_UNARY:
            if (expr->data.unary.op.type == TOKEN_NOT)
                return "Bool";
            return transpiler_infer_local_type_name_from_expr(ctx, func_decl, expr->data.unary.operand);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_MEMBER_ACCESS
                && expr->data.call.callee->data.member.name != NULL) {
                ASTNode *receiver = expr->data.call.callee->data.member.object;
                const char *method_name = expr->data.call.callee->data.member.name;
                const char *receiver_type = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, receiver);
                ASTNode *method_decl = NULL;
                if (receiver_type != NULL) {
                    method_decl = find_nominal_host_method_decl(ctx, receiver_type, method_name);
                }
                if (method_decl != NULL && method_decl->type == AST_FUNC_DECL
                    && method_decl->data.func_decl.return_type != NULL) {
                    static char *rendered_return = NULL;
                    free(rendered_return);
                    rendered_return = render_type_name(method_decl->data.func_decl.return_type);
                    return rendered_return;
                }
            }
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_IDENTIFIER
                && expr->data.call.callee->data.identifier.name != NULL) {
                const char *callee_name = expr->data.call.callee->data.identifier.name;
                ASTNode *callee_decl = find_function_decl(ctx, callee_name);
                if (callee_decl != NULL
                    && callee_decl->type == AST_FUNC_DECL
                    && callee_decl->data.func_decl.return_type != NULL) {
                    static char *rendered_func_return = NULL;
                    free(rendered_func_return);
                    rendered_func_return = render_type_name(callee_decl->data.func_decl.return_type);
                    return rendered_func_return;
                }
                if (find_class_decl(ctx, callee_name) != NULL
                    || find_zone_decl(ctx, callee_name) != NULL
                    || find_world_decl(ctx, callee_name) != NULL
                    || find_relation_decl(ctx, callee_name) != NULL
                    || find_effect_decl(ctx, callee_name) != NULL
                    || find_party_decl(ctx, callee_name) != NULL
                    || find_roster_decl(ctx, callee_name) != NULL) {
                    return callee_name;
                }
            }
            return semantic_type != NULL && semantic_type[0] != '\0' ? semantic_type : NULL;
        default:
            return NULL;
    }
}

static const char *
transpiler_find_local_type_name_in_block(TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         ASTNode *body,
                                         const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx,
                func_decl, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL) {
            static char *rendered = NULL;
            free(rendered);
            rendered = render_type_name(body->data.let_decl.type);
            return rendered;
        }
        return transpiler_infer_local_type_name_from_expr(ctx, func_decl, body->data.let_decl.initializer);
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL
            && strcmp(body->data.with_stmt.alias, base_name) == 0) {
            static char rendered_slot[256];
            char *inner = render_type_name(body->data.with_stmt.slot_type);
            snprintf(rendered_slot, sizeof(rendered_slot),
                     "%s<%s>",
                     body->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                     inner != NULL ? inner : "Int");
            free(inner);
            return rendered_slot;
        }
        return transpiler_find_local_type_name_in_block(ctx, func_decl,
            body->data.with_stmt.body, base_name);
    }
    if (body->type == AST_IF_STMT) {
        const char *found = transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.while_loop.body, base_name);
    return NULL;
}

static ASTNode *
transpiler_find_local_type_ast_in_block(TranspilerCtx *ctx,
                                        ASTNode *body,
                                        const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_local_type_ast_in_block(
                ctx, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL)
            return body->data.let_decl.type;
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_CALL
            && body->data.let_decl.initializer->data.call.callee != NULL
            && body->data.let_decl.initializer->data.call.callee->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.call.callee->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.call.callee->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
                return decl->data.func_decl.return_type;
            }
        }
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
                return decl->data.func_decl.return_type;
            }
        }
        return NULL;
    }
    if (body->type == AST_WITH_STMT)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.with_stmt.body, base_name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_local_type_ast_in_block(
            ctx, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.while_loop.body, base_name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.for_loop.body, base_name);
    return NULL;
}

static ASTNode *
transpiler_find_local_type_ast(TranspilerCtx *ctx,
                               const ASTNode *func_decl,
                               const char *base_name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL
        || base_name == NULL) {
        return NULL;
    }
    return transpiler_find_local_type_ast_in_block(
        ctx, func_decl->data.func_decl.body, base_name);
}

static bool
transpiler_has_local_binding_in_block(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return false;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            if (transpiler_has_local_binding_in_block(
                    body->data.block.statements[i], base_name)) {
                return true;
            }
        }
        return false;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        return true;
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL
            && strcmp(body->data.with_stmt.alias, base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(body->data.with_stmt.body, base_name);
    }
    if (body->type == AST_IF_STMT) {
        return transpiler_has_local_binding_in_block(body->data.if_stmt.then_branch, base_name)
            || transpiler_has_local_binding_in_block(body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_has_local_binding_in_block(body->data.while_loop.body, base_name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_has_local_binding_in_block(body->data.for_loop.body, base_name);
    return false;
}

static void
transpiler_register_with_alias_bindings_in_block(TranspilerSSANameMap *ssa_map,
                                                 ASTNode *body)
{
    if (ssa_map == NULL || body == NULL)
        return;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++)
            transpiler_register_with_alias_bindings_in_block(ssa_map,
                body->data.block.statements[i]);
        return;
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL)
            transpiler_ssa_name_map_set(ssa_map,
                body->data.with_stmt.alias, body->data.with_stmt.alias);
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.with_stmt.body);
        return;
    }
    if (body->type == AST_IF_STMT) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.if_stmt.then_branch);
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.if_stmt.else_branch);
        return;
    }
    if (body->type == AST_WHILE_LOOP) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.while_loop.body);
        return;
    }
    if (body->type == AST_FOR_LOOP) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.for_loop.body);
    }
}

static void
transpiler_register_explicit_local_bindings_in_block(TranspilerCtx *ctx,
                                                     const ASTNode *func_decl,
                                                     ASTNode *body)
{
    if (ctx == NULL || body == NULL || body->type != AST_BLOCK)
        return;
    for (size_t i = 0; i < body->data.block.count; i++) {
        ASTNode *stmt = body->data.block.statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_LET_DECL && stmt->data.let_decl.name != NULL) {
            const char *type_name = NULL;
            char *rendered_type = NULL;
            if (stmt->data.let_decl.type != NULL) {
                rendered_type = render_type_name(stmt->data.let_decl.type);
                type_name = rendered_type;
            } else if (stmt->data.let_decl.initializer != NULL) {
                type_name = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, stmt->data.let_decl.initializer);
            }
            if (type_name != NULL && type_name[0] != '\0') {
                register_typed_var(ctx, stmt->data.let_decl.name, type_name);
                if (transpiler_type_name_is_slot_like(type_name)) {
                    register_slot_var(ctx, stmt->data.let_decl.name,
                        slot_inner_type_name(type_name),
                        strncmp(type_name, "SecureSlot<", 11) == 0,
                        false);
                }
            }
            free(rendered_type);
            continue;
        }
        if (stmt->type == AST_WITH_STMT && stmt->data.with_stmt.alias != NULL) {
            char *inner = render_type_name(stmt->data.with_stmt.slot_type);
            char *slot_type = strdup_fmt("%s<%s>",
                stmt->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                inner != NULL ? inner : "Int");
            register_typed_var(ctx, stmt->data.with_stmt.alias,
                slot_type != NULL ? slot_type : "Slot<Int>");
            register_slot_var(ctx, stmt->data.with_stmt.alias,
                inner != NULL ? inner : "Int",
                stmt->data.with_stmt.is_secure, false);
            free(slot_type);
            free(inner);
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.with_stmt.body);
        }
    }
}

static bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        return false;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0)
            return true;
    }
    return transpiler_has_local_binding_in_block(func_decl->data.func_decl.body,
        base_name);
}

static const char *
transpiler_find_local_type_name(TranspilerCtx *ctx,
                                const ASTNode *func_decl,
                                const char *base_name)
{
    const char *typed_name = NULL;
    const char *member_type = NULL;

    if (base_name == NULL)
        return NULL;
    if (ctx != NULL) {
        typed_name = lookup_typed_var(ctx, base_name);
        if (typed_name != NULL)
            return typed_name;
    }
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        goto owner_fallback;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0 && p->type != NULL) {
            static char *rendered_param = NULL;
            free(rendered_param);
            rendered_param = render_type_name(p->type);
            return rendered_param;
        }
    }
    typed_name = transpiler_find_local_type_name_in_block(ctx, func_decl,
        func_decl->data.func_decl.body, base_name);
    if (typed_name != NULL)
        return typed_name;

owner_fallback:
    if (ctx != NULL && ctx->current_class_name != NULL) {
        member_type = lookup_nominal_host_member_type_name(ctx,
            ctx->current_class_name, base_name);
        if (member_type != NULL)
            return member_type;
    }
    if (ctx != NULL && ctx->current_zone_name != NULL) {
        member_type = lookup_nominal_host_member_type_name(ctx,
            ctx->current_zone_name, base_name);
        if (member_type != NULL)
            return member_type;
    }
    if (ctx != NULL && ctx->current_world_name != NULL) {
        member_type = lookup_nominal_host_member_type_name(ctx,
            ctx->current_world_name, base_name);
        if (member_type != NULL)
            return member_type;
    }
    return NULL;
}

static bool
transpiler_mir_type_supported(const char *type_name)
{
    if (type_name == NULL)
        return false;
    if (strcmp(type_name, "Void") == 0)
        return true;
    return strcmp(type_name, "Int") == 0
           || strcmp(type_name, "Long") == 0
           || strcmp(type_name, "Float") == 0
           || strcmp(type_name, "Bool") == 0
           || strcmp(type_name, "String") == 0
           || strncmp(type_name, "Slot<", 5) == 0
           || strncmp(type_name, "SecureSlot<", 11) == 0
           || strncmp(type_name, "DeviceSlot<", 11) == 0;
}

static bool
transpiler_mir_ast_type_supported(const ASTNode *type_node)
{
    char *type_name = NULL;
    const char *c_type = NULL;

    if (type_node == NULL)
        return true;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        if (!transpiler_mir_ast_type_supported(
                type_node->data.event_handler_type.return_type)) {
            return false;
        }
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            if (!transpiler_mir_ast_type_supported(
                    type_node->data.event_handler_type.param_types[i])) {
                return false;
            }
        }
        return true;
    }

    type_name = render_type_name((ASTNode *)type_node);
    if (type_name == NULL)
        return false;
    if (transpiler_mir_type_supported(type_name)) {
        free(type_name);
        return true;
    }

    c_type = pergyra_ast_type_to_c((ASTNode *)type_node);
    if (c_type == NULL || c_type[0] == '\0') {
        free(type_name);
        return false;
    }
    if (strcmp(type_name, "Unknown") == 0 || strcmp(c_type, "Unknown") == 0) {
        free(type_name);
        return false;
    }

    free(type_name);
    return true;
}

static bool
transpiler_mir_function_signature_supported(const ASTNode *func_decl)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    if (!transpiler_mir_ast_type_supported(func_decl->data.func_decl.return_type))
        return false;

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL)
            continue;
        if (!transpiler_mir_ast_type_supported(param->type))
            return false;
    }

    return true;
}

static char *
emit_expression_with_ssa_map(ASTNode *node, TranspilerCtx *ctx,
                             const TranspilerSSANameMap *ssa_map)
{
    const void *saved_active_ssa_map;
    char *result;

    if (node == NULL)
        return pergyra_strdup("0");
    if (ctx == NULL)
        return emit_expression(node, ctx);

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map;
    result = emit_expression(node, ctx);
    ctx->active_ssa_map = saved_active_ssa_map;
    return result;
}

static bool
transpiler_emit_mir_phi_copies(CodeBuf *buf, int indent,
                               size_t pred_block_index,
                               const MIRBasicBlock *pred_block,
                               const MIRBasicBlock *target_block)
{
    const char *pred_bases[TRANSPILE_SSA_MAP_CAPACITY];
    const char *pred_versions[TRANSPILE_SSA_MAP_CAPACITY];
    const char *target_bases[TRANSPILE_SSA_MAP_CAPACITY];
    const char *target_versions[TRANSPILE_SSA_MAP_CAPACITY];
    size_t pred_count = 0;
    size_t target_count = 0;

    if (buf == NULL || pred_block == NULL || target_block == NULL)
        return false;

    for (size_t i = 0; i < target_block->instruction_count; i++) {
        const MIRInstruction *inst = &target_block->instructions[i];
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        for (size_t j = 0; j < inst->phi_incoming_count; j++) {
            const MIRPhiIncoming *incoming = &inst->phi_incomings[j];
            if (incoming->predecessor_block != pred_block_index
                || incoming->value_name == NULL
                || strcmp(inst->result_name, incoming->value_name) == 0) {
                continue;
            }
            {
                char *lhs = transpiler_make_c_ssa_name(inst->result_name);
                char *rhs = transpiler_make_c_ssa_name(incoming->value_name);
                write_indent_to(buf, indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
            }
            break;
        }
    }

    if (!transpiler_collect_ssa_name_entries(
            pred_block->ssa_exit_value_count > 0
                ? pred_block->ssa_exit_values
                : pred_block->ssa_entry_values,
            pred_block->ssa_exit_value_count > 0
                ? pred_block->ssa_exit_value_count
                : pred_block->ssa_entry_value_count,
            pred_bases,
            pred_versions,
            TRANSPILE_SSA_MAP_CAPACITY,
            &pred_count)) {
        return false;
    }
    if (!transpiler_collect_ssa_name_entries(target_block->ssa_entry_values,
                                             target_block->ssa_entry_value_count,
                                             target_bases,
                                             target_versions,
                                             TRANSPILE_SSA_MAP_CAPACITY,
                                             &target_count)) {
        transpiler_free_ssa_name_entries(pred_bases, pred_count);
        return false;
    }

    for (size_t i = 0; i < target_count; i++) {
        const char *lhs_version = target_versions[i];
        const char *rhs_version = NULL;

        if (lhs_version == NULL || target_bases[i] == NULL)
            continue;
        for (size_t j = 0; j < pred_count; j++) {
            if (pred_bases[j] != NULL
                && strcmp(pred_bases[j], target_bases[i]) == 0) {
                rhs_version = pred_versions[j];
                break;
            }
        }
        if (rhs_version == NULL || strcmp(lhs_version, rhs_version) == 0)
            continue;
        {
            char *lhs = transpiler_make_c_ssa_name(lhs_version);
            char *rhs = transpiler_make_c_ssa_name(rhs_version);
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }
    }

    transpiler_free_ssa_name_entries(pred_bases, pred_count);
    transpiler_free_ssa_name_entries(target_bases, target_count);
    return true;
}

static bool
transpiler_emit_mir_block_statements(CodeBuf *buf, const ASTNode *func_decl,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *out_ssa_map,
                                     char *reason,
                                     size_t reason_cap)
{
    TranspilerSSANameMap ssa_map = {0};
    TranspilerSSANameMap *ssa_map_out = &ssa_map;
    const void *saved_active_ssa_map;
    bool ok = true;

    if (buf == NULL || func_decl == NULL || mir_routine == NULL || block == NULL || ctx == NULL)
        return false;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';

    if (out_ssa_map != NULL)
        ssa_map_out = out_ssa_map;
    if (!transpiler_emit_mir_block_with_ssa_map(ssa_map_out, block)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR emission failed for block %zu in %s: missing SSA entry map",
                     block->id, func_decl->type == AST_FUNC_DECL
                     ? func_decl->data.func_decl.name
                     : func_decl->data.intent_decl.name);
        return false;
    }
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char base[128];
        size_t version = 0;
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        if (!transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version))
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map_out, base, inst->result_name))
            return false;
    }

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map_out;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        ASTNode *stmt = inst->ast;

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && inst->arg0 != NULL
            && inst->result_name != NULL
            && strcmp(inst->arg0, stmt->data.let_decl.name) == 0) {
            const char *saved_type_hint = ctx->active_type_hint;
            char *rendered_type_hint = NULL;
            char *local_type_name_owned = NULL;
            char *lhs = NULL;
            char *rhs = NULL;

            if (stmt->data.let_decl.type != NULL) {
                rendered_type_hint = render_type_name(stmt->data.let_decl.type);
                ctx->active_type_hint = rendered_type_hint;
            }
            if (rendered_type_hint != NULL)
                local_type_name_owned = pergyra_strdup(rendered_type_hint);
            else if (stmt->data.let_decl.initializer != NULL) {
                const char *inferred =
                    infer_expression_type_name(ctx, stmt->data.let_decl.initializer);
                if (inferred != NULL)
                    local_type_name_owned = pergyra_strdup(inferred);
            }
            if (transpiler_type_name_is_slot_like(local_type_name_owned)
                || (local_type_name_owned != NULL
                    && strncmp(local_type_name_owned, "Channel<", 8) == 0)) {
                emit_statement(stmt, ctx);
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(local_type_name_owned);
                continue;
            }

            lhs = transpiler_make_c_ssa_name(inst->result_name);
            rhs = emit_expression_with_ssa_map(stmt->data.let_decl.initializer, ctx, ssa_map_out);
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);

            if (rhs == NULL) {
                free(lhs);
                free(local_type_name_owned);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %zu emission failed: unable to render initializer for '%s'",
                             block->id,
                             stmt->data.let_decl.name != NULL
                                 ? stmt->data.let_decl.name
                                 : "<binding>");
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (!transpiler_ssa_name_map_set(ssa_map_out, stmt->data.let_decl.name,
                                             inst->result_name)) {
                free(local_type_name_owned);
                ok = false;
                break;
            }
            if (local_type_name_owned != NULL)
                register_typed_var(ctx, stmt->data.let_decl.name, local_type_name_owned);
            free(local_type_name_owned);
            continue;
        }

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && inst->arg0 != NULL
            && inst->result_name != NULL) {
            const char *target_name = stmt->data.assignment.target->data.identifier.name;
            char *lhs = NULL;
            char *rhs = NULL;

            if (target_name == NULL || strcmp(inst->arg0, target_name) != 0)
                continue;
            if (!transpiler_has_explicit_local_binding(func_decl, target_name)) {
                emit_statement(stmt, ctx);
                continue;
            }
            if (transpiler_type_name_is_slot_like(lookup_typed_var(ctx, target_name))) {
                emit_statement(stmt, ctx);
                continue;
            }

            lhs = transpiler_make_c_ssa_name(inst->result_name);
            rhs = emit_expression_with_ssa_map(stmt->data.assignment.value, ctx, ssa_map_out);
            if (rhs == NULL) {
                free(lhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %zu emission failed: unable to render assignment to '%s'",
                             block->id, target_name != NULL ? target_name : "<target>");
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (!transpiler_ssa_name_map_set(ssa_map_out, target_name, inst->result_name)) {
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind != MIR_INST_STMT)
            continue;
        if (stmt == NULL
            || stmt->type == AST_IF_STMT
            || stmt->type == AST_WHILE_LOOP
            || stmt->type == AST_RETURN) {
            continue;
        }
        emit_statement(stmt, ctx);
    }

    ctx->active_ssa_map = saved_active_ssa_map;
    return ok;
}

static bool
transpiler_can_emit_function_from_mir(const TranspilerCtx *ctx,
                                      const ASTNode *func_decl,
                                      const MIRRoutine **mir_routine_out)
{
    return transpiler_can_emit_function_from_mir_with_reason(
        ctx, func_decl, mir_routine_out, NULL, 0);
}

static bool
transpiler_can_emit_intent_cleanup_from_mir(const TranspilerCtx *ctx,
                                            const ASTNode *intent_decl,
                                            const MIRRoutine **mir_routine_out)
{
    return transpiler_can_emit_intent_cleanup_from_mir_with_reason(
        ctx, intent_decl, mir_routine_out, NULL, 0);
}

static bool
transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                  const ASTNode *expr,
                                  const TranspilerSSANameMap *ssa_map,
                                  const char *routine_name,
                                  char *reason,
                                  size_t reason_cap)
{
    if (expr == NULL)
        return true;
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *name = expr->data.identifier.name;
        if (transpiler_resolve_ssa_name(ssa_map, expr->data.identifier.name) != NULL)
            return true;
        if (transpiler_name_is_token_local(name))
            return true;
        if (ctx != NULL && find_enum_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL && lookup_enum_variant_qualified_name((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL && find_function_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR contract breach in %s at line %u: unresolved identifier `%s` (expected SSA-mapped local)",
                     routine_name != NULL ? routine_name : "<routine>",
                     expr->line,
                     expr->data.identifier.name);
        }
        return false;
    }

    switch (expr->type) {
        case AST_BINARY:
            if (!transpiler_expr_identifiers_mapped(ctx, expr->data.binary.left, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, expr->data.binary.right, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_UNARY:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.unary.operand, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_IDENTIFIER
                && expr->data.call.callee->data.identifier.name != NULL
                && (strcmp(expr->data.call.callee->data.identifier.name, "ToObject") == 0
                    || strcmp(expr->data.call.callee->data.identifier.name, "ToTObject") == 0)) {
                for (size_t i = 1; i < expr->data.call.arg_count; i++) {
                    if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.arguments[i], ssa_map,
                                                           routine_name, reason, reason_cap))
                        return false;
                }
                return true;
            }
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.callee, ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            } else if (expr->data.call.callee != NULL
                       && expr->data.call.callee->type == AST_IDENTIFIER
                       && expr->data.call.callee->data.identifier.name != NULL) {
                const char *call_target = expr->data.call.callee->data.identifier.name;
                if (transpiler_resolve_ssa_name(ssa_map, call_target) != NULL
                    && !transpiler_expr_identifiers_mapped(ctx, expr->data.call.callee, ssa_map,
                                                          routine_name, reason, reason_cap))
                    return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.arguments[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.member.object, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_ACCESS:
            if (!transpiler_expr_identifiers_mapped(ctx, expr->data.array_access.array, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, expr->data.array_access.index, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.array_literal.elements[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        }
        case AST_ASSIGNMENT:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.assignment.target, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, expr->data.assignment.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_AWAIT_EXPR:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.await_expr.expression, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_SEND:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.channel_send.channel, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, expr->data.channel_send.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_RECV:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.channel_recv.channel, ssa_map,
                                                     routine_name, reason, reason_cap);
        default:
            return true;
    }
}

static bool
transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                             const MIRRoutine *routine,
                                             const ASTNode *func_decl,
                                             bool require_non_cleanup,
                                             char *reason,
                                             size_t reason_cap)
{
    const char *routine_name = routine != NULL && routine->name != NULL ? routine->name : "<routine>";
    if (routine == NULL || routine->blocks == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        TranspilerSSANameMap ssa_map = {0};

        if (block == NULL || (require_non_cleanup && block->is_cleanup)
            || !block->is_reachable)
            continue;
        if (!transpiler_emit_mir_block_with_ssa_map(&ssa_map, block))
            return false;
        /* Add function/intent parameters to SSA map - they're valid C identifiers, not SSA vars */
        if (func_decl != NULL) {
            if (func_decl->type == AST_FUNC_DECL) {
                for (size_t p = 0; p < func_decl->data.func_decl.param_count; p++) {
                    FuncParam *param = func_decl->data.func_decl.params[p];
                    if (param != NULL && param->name != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, param->name, param->name);
                    }
                }
                transpiler_register_with_alias_bindings_in_block(&ssa_map,
                    func_decl->data.func_decl.body);
            } else if (func_decl->type == AST_INTENT_DECL) {
                /* For intent, extract alias from involves/value bindings */
                for (size_t p = 0; p < func_decl->data.intent_decl.involve_count; p++) {
                    ASTNode *involves = func_decl->data.intent_decl.involves[p];
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                        && involves->data.intent_involves.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, involves->data.intent_involves.alias, involves->data.intent_involves.alias);
                    }
                }
                for (size_t p = 0; p < func_decl->data.intent_decl.value_count; p++) {
                    ASTNode *value = func_decl->data.intent_decl.values[p];
                    if (value != NULL && value->type == AST_INTENT_VALUE
                        && value->data.intent_value.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, value->data.intent_value.alias,
                            value->data.intent_value.alias);
                    }
                }
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if ((inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
                 || inst->kind == MIR_INST_RESOURCE_OP || inst->kind == MIR_INST_CLEANUP_EDGE)
                && inst->ast != NULL) {
                if (!transpiler_expr_identifiers_mapped(ctx, inst->ast, &ssa_map, routine_name,
                                                       reason, reason_cap))
                    return false;
            }
            if (inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) {
                char base[128];
                size_t version = 0;
                const char *base_name = (inst->kind == MIR_INST_PHI
                                         ? inst->name : inst->arg0);
                if (base_name != NULL && base_name[0] != '\0'
                    && inst->result_name != NULL
                    && transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                    if (!transpiler_ssa_name_map_set(&ssa_map, base_name, inst->result_name))
                        return false;
                }
            }
        }
    }
    return true;
}

static bool
transpiler_validate_mir_emission_contract(const TranspilerCtx *ctx,
                                         const MIRRoutine *routine,
                                         const ASTNode *decl,
                                         bool require_cleanup,
                                         bool require_cleanup_blocks,
                                         char *reason,
                                         size_t reason_cap)
{
    const char *routine_name = "<routine>";
    const char *decl_name = NULL;

    if (decl != NULL) {
        if (decl->type == AST_FUNC_DECL && decl->data.func_decl.name != NULL)
            decl_name = decl->data.func_decl.name;
        if (decl->type == AST_INTENT_DECL && decl->data.intent_decl.name != NULL)
            decl_name = decl->data.intent_decl.name;
    }
    routine_name = decl_name != NULL ? decl_name
        : (routine != NULL && routine->name != NULL ? routine->name : "<routine>");

    if (routine == NULL || routine->blocks == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "MIR contract invalid for %s: no routine", routine_name);
        return false;
    }

    {
        char *topology_error = NULL;
        if (!mir_validate_emission_topology(routine,
                                           require_cleanup,
                                           !require_cleanup_blocks,
                                           &topology_error)) {
            if (reason != NULL && reason_cap > 0) {
                if (topology_error != NULL)
                    snprintf(reason, reason_cap, "%s", topology_error);
                else
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: topology validation failed",
                             routine_name);
            }
            free(topology_error);
            return false;
        }
        free(topology_error);
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        if (block == NULL)
            return false;

        if (!transpiler_validate_mir_emission_block_shape(block,
                                                          routine_name,
                                                          require_cleanup,
                                                          reason,
                                                          reason_cap)) {
            return false;
        }

        if (block->has_succ_true && block->succ_true >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad true successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_succ_false && block->succ_false >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad false successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_cleanup_succ && block->cleanup_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad cleanup successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_rollback_succ && block->rollback_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad rollback successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_invalidation_succ && block->invalidation_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad invalidation successor",
                         routine_name, block->id);
            return false;
        }

        for (size_t j = 0; j < block->instruction_count; j++) {
            MIRInstKind kind = block->instructions[j].kind;
            if (kind != MIR_INST_BRANCH && kind != MIR_INST_RETURN
                && kind != MIR_INST_RESOURCE_OP && kind != MIR_INST_CLEANUP_EDGE
                && kind != MIR_INST_PHI && kind != MIR_INST_DEF
                && kind != MIR_INST_STMT) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap, "MIR contract invalid for %s: unsupported instruction kind %d in block %zu",
                             routine_name, (int)kind, block->id);
                return false;
            }
            if ((kind == MIR_INST_BRANCH || kind == MIR_INST_RETURN)
                && require_cleanup_blocks && block->is_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: cleanup block %zu has terminal instruction",
                             routine_name, block->id);
                return false;
            }
        }
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(ctx, routine, decl,
                                                       !require_cleanup_blocks,
                                                       reason,
                                                       reason_cap)) {
        return false;
    }
    return true;
}

static bool
transpiler_validate_mir_emission_block_shape(const MIRBasicBlock *block,
                                            const char *routine_name,
                                            bool require_cleanup,
                                            char *reason,
                                            size_t reason_cap)
{
    bool has_branch = false;
    bool has_return = false;
    size_t branch_count = 0;
    size_t return_count = 0;
    size_t term_index = SIZE_MAX;

    if (block == NULL || routine_name == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH) {
            if (term_index == SIZE_MAX) {
                term_index = i;
            }
            has_branch = true;
            branch_count++;
            if (inst->ast == NULL && (reason != NULL && reason_cap > 0)) {
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu branch instruction misses condition AST",
                         routine_name, block->id);
                return false;
            }
        } else if (inst->kind == MIR_INST_RETURN) {
            if (term_index == SIZE_MAX) {
                term_index = i;
            }
            has_return = true;
            return_count++;
        } else if (inst->kind != MIR_INST_RESOURCE_OP
                   && inst->kind != MIR_INST_DEF
                   && inst->kind != MIR_INST_PHI
                   && inst->kind != MIR_INST_CLEANUP_EDGE
                   && inst->kind != MIR_INST_STMT) {
            continue;
        }

        if (term_index != SIZE_MAX && i > term_index
            && inst->kind != MIR_INST_RESOURCE_OP
            && inst->kind != MIR_INST_CLEANUP_EDGE) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu has non-trailing instructions after terminal",
                         routine_name, block->id);
            return false;
        }
    }

    if (branch_count > 1 || return_count > 1 || (has_branch && has_return)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %zu has %zu branch(es), %zu return(s)",
                     routine_name, block->id, branch_count, return_count);
        return false;
    }

    if (has_branch) {
        if (!block->has_succ_true || !block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu has branch without both true/false successors",
                         routine_name, block->id);
            return false;
        }
        if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
            if (!require_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: block %zu has branch plus exceptional successor",
                             routine_name, block->id);
                return false;
            }
        }
        return true;
    }

    if (has_return) {
        if (block->has_succ_true || block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu return has explicit successors",
                         routine_name, block->id);
            return false;
        }
        if (!require_cleanup
            && (block->has_cleanup_succ || block->has_rollback_succ
                || block->has_invalidation_succ)) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu return has exceptional successor"
                         " without cleanup-capable routine",
                         routine_name, block->id);
            return false;
        }
        return true;
    }

    if (block->has_succ_false) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %zu has false successor without branch",
                     routine_name, block->id);
        return false;
    }

    if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
        if (!require_cleanup) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu has exceptional successor without cleanup-capable routine",
                         routine_name, block->id);
            return false;
        }
    }

    return true;
}

static bool
transpiler_can_emit_function_from_mir_with_reason(const TranspilerCtx *ctx,
                                                 const ASTNode *func_decl,
                                                 const MIRRoutine **mir_routine_out,
                                                 char *reason,
                                                 size_t reason_cap)
{
    const MIRRoutine *routine = transpiler_find_mir_function(ctx, func_decl);
    if (mir_routine_out != NULL)
        *mir_routine_out = NULL;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';
    if (routine == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function cannot lower to MIR: no matching MIR routine");
        return false;
    }
    if (routine->kind != MIR_SCOPE_FUNCTION) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has wrong MIR kind: %d", func_decl->data.func_decl.name, routine->kind);
        return false;
    }
    if (routine->ast == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has no declaration AST in MIR", func_decl->data.func_decl.name);
        return false;
    }
    /* cleanup blocks are now fully supported - removed restriction */
    if (!transpiler_mir_function_signature_supported(func_decl)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has unsupported MIR signature", func_decl->data.func_decl.name);
        return false;
    }
    const bool requires_cleanup = routine->has_cleanup_block;
    if (!transpiler_validate_mir_emission_contract(ctx,
                                                   routine,
                                                   func_decl,
                                                   requires_cleanup,
                                                   requires_cleanup,
                                                   reason,
                                                   reason_cap)) {
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(ctx, routine, func_decl, true, reason, reason_cap)) {
        return false;
    }
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

bool
transpiler_can_emit_function_from_mir_with_reason_for_test(
    const ASTNode *func_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap)
{
    bool can_emit;
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "Out of memory while creating transpiler context");
        return false;
    }
    ctx->mir = mir;
    can_emit = transpiler_can_emit_function_from_mir_with_reason(ctx, func_decl, NULL,
                                                                reason, reason_cap);
    transpiler_ctx_destroy(ctx);
    return can_emit;
}

static bool
transpiler_can_emit_intent_cleanup_from_mir_with_reason(const TranspilerCtx *ctx,
                                                       const ASTNode *intent_decl,
                                                       const MIRRoutine **mir_routine_out,
                                                       char *reason,
                                                       size_t reason_cap)
{
    const MIRRoutine *routine = transpiler_find_mir_intent(ctx, intent_decl);
    if (mir_routine_out != NULL)
        *mir_routine_out = NULL;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';
    if (routine == NULL || intent_decl == NULL || intent_decl->type != AST_INTENT_DECL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent cannot lower to MIR: no matching MIR routine (found %zu routines)", ctx != NULL && ctx->mir != NULL ? ctx->mir->routine_count : 0);
        return false;
    }
    if (routine->kind != MIR_SCOPE_INTENT
        || routine->ast == NULL
        || !routine->has_cleanup_block) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent %s has no MIR cleanup section (kind=%d, ast=%p, has_cleanup_block=%d)",
                intent_decl->data.intent_decl.name, routine->kind, (void*)routine->ast, routine->has_cleanup_block);
        return false;
    }
    if (!transpiler_validate_mir_emission_contract(ctx,
                                                   routine,
                                                   intent_decl,
                                                   true,
                                                   true,
                                                   reason,
                                                   reason_cap)) {
        if (reason != NULL && reason_cap > 0 && reason[0] == '\0')
            snprintf(reason, reason_cap, "intent %s MIR emission contract validation failed", intent_decl->data.intent_decl.name);
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(ctx, routine, intent_decl, false, reason, reason_cap)) {
        if (reason != NULL && reason_cap > 0 && reason[0] == '\0')
            snprintf(reason, reason_cap, "intent %s SSA mapping incomplete", intent_decl->data.intent_decl.name);
        return false;
    }
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

bool
transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(
    const ASTNode *intent_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap)
{
    bool can_emit;
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "Out of memory while creating transpiler context");
        return false;
    }
    ctx->mir = mir;
    can_emit = transpiler_can_emit_intent_cleanup_from_mir_with_reason(ctx, intent_decl, NULL,
                                                                       reason, reason_cap);
    transpiler_ctx_destroy(ctx);
    return can_emit;
}

static bool
transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                  CodeBuf *out,
                                  int indent,
                                  const MIRInstruction *inst,
                                  const char *handle_expr,
                                  bool cleanup_hook)
{
    const char *slot_anchor = "";
    const char *arg_name = "";
    const char *helper = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";

    if (out == NULL || inst == NULL)
        return false;

    if (inst->kind == MIR_INST_RESOURCE_OP) {
        bool claim_already_materialized_by_stmt = false;
        bool is_claim_op = inst->name != NULL && strcmp(inst->name, "Claim") == 0;
        if (!cleanup_hook
            && is_claim_op) {
            const char *claim_name = inst->slot_anchor != NULL
                ? inst->slot_anchor
                : inst->arg0;
            const char *existing_type = claim_name != NULL
                ? lookup_typed_var(ctx, claim_name)
                : NULL;
            if (inst->ast != NULL
                && inst->ast->type == AST_LET_DECL
                && inst->ast->data.let_decl.name != NULL
                && claim_name != NULL
                && strcmp(claim_name, inst->ast->data.let_decl.name) == 0) {
                claim_already_materialized_by_stmt = true;
            } else if (claim_name != NULL
                       && existing_type != NULL
                       && (strncmp(existing_type, "Slot<", 5) == 0
                           || strncmp(existing_type, "SecureSlot<", 11) == 0
                           || strncmp(existing_type, "DeviceSlot<", 11) == 0)) {
                claim_already_materialized_by_stmt = true;
            }
        }
        bool needs_concrete_emit = (cleanup_hook && !is_claim_op)
            || (is_claim_op && !claim_already_materialized_by_stmt && !cleanup_hook);
        if (needs_concrete_emit) {
            if (!transpiler_emit_mir_resource_op(ctx, out, indent, inst, inst->type_layout, NULL)) {
                if (inst->name != NULL
                    && (strcmp(inst->name, "ProjectRefresh") == 0
                        || strcmp(inst->name, "ProjectPublish") == 0)) {
                    /* Direct projection expressions already emit the concrete value
                     * via MIR DEF/STMT paths; keep only the observability/export hook
                     * for projection resource ops that do not have a slot runtime ABI. */
                } else {
                    if (ctx != NULL && ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "cannot emit MIR resource op '%s' for slot '%s': missing typed runtime layout",
                            inst->name != NULL ? inst->name : "<op>",
                            inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
                    }
                    return false;
                }
            }
            if (!cleanup_hook)
                return true;
        }
    }

    if (inst->name != NULL) {
        codebuf_write(out, "%*s%s(%s, ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
        codebuf_write(out, "\"%s\", ", inst->name);
    } else {
        codebuf_write(out, "%*s%s(%s, \"unknown\", ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
    }

    if (inst->rir_op != NULL && inst->rir_op->slot_anchor != NULL)
        slot_anchor = inst->rir_op->slot_anchor;
    else if (inst->arg0 != NULL)
        slot_anchor = inst->arg0;
    if (inst->arg1 != NULL)
        arg_name = inst->arg1;
    else if (inst->rir_op != NULL && inst->rir_op->arg0 != NULL)
        arg_name = inst->rir_op->arg0;

    codebuf_write(out, "\"%s\", \"%s\");\n",
        slot_anchor != NULL ? slot_anchor : "",
        arg_name != NULL ? arg_name : "");
    return true;
}

static void
