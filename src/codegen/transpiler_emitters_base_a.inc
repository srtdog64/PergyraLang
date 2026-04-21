static int transpiler_find_loop_label_depth(const TranspilerCtx *ctx,
                                            const char *label);
static ASTNode *transpiler_current_host_decl_local(TranspilerCtx *ctx);
static bool transpiler_is_implicit_field(TranspilerCtx *ctx,
                                         const char *base_name);
static bool transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                              const ASTNode *expr,
                                              const TranspilerSSANameMap *ssa_map,
                                              const char *routine_name,
                                              char *reason,
                                              size_t reason_cap);
static bool transpiler_emit_mir_phi_copies(CodeBuf *buf,
                                           TranspilerCtx *ctx,
                                           int indent,
                                           size_t pred_block_index,
                                           const MIRBasicBlock *pred_block,
                                           const MIRBasicBlock *target_block);
static const char *transpiler_find_prior_block_ssa_name(const MIRBasicBlock *block,
                                                        size_t limit_inst_index,
                                                        const char *base_name);
static const char *transpiler_find_block_exit_ssa_name(const MIRBasicBlock *block,
                                                       const char *base_name);
static bool transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                                     size_t inst_index,
                                                     const ASTNode *expr,
                                                     TranspilerSSANameMap *ssa_map_out);
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
static char *transpiler_render_effective_local_type_name(TranspilerCtx *ctx,
                                                         ASTNode *type_node);
/* Forward declarations for generic class monomorphization */
static bool class_has_generic_params(ASTNode *node);
static const char *ensure_generic_class_specialization(
    TranspilerCtx *ctx, ASTNode *class_decl, ASTNode *ann);

/* -----------------------------------------------------------------
 * Let declaration emitter
 * ----------------------------------------------------------------- */

#include "transpiler_emitters_type_require.inc"


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
        if (strcmp(callee_name, "Box") == 0
            && find_class_decl(ctx, callee_name) == NULL) {
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
        else if (strcmp(callee_name, "Rc") == 0
                 && find_class_decl(ctx, callee_name) == NULL) {
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
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine C type for let binding '%s'; explicit annotation or resolvable initializer type is required",
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

#include "transpiler_emitters_mir_inventory_ssa.inc"

static ASTNode *
transpiler_find_let_decl_by_name_in_block(ASTNode *body, const char *name)
{
    if (body == NULL || name == NULL)
        return NULL;

    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_let_decl_by_name_in_block(
                body->data.block.statements[i], name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }

    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, name) == 0) {
        return body;
    }

    if (body->type == AST_WITH_STMT)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.with_stmt.body, name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.then_branch, name);
        if (found != NULL)
            return found;
        return transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.else_branch, name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.while_loop.body, name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.for_loop.body, name);

    return NULL;
}

static ASTNode *
transpiler_find_let_decl_by_name(const ASTNode *func_decl, const char *name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    return transpiler_find_let_decl_by_name_in_block(
        func_decl->data.func_decl.body, name);
}

static bool
transpiler_materialize_pending_inst_uses(CodeBuf *buf,
                                         TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         const MIRBasicBlock *block,
                                         const MIRInstruction *inst,
                                         TranspilerSSANameMap *ssa_map_out,
                                         int indent,
                                         bool emit_assignments,
                                         char *reason,
                                         size_t reason_cap)
{
    if (ctx == NULL || block == NULL || inst == NULL || ssa_map_out == NULL)
        return true;

    for (size_t i = 0; i < inst->use_count; i++) {
        const char *versioned_use = inst->uses[i];
        const char *exit_versioned;
        ASTNode *let_decl;
        ASTNode *initializer;
        const char *existing_type;
        ASTNode *binding_type_ast;
        char *binding_type_name = NULL;
        char base[128];
        size_t version = 0;
        char *lhs = NULL;
        char *rhs = NULL;
        const char *value_type = NULL;
        char *rendered_type = NULL;

        if (versioned_use == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned_use, base, sizeof(base), &version))
            continue;
        if (transpiler_resolve_ssa_name(ssa_map_out, base) != NULL)
            continue;
        if (is_slot_var(ctx, base))
            continue;
        existing_type = lookup_typed_var(ctx, base);
        if (existing_type != NULL
            && (transpiler_type_name_is_slot_like(existing_type)
                || transpiler_type_name_is_claim_shape(existing_type)
                || strncmp(existing_type, "Channel<", 8) == 0)) {
            continue;
        }
        binding_type_ast = transpiler_find_local_type_ast(ctx, func_decl, base);
        if (binding_type_ast != NULL) {
            binding_type_name = transpiler_render_effective_local_type_name(
                ctx, binding_type_ast);
            if (binding_type_name != NULL
                && (transpiler_type_name_is_slot_like(binding_type_name)
                    || transpiler_type_name_is_claim_shape(binding_type_name)
                    || strncmp(binding_type_name, "Channel<", 8) == 0)) {
                free(binding_type_name);
                continue;
            }
            free(binding_type_name);
            binding_type_name = NULL;
        }

        exit_versioned = transpiler_find_block_exit_ssa_name(block, base);
        if (exit_versioned == NULL)
            continue;

        let_decl = NULL;
        if (block->source_statements != NULL) {
            for (size_t stmt_idx = 0; stmt_idx < block->source_statement_count; stmt_idx++) {
                ASTNode *source_stmt = block->source_statements[stmt_idx];
                if (source_stmt == NULL || source_stmt->type != AST_LET_DECL)
                    continue;
                if (source_stmt->data.let_decl.name != NULL
                    && strcmp(source_stmt->data.let_decl.name, base) == 0) {
                    let_decl = source_stmt;
                    break;
                }
            }
        }
        if (let_decl == NULL)
            let_decl = transpiler_find_let_decl_by_name(func_decl, base);
        if (let_decl == NULL || let_decl->type != AST_LET_DECL)
            continue;
        initializer = let_decl->data.let_decl.initializer;
        if (initializer == NULL)
            continue;
        if (initializer->type == AST_CALL
            && initializer->data.call.callee != NULL
            && initializer->data.call.callee->type == AST_IDENTIFIER
            && initializer->data.call.callee->data.identifier.name != NULL
            && strncmp(initializer->data.call.callee->data.identifier.name,
                       "Claim", 5) == 0) {
            continue;
        }

        if (emit_assignments) {
            lhs = transpiler_render_ssa_name(ctx, exit_versioned);
            rhs = emit_expression_with_ssa_map(initializer, ctx, ssa_map_out);
            if (lhs == NULL || rhs == NULL) {
                free(lhs);
                free(rhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to materialize pending value '%s'",
                             (unsigned long long) block->id, base);
                }
                return false;
            }
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }

        if (!transpiler_ssa_name_map_set(ssa_map_out, base, exit_versioned))
            return false;

        if (let_decl->data.let_decl.type != NULL) {
            rendered_type = transpiler_render_effective_local_type_name(
                ctx, let_decl->data.let_decl.type);
            value_type = rendered_type;
        } else {
            value_type = infer_expression_type_name(ctx, initializer);
        }
        if (value_type != NULL
            && value_type[0] != '\0'
            && strcmp(value_type, "Void") != 0) {
            register_typed_var(ctx, base, value_type);
        }
        free(rendered_type);
    }

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
    bool source_order_mode = false;
    size_t *inst_order = NULL;
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
                     "MIR emission failed for block %llu in %s: missing SSA entry map",
                     (unsigned long long) block->id, func_decl->type == AST_FUNC_DECL
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

    source_order_mode = !transpiler_mir_routine_has_explicit_cfg(mir_routine)
        && block->source_statement_count > 0
        && block->source_statements != NULL
        && block->instruction_count > 0;
    if (source_order_mode) {
        bool *scheduled = calloc(block->instruction_count, sizeof(bool));
        inst_order = calloc(block->instruction_count, sizeof(size_t));
        size_t order_count = 0;

        if (scheduled == NULL || inst_order == NULL) {
            free(scheduled);
            free(inst_order);
            ctx->active_ssa_map = saved_active_ssa_map;
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %llu emission failed: unable to allocate source-order schedule",
                         (unsigned long long) block->id);
            }
            return false;
        }

        for (size_t stmt_idx = 0; stmt_idx < block->source_statement_count; stmt_idx++) {
            ASTNode *source_stmt = block->source_statements[stmt_idx];
            for (size_t inst_idx = 0; inst_idx < block->instruction_count; inst_idx++) {
                const MIRInstruction *inst = &block->instructions[inst_idx];
                bool attach_to_source_stmt = false;
                if (scheduled[inst_idx])
                    continue;
                if (inst->ast == source_stmt) {
                    attach_to_source_stmt = true;
                } else if (source_stmt != NULL
                           && inst->kind == MIR_INST_DEF
                           && inst->arg0 != NULL) {
                    if (source_stmt->type == AST_LET_DECL
                        && source_stmt->data.let_decl.name != NULL
                        && strcmp(inst->arg0, source_stmt->data.let_decl.name) == 0) {
                        attach_to_source_stmt = true;
                    } else if (source_stmt->type == AST_ASSIGNMENT
                               && source_stmt->data.assignment.target != NULL
                               && source_stmt->data.assignment.target->type == AST_IDENTIFIER
                               && source_stmt->data.assignment.target->data.identifier.name != NULL
                               && strcmp(inst->arg0,
                                         source_stmt->data.assignment.target->data.identifier.name) == 0) {
                        attach_to_source_stmt = true;
                    }
                }
                if (!attach_to_source_stmt)
                    continue;
                inst_order[order_count++] = inst_idx;
                scheduled[inst_idx] = true;
            }
        }
        for (size_t inst_idx = 0; inst_idx < block->instruction_count; inst_idx++) {
            if (scheduled[inst_idx])
                continue;
            inst_order[order_count++] = inst_idx;
        }
        free(scheduled);
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_RESOURCE_OP)
            continue;
        if (inst->name == NULL || strcmp(inst->name, "Claim") != 0)
            continue;
        if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, inst, "0", false)) {
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %zu emission failed: unable to emit claim op for '%s'",
                         block->id,
                         inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
            }
            ok = false;
            break;
        }
    }

    if (!ok) {
        free(inst_order);
        ctx->active_ssa_map = saved_active_ssa_map;
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        size_t inst_index = source_order_mode ? inst_order[i] : i;
        const MIRInstruction *inst = &block->instructions[inst_index];
        ASTNode *stmt = inst->ast;

        if (!transpiler_materialize_pending_inst_uses(buf, ctx, func_decl, block,
                                                      inst, ssa_map_out, ctx->indent,
                                                      true, reason, reason_cap)) {
            ok = false;
            break;
        }

        if (inst->kind == MIR_INST_RESOURCE_OP) {
            if (inst->name != NULL
                && strcmp(inst->name, "Write") == 0
                && inst->ast != NULL
                && inst->ast->type == AST_CALL) {
                ASTNode *callee = inst->ast->data.call.callee;
                ASTNode *value_expr = NULL;
                char map_reason[256];
                if (callee != NULL
                    && callee->type == AST_IDENTIFIER
                    && callee->data.identifier.name != NULL
                    && strcmp(callee->data.identifier.name, "Write") == 0
                    && inst->ast->data.call.arg_count >= 2) {
                    value_expr = inst->ast->data.call.arguments[1];
                } else if (callee != NULL
                           && callee->type == AST_MEMBER_ACCESS
                           && callee->data.member.name != NULL
                           && strcmp(callee->data.member.name, "Write") == 0
                           && inst->ast->data.call.arg_count >= 1) {
                    value_expr = inst->ast->data.call.arguments[0];
                }
                if (value_expr != NULL) {
                    if (!transpiler_expr_identifiers_mapped(
                            ctx,
                            value_expr,
                            (const TranspilerSSANameMap *)ssa_map_out,
                            mir_routine->name,
                            map_reason,
                            sizeof(map_reason))) {
                        continue;
                    }
                    if (!transpiler_seed_expr_identifier_mappings(
                            block, inst_index, value_expr, ssa_map_out)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (inst->arg1 != NULL
                && transpiler_resolve_ssa_name(
                       (const TranspilerSSANameMap *)ssa_map_out,
                       inst->arg1) == NULL) {
                const char *mapped_value = transpiler_find_prior_block_ssa_name(
                    block, inst_index, inst->arg1);
                if (mapped_value == NULL)
                    mapped_value = transpiler_find_block_exit_ssa_name(block, inst->arg1);
                if (mapped_value != NULL) {
                    if (!transpiler_ssa_name_map_set(ssa_map_out, inst->arg1, mapped_value)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (inst->name != NULL && strcmp(inst->name, "Claim") == 0)
                continue;
            if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, inst, "0", false)) {
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to emit resource op '%s'",
                             (unsigned long long) block->id,
                             inst->name != NULL ? inst->name : "<op>");
                }
                ok = false;
                break;
            }
            continue;
        }

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
            if (transpiler_type_name_is_slot_like(local_type_name_owned)) {
                bool claim_backed_slot = transpiler_block_has_claim_for_slot_local(
                    block, stmt->data.let_decl.name);
                if (claim_backed_slot) {
                    ctx->active_type_hint = saved_type_hint;
                    free(rendered_type_hint);
                    free(local_type_name_owned);
                    continue;
                }
                emit_statement(stmt, ctx);
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(local_type_name_owned);
                continue;
            }
            if (local_type_name_owned != NULL
                && strncmp(local_type_name_owned, "Channel<", 8) == 0) {
                emit_statement(stmt, ctx);
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(local_type_name_owned);
                continue;
            }

            if (stmt->data.let_decl.type != NULL) {
                ensure_type_specializations_from_ast_to(ctx, ctx->out,
                                                        stmt->data.let_decl.type);
            } else if (local_type_name_owned != NULL) {
                ensure_result_specialization_from_type_name_to(ctx, ctx->out,
                                                               local_type_name_owned);
            }

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            {
                const char *saved_expected_type = ctx->expected_type;
                ctx->expected_type = local_type_name_owned;
                rhs = emit_expression_with_ssa_map(stmt->data.let_decl.initializer,
                                                   ctx, ssa_map_out);
                ctx->expected_type = saved_expected_type;
            }
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);

            if (rhs == NULL) {
                free(lhs);
                free(local_type_name_owned);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render initializer for '%s'",
                             (unsigned long long) block->id,
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
            bool target_is_field = false;
            bool is_local_binding = false;

            if (target_name == NULL || strcmp(inst->arg0, target_name) != 0)
                continue;
            is_local_binding = transpiler_has_explicit_local_binding(func_decl, target_name);
            if (!is_local_binding) {
                if (current_class_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_relation_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_effect_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_zone_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_world_has_field(ctx, target_name))
                    target_is_field = true;
            }
            if (!target_is_field && inst->slot_anchor != NULL) {
                const char *slot_anchor = inst->slot_anchor;
                if (current_class_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_relation_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_effect_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_zone_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_world_has_field(ctx, slot_anchor))
                    target_is_field = true;
            }
            if (target_is_field) {
                char *field_lhs = NULL;
                char *field_rhs = NULL;

                if (stmt->data.assignment.value != NULL
                    && stmt->data.assignment.value->type == AST_IDENTIFIER
                    && stmt->data.assignment.value->data.identifier.name != NULL
                    && transpiler_resolve_ssa_name(
                           (const TranspilerSSANameMap *)ssa_map_out,
                           stmt->data.assignment.value->data.identifier.name) == NULL) {
                    const char *prior_ssa = transpiler_find_prior_block_ssa_name(
                        block, inst_index, stmt->data.assignment.value->data.identifier.name);
                    if (prior_ssa != NULL) {
                        transpiler_ssa_name_map_set(ssa_map_out,
                            stmt->data.assignment.value->data.identifier.name,
                            prior_ssa);
                    }
                }
                field_lhs = emit_expression_with_ssa_map(
                    stmt->data.assignment.target, ctx, ssa_map_out);
                field_rhs = emit_expression_with_ssa_map(
                    stmt->data.assignment.value, ctx, ssa_map_out);
                if (field_lhs == NULL || field_rhs == NULL) {
                    free(field_lhs);
                    free(field_rhs);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: unable to render field assignment to '%s'",
                                 (unsigned long long) block->id,
                                 target_name != NULL ? target_name : "<field>");
                    }
                    ok = false;
                    break;
                }
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", field_lhs, field_rhs);
                free(field_lhs);
                free(field_rhs);
                continue;
            }
            if (!is_local_binding) {
                emit_statement(stmt, ctx);
                continue;
            }
            if (transpiler_type_name_is_slot_like(lookup_typed_var(ctx, target_name))) {
                emit_statement(stmt, ctx);
                continue;
            }

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            rhs = emit_expression_with_ssa_map(stmt->data.assignment.value, ctx, ssa_map_out);
            if (rhs == NULL) {
                free(lhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render assignment to '%s'",
                             (unsigned long long) block->id, target_name != NULL ? target_name : "<target>");
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

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type != AST_LET_DECL
            && stmt->type != AST_ASSIGNMENT
            && inst->result_name != NULL) {
            ASTNode *binding_type_ast = NULL;
            char *lhs = NULL;
            char *rhs = NULL;
            const char *binding_name = inst->arg0;
            const char *value_type = NULL;

            if (binding_name != NULL)
                binding_type_ast =
                    transpiler_find_local_type_ast(ctx, func_decl, binding_name);
            if (binding_type_ast != NULL) {
                ensure_type_specializations_from_ast_to(ctx, ctx->decls,
                                                        binding_type_ast);
                value_type = transpiler_find_local_type_name(ctx, func_decl,
                                                             binding_name);
            }
            if (value_type == NULL || strcmp(value_type, "Unknown") == 0)
                value_type = infer_expression_type_name(ctx, stmt);

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            {
                const char *saved_expected_type = ctx->expected_type;
                ctx->expected_type = value_type;
                rhs = emit_expression_with_ssa_map(stmt, ctx, ssa_map_out);
                ctx->expected_type = saved_expected_type;
            }
            if (lhs == NULL || rhs == NULL) {
                free(lhs);
                free(rhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render DEF for '%s'",
                             (unsigned long long) block->id,
                             binding_name != NULL ? binding_name : inst->result_name);
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (binding_name != NULL) {
                if (!transpiler_ssa_name_map_set(ssa_map_out,
                                                 binding_name,
                                                 inst->result_name)) {
                    ok = false;
                    break;
                }
                if (value_type != NULL
                    && value_type[0] != '\0'
                    && strcmp(value_type, "Unknown") != 0) {
                    register_typed_var(ctx, binding_name, value_type);
                }
            }
            continue;
        }

        if (inst->kind != MIR_INST_STMT)
            continue;
        if (stmt == NULL || stmt->type == AST_BLOCK || stmt->type == AST_RETURN) {
            continue;
        }
        if (stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && stmt->data.let_decl.initializer != NULL
            && stmt->data.let_decl.initializer->type != AST_CALL
            && transpiler_find_routine_exit_ssa_name(
                   mir_routine, stmt->data.let_decl.name) != NULL) {
            continue;
        }
        if (ctx->mir != NULL) {
            bool mirrored_resource_stmt = false;
            for (size_t ri = 0; ri < block->instruction_count; ri++) {
                const MIRInstruction *resource_inst = &block->instructions[ri];
                bool resource_is_secure = false;
                if (resource_inst->kind != MIR_INST_RESOURCE_OP)
                    continue;
                if (resource_inst->ast != stmt)
                    continue;
                if (resource_inst->name != NULL
                    && strcmp(resource_inst->name, "Read") == 0) {
                    continue;
                }
                if (resource_inst->slot_anchor != NULL)
                    resource_is_secure = lookup_slot_is_secure(ctx, resource_inst->slot_anchor);
                if (resource_is_secure)
                    continue;
                mirrored_resource_stmt = true;
                break;
            }
            if (mirrored_resource_stmt)
                continue;
        }
        if (stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && stmt->data.assignment.target->data.identifier.name != NULL
            && transpiler_is_implicit_field(ctx,
                stmt->data.assignment.target->data.identifier.name)) {
            char map_reason[256];
            if (!transpiler_expr_identifiers_mapped(
                    ctx,
                    stmt->data.assignment.value,
                    ssa_map_out,
                    mir_routine->name,
                    map_reason,
                    sizeof(map_reason))) {
                continue;
            }
        }
        if (stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && stmt->data.assignment.target->data.identifier.name != NULL
            && !transpiler_is_implicit_field(
                   ctx, stmt->data.assignment.target->data.identifier.name)) {
            const char *target_name =
                stmt->data.assignment.target->data.identifier.name;
            const char *mapped_target = transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ssa_map_out,
                target_name);
            if (mapped_target != NULL) {
                char *lhs = transpiler_render_ssa_name(ctx, mapped_target);
                char *rhs = emit_expression_with_ssa_map(
                    stmt->data.assignment.value, ctx, ssa_map_out);
                if (lhs == NULL || rhs == NULL) {
                    free(lhs);
                    free(rhs);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: unable to render local assignment to '%s'",
                                 (unsigned long long) block->id,
                                 target_name);
                    }
                    ok = false;
                    break;
                }
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
                continue;
            }
        }
        if (transpiler_mir_routine_has_explicit_cfg(mir_routine)
            && (stmt->type == AST_WITH_STMT
                || stmt->type == AST_IF_STMT
                || stmt->type == AST_WHILE_LOOP)) {
            continue;
        }
        if (stmt->type == AST_CALL) {
            char *expr = emit_expression_with_ssa_map(stmt, ctx, ssa_map_out);
            if (expr == NULL || expr[0] == '\0') {
                free(expr);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render call statement with SSA mapping",
                             (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s;\n", expr);
            emit_zone_action_effect_runtime(stmt, ctx);
            free(expr);
            continue;
        }
        if (stmt->type == AST_LET_DESTRUCTURE) {
            /* MIR lowering for `let (a, b, c) = rhs`.
             * The routine's SSA header pre-declared `_pgy_ssa_<name>_1 = 0`
             * for each binding; here we emit the initializer into a scratch
             * and assign element-wise into the SSA-renamed targets. */
            ASTNode *init = stmt->data.let_destructure.initializer;
            /* Special case: ClaimSecureSlot destructuring must emit a
             * PgyToken + PgySecureSlot pair (MIR header already skipped
             * SSA pre-decl for the slot/token because their resolved
             * type is Slot-like). */
            if (init != NULL && init->type == AST_CALL
                && init->data.call.callee != NULL
                && init->data.call.callee->type == AST_IDENTIFIER
                && init->data.call.callee->data.identifier.name != NULL
                && stmt->data.let_destructure.name_count == 2) {
                const char *cname =
                    init->data.call.callee->data.identifier.name;
                if (strcmp(cname, "ClaimSecureSlot") == 0) {
                    const char *inner = NULL;
                    if (init->data.call.generic_args != NULL
                        && init->data.call.generic_args->count > 0
                        && init->data.call.generic_args->params[0] != NULL)
                        inner = init->data.call.generic_args->params[0]->name;
                    if (inner == NULL) inner = "Int";
                    const char *slot_name =
                        stmt->data.let_destructure.names[0];
                    const char *token_name =
                        stmt->data.let_destructure.names[1];
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf,
                        "PgyToken_%s %s;\n", inner, token_name);
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf,
                        "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s);\n",
                        inner, slot_name, inner, token_name);
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf, "(void)%s;\n", slot_name);
                    register_slot_var(ctx, slot_name, inner, true, false);
                    set_slot_token_name(ctx, slot_name, token_name);
                    char typed_tok[64];
                    snprintf(typed_tok, sizeof(typed_tok), "Token<%s>", inner);
                    register_typed_var(ctx, token_name, typed_tok);
                    char typed_slot[64];
                    snprintf(typed_slot, sizeof(typed_slot), "SecureSlot<%s>", inner);
                    register_typed_var(ctx, slot_name, typed_slot);
                    transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
                    transpiler_ssa_name_map_set(ssa_map_out, token_name, token_name);
                    continue;
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
                if (init->data.call.generic_args != NULL
                    && init->data.call.generic_args->count > 0
                    && init->data.call.generic_args->params[0] != NULL)
                    inner = init->data.call.generic_args->params[0]->name;
                if (inner == NULL) inner = "Int";
                const char *slot_name =
                    stmt->data.let_destructure.names[0];
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf,
                    "PgySlot_%s %s = pgy_claim_%s();\n",
                    inner, slot_name, inner);
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "(void)%s;\n", slot_name);
                register_slot_var(ctx, slot_name, inner, false, false);
                char typed_slot[64];
                snprintf(typed_slot, sizeof(typed_slot), "Slot<%s>", inner);
                register_typed_var(ctx, slot_name, typed_slot);
                transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
                continue;
            }
            const char *init_type_name =
                infer_expression_type_name(ctx, init);
            if ((init_type_name == NULL || strcmp(init_type_name, "Unknown") == 0)
                && init != NULL
                && init->type == AST_IDENTIFIER
                && init->data.identifier.name != NULL
                && ctx->current_func_decl != NULL) {
                const char *resolved =
                    transpiler_find_local_type_name(ctx, ctx->current_func_decl,
                        init->data.identifier.name);
                if (resolved != NULL)
                    init_type_name = resolved;
            }
            const char *c_init_type =
                pergyra_type_to_c(init_type_name);

            /* Tuple destructuring: let (a, b, ...) = tuple_expr (MIR path) */
            if (init_type_name != NULL && init_type_name[0] == '(') {
                char elem_names[8][64];
                size_t arity = 0;
                {
                    size_t ti = 1;
                    size_t tlen = strlen(init_type_name);
                    while (ti < tlen && init_type_name[ti] != ')' && arity < 8) {
                        while (ti < tlen
                               && (init_type_name[ti] == ' '
                                   || init_type_name[ti] == '\t'))
                            ti++;
                        size_t eo = 0;
                        int depth = 0;
                        while (ti < tlen && eo + 1 < sizeof(elem_names[0])) {
                            char c = init_type_name[ti];
                            if (depth == 0 && (c == ',' || c == ')'))
                                break;
                            if (c == '<' || c == '(') depth++;
                            if (c == '>' || c == ')') depth--;
                            elem_names[arity][eo++] = c;
                            ti++;
                        }
                        elem_names[arity][eo] = '\0';
                        while (eo > 0
                               && (elem_names[arity][eo-1] == ' '
                                   || elem_names[arity][eo-1] == '\t'))
                            elem_names[arity][--eo] = '\0';
                        arity++;
                        if (ti < tlen && init_type_name[ti] == ',') ti++;
                    }
                }
                if (arity != stmt->data.let_destructure.name_count) {
                    if (ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                            (unsigned long long) stmt->data.let_destructure.name_count,
                            (unsigned long long) arity);
                    }
                    ok = false;
                    break;
                }
                char *rhs_t = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
                if (rhs_t == NULL) {
                    ok = false;
                    break;
                }
                int tmp_id = ++ctx->tmp_counter;
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                    c_init_type, tmp_id, rhs_t);
                free(rhs_t);
                for (size_t dn = 0;
                     dn < stmt->data.let_destructure.name_count; dn++) {
                    const char *bname = stmt->data.let_destructure.names[dn];
                    if (bname == NULL)
                        continue;
                    char ssa_versioned_tmp[192];
                    snprintf(ssa_versioned_tmp, sizeof(ssa_versioned_tmp),
                             "%s.1", bname);
                    char *ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
                    if (ssa_versioned == NULL) {
                        ok = false;
                        break;
                    }
                    char *ssa_lhs =
                        transpiler_render_ssa_name(ctx, ssa_versioned);
                    if (ssa_lhs == NULL) {
                        free(ssa_versioned);
                        continue;
                    }
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf,
                        "%s = _pgy_destr_%d.f%zu;\n",
                        ssa_lhs, tmp_id, dn);
                    free(ssa_lhs);
                    register_typed_var(ctx, bname, elem_names[dn]);
                    transpiler_ssa_name_map_set(ssa_map_out, bname,
                        ssa_versioned);
                }
                if (!ok) break;
                continue;
            }

            const char *elem_inner = NULL;
            const char *elem_c_type = NULL;
            if (init_type_name != NULL
                && (strncmp(init_type_name, "Array<", 6) == 0
                    || strncmp(init_type_name, "Slice<", 6) == 0)) {
                elem_inner = slot_inner_type_name(init_type_name);
                elem_c_type = pergyra_type_to_c(elem_inner);
            }
            if (c_init_type == NULL || elem_c_type == NULL
                || elem_inner == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                        init_type_name != NULL
                            ? init_type_name
                            : "(unknown)");
                }
                ok = false;
                break;
            }
            char *rhs = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
            if (rhs == NULL) {
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render destructuring initializer",
                             (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            int tmp_id = ++ctx->tmp_counter;
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                c_init_type, tmp_id, rhs);
            free(rhs);
            for (size_t dn = 0;
                 dn < stmt->data.let_destructure.name_count; dn++) {
                const char *bname = stmt->data.let_destructure.names[dn];
                if (bname == NULL)
                    continue;
                /* The SSA map stores pointers without duplicating, so
                 * allocate a persistent "NAME.1" string per binding. */
                char ssa_versioned_tmp[192];
                snprintf(ssa_versioned_tmp, sizeof(ssa_versioned_tmp),
                         "%s.1", bname);
                char *ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
                if (ssa_versioned == NULL) {
                    ok = false;
                    break;
                }
                char *ssa_lhs =
                    transpiler_render_ssa_name(ctx, ssa_versioned);
                if (ssa_lhs == NULL) {
                    free(ssa_versioned);
                    continue;
                }
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf,
                    "%s = _pgy_destr_%d.data[%zu];\n",
                    ssa_lhs, tmp_id, dn);
                free(ssa_lhs);
                register_typed_var(ctx, bname, elem_inner);
                transpiler_ssa_name_map_set(ssa_map_out, bname,
                    ssa_versioned);
            }
            if (!ok)
                break;
            continue;
        }
        if (stmt != NULL
            && stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && stmt->data.let_decl.initializer != NULL) {
            const char *versioned_local =
                transpiler_find_block_exit_ssa_name(block,
                                                    stmt->data.let_decl.name);
            if (versioned_local == NULL)
                versioned_local = transpiler_resolve_ssa_name(
                    (const TranspilerSSANameMap *)ssa_map_out,
                    stmt->data.let_decl.name);
            if (versioned_local != NULL) {
                char *lhs = transpiler_render_ssa_name(ctx, versioned_local);
                char *rhs = emit_expression_with_ssa_map(
                    stmt->data.let_decl.initializer, ctx, ssa_map_out);
                char *rendered_type = NULL;
                const char *value_type = NULL;

                if (stmt->data.let_decl.type != NULL) {
                    rendered_type = transpiler_render_effective_local_type_name(
                        ctx, stmt->data.let_decl.type);
                    value_type = rendered_type;
                } else {
                    value_type = infer_expression_type_name(
                        ctx, stmt->data.let_decl.initializer);
                }

                if (value_type != NULL
                    && strcmp(value_type, "Unknown") != 0) {
                    ASTNode *binding_type_ast =
                        transpiler_find_local_type_ast(ctx, func_decl,
                                                       stmt->data.let_decl.name);
                    char *binding_type_name = NULL;
                    if (binding_type_ast != NULL)
                        binding_type_name =
                            transpiler_render_effective_local_type_name(ctx,
                                                                        binding_type_ast);
                    if (is_slot_var(ctx, stmt->data.let_decl.name)
                        || (binding_type_name != NULL
                            && (transpiler_type_name_is_slot_like(binding_type_name)
                                || transpiler_type_name_is_claim_shape(binding_type_name)
                                || strncmp(binding_type_name, "Channel<", 8) == 0))
                        || (value_type != NULL
                            && (transpiler_type_name_is_slot_like(value_type)
                                || transpiler_type_name_is_claim_shape(value_type)
                                || strncmp(value_type, "Channel<", 8) == 0))) {
                        free(binding_type_name);
                        free(lhs);
                        free(rhs);
                        free(rendered_type);
                        continue;
                    }
                    free(binding_type_name);
                }

                if (lhs == NULL || rhs == NULL) {
                    free(lhs);
                    free(rhs);
                    free(rendered_type);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %zu emission failed: unable to materialize preserved let-binding '%s'",
                                 block->id,
                                 stmt->data.let_decl.name);
                    }
                    ok = false;
                    break;
                }

                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);

                if (!transpiler_ssa_name_map_set(ssa_map_out,
                                                 stmt->data.let_decl.name,
                                                 versioned_local)) {
                    free(rendered_type);
                    ok = false;
                    break;
                }
                if (value_type != NULL && value_type[0] != '\0')
                    register_typed_var(ctx, stmt->data.let_decl.name, value_type);
                free(rendered_type);
                continue;
            }
            if (transpiler_find_routine_exit_ssa_name(
                    mir_routine, stmt->data.let_decl.name) != NULL) {
                continue;
            }
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
transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                         size_t inst_index,
                                         const ASTNode *expr,
                                         TranspilerSSANameMap *ssa_map_out)
{
    if (block == NULL || expr == NULL || ssa_map_out == NULL)
        return true;

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *name = expr->data.identifier.name;
        const char *mapped_value;
        if (transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ssa_map_out, name) != NULL) {
            return true;
        }
        mapped_value = transpiler_find_prior_block_ssa_name(block, inst_index, name);
        if (mapped_value == NULL)
            mapped_value = transpiler_find_block_exit_ssa_name(block, name);
        if (mapped_value != NULL)
            return transpiler_ssa_name_map_set(ssa_map_out, name, mapped_value);
        return true;
    }

    switch (expr->type) {
        case AST_BINARY:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.binary.left, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.binary.right, ssa_map_out);
        case AST_UNARY:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.unary.operand, ssa_map_out);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER
                && !transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.call.callee, ssa_map_out)) {
                return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, expr->data.call.arguments[i], ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.member.object, ssa_map_out);
        case AST_ARRAY_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.array_access.array, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.array_access.index, ssa_map_out);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, expr->data.array_literal.elements[i], ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_ASSIGNMENT:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.assignment.target, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.assignment.value, ssa_map_out);
        case AST_AWAIT_EXPR:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.await_expr.expression, ssa_map_out);
        case AST_CHANNEL_SEND:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.channel_send.channel, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.channel_send.value, ssa_map_out);
        case AST_CHANNEL_RECV:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.channel_recv.channel, ssa_map_out);
        default:
            return true;
    }
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
        const char *existing_type = NULL;
        if (transpiler_resolve_ssa_name(ssa_map, expr->data.identifier.name) != NULL)
            return true;
        if (transpiler_name_is_token_local(name))
            return true;
        if (ctx != NULL) {
            existing_type = lookup_typed_var((TranspilerCtx *)ctx, name);
            if (is_slot_var((TranspilerCtx *)ctx, name))
                return true;
            if (existing_type != NULL
                && (transpiler_type_name_is_slot_like(existing_type)
                    || transpiler_type_name_is_claim_shape(existing_type)
                    || strncmp(existing_type, "Channel<", 8) == 0)) {
                return true;
            }
        }
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
            if (inst->kind == MIR_INST_STMT && inst->ast != NULL) {
                if (inst->ast->type == AST_LET_DECL
                    && inst->ast->data.let_decl.name != NULL) {
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(
                            block, inst->ast->data.let_decl.name);
                    if (versioned_name != NULL) {
                        if (!transpiler_ssa_name_map_set(
                                &ssa_map,
                                inst->ast->data.let_decl.name,
                                versioned_name)) {
                            return false;
                        }
                    }
                } else if (inst->ast->type == AST_LET_DESTRUCTURE
                           && inst->ast->data.let_destructure.names != NULL) {
                    for (size_t dn = 0;
                         dn < inst->ast->data.let_destructure.name_count;
                         dn++) {
                        const char *binding =
                            inst->ast->data.let_destructure.names[dn];
                        const char *versioned_name;
                        if (binding == NULL)
                            continue;
                        versioned_name =
                            transpiler_find_block_exit_ssa_name(block, binding);
                        if (versioned_name != NULL) {
                            if (!transpiler_ssa_name_map_set(&ssa_map,
                                                             binding,
                                                             versioned_name)) {
                                return false;
                            }
                        }
                    }
                } else if (inst->ast->type == AST_ASSIGNMENT
                           && inst->ast->data.assignment.target != NULL
                           && inst->ast->data.assignment.target->type == AST_IDENTIFIER
                           && inst->ast->data.assignment.target->data.identifier.name != NULL) {
                    const char *target_name =
                        inst->ast->data.assignment.target->data.identifier.name;
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(block, target_name);
                    if (versioned_name != NULL) {
                        if (!transpiler_ssa_name_map_set(&ssa_map,
                                                         target_name,
                                                         versioned_name)) {
                            return false;
                        }
                    }
                }
            }
            if (!transpiler_materialize_pending_inst_uses(NULL,
                                                          (TranspilerCtx *)ctx,
                                                          func_decl,
                                                          block,
                                                          inst,
                                                          &ssa_map,
                                                          0,
                                                          false,
                                                          reason,
                                                          reason_cap)) {
                return false;
            }
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
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad true successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_succ_false && block->succ_false >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad false successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_cleanup_succ && block->cleanup_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad cleanup successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_rollback_succ && block->rollback_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad rollback successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_invalidation_succ && block->invalidation_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad invalidation successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }

        for (size_t j = 0; j < block->instruction_count; j++) {
            MIRInstKind kind = block->instructions[j].kind;
            if (kind != MIR_INST_BRANCH && kind != MIR_INST_RETURN
                && kind != MIR_INST_RESOURCE_OP && kind != MIR_INST_CLEANUP_EDGE
                && kind != MIR_INST_PHI && kind != MIR_INST_DEF
                && kind != MIR_INST_STMT) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap, "MIR contract invalid for %s: unsupported instruction kind %d in block %llu",
                             routine_name, (int)kind, (unsigned long long) block->id);
                return false;
            }
            if ((kind == MIR_INST_BRANCH || kind == MIR_INST_RETURN)
                && require_cleanup_blocks && block->is_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: cleanup block %llu has terminal instruction",
                             routine_name, (unsigned long long) block->id);
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
                         "MIR contract invalid for %s: block %llu branch instruction misses condition AST",
                         routine_name, (unsigned long long) block->id);
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
                         "MIR contract invalid for %s: block %llu has non-trailing instructions after terminal",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
    }

    if (branch_count > 1 || return_count > 1 || (has_branch && has_return)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %llu has %llu branch(es), %llu return(s)",
                     routine_name,
                     (unsigned long long) block->id,
                     (unsigned long long) branch_count,
                     (unsigned long long) return_count);
        return false;
    }

    if (has_branch) {
        if (!block->has_succ_true || !block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu has branch without both true/false successors",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
            if (!require_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: block %llu has branch plus exceptional successor",
                             routine_name, (unsigned long long) block->id);
                return false;
            }
        }
        return true;
    }

    if (has_return) {
        if (block->has_succ_true || block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu return has explicit successors",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (!require_cleanup
            && (block->has_cleanup_succ || block->has_rollback_succ
                || block->has_invalidation_succ)) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu return has exceptional successor"
                         " without cleanup-capable routine",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        return true;
    }

    if (block->has_succ_false) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %llu has false successor without branch",
                     routine_name, (unsigned long long) block->id);
        return false;
    }

    if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
        if (!require_cleanup) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu has exceptional successor without cleanup-capable routine",
                         routine_name, (unsigned long long) block->id);
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
            snprintf(reason, reason_cap, "intent cannot lower to MIR: no matching MIR routine (found %llu routines)", (unsigned long long) (ctx != NULL && ctx->mir != NULL ? ctx->mir->routine_count : 0));
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
    const MIRInstruction *emit_inst = inst;
    MIRInstruction inst_copy;
    char *write_value_expr = NULL;
    const char *helper = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";

    if (out == NULL || inst == NULL)
        return false;

    if (inst->kind == MIR_INST_RESOURCE_OP) {
        if (!cleanup_hook
            && ctx != NULL
            && ctx->active_ssa_map != NULL
            && inst->name != NULL
            && strcmp(inst->name, "Write") == 0
            && inst->ast != NULL
            && inst->ast->type == AST_CALL) {
            ASTNode *callee = inst->ast->data.call.callee;
            ASTNode *value_node = NULL;
            if (callee != NULL
                && callee->type == AST_IDENTIFIER
                && callee->data.identifier.name != NULL
                && strcmp(callee->data.identifier.name, "Write") == 0
                && inst->ast->data.call.arg_count >= 2) {
                value_node = inst->ast->data.call.arguments[1];
            } else if (callee != NULL
                       && callee->type == AST_MEMBER_ACCESS
                       && callee->data.member.name != NULL
                       && strcmp(callee->data.member.name, "Write") == 0
                       && inst->ast->data.call.arg_count >= 1) {
                value_node = inst->ast->data.call.arguments[0];
            }
            if (value_node != NULL) {
                write_value_expr = emit_expression_with_ssa_map(
                    value_node,
                    ctx,
                    (TranspilerSSANameMap *)ctx->active_ssa_map);
                if (write_value_expr != NULL && write_value_expr[0] != '\0') {
                    inst_copy = *inst;
                    inst_copy.arg1 = write_value_expr;
                    emit_inst = &inst_copy;
                }
            }
        }
        bool claim_already_materialized_by_stmt = false;
        bool is_claim_op = inst->name != NULL && strcmp(inst->name, "Claim") == 0;
        if (!cleanup_hook
            && is_claim_op) {
            if (ctx != NULL && ctx->mir != NULL) {
                claim_already_materialized_by_stmt = false;
            } else {
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
        }
        bool needs_concrete_emit = (cleanup_hook && !is_claim_op)
            || (is_claim_op && !claim_already_materialized_by_stmt && !cleanup_hook);
        if (ctx != NULL && ctx->mir != NULL && !cleanup_hook) {
            bool slot_is_secure = inst->slot_anchor != NULL
                && lookup_slot_is_secure(ctx, inst->slot_anchor);
            if (is_claim_op
                || (inst->name != NULL
                    && (strcmp(inst->name, "Write") == 0
                        || strcmp(inst->name, "Release") == 0
                        || strcmp(inst->name, "Move") == 0)
                    && !slot_is_secure)) {
                needs_concrete_emit = true;
            }
        }
        if (needs_concrete_emit) {
            if (!transpiler_emit_mir_resource_op(ctx, out, indent, emit_inst, emit_inst->type_layout, NULL)) {
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
                    free(write_value_expr);
                    return false;
                }
            }
            if (!cleanup_hook) {
                free(write_value_expr);
                return true;
            }
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
    free(write_value_expr);
    return true;
}
