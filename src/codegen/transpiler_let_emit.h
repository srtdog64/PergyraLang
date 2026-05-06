#ifndef PGY_TRANSPILER_LET_EMIT_H
#define PGY_TRANSPILER_LET_EMIT_H

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
    if (transpiler_try_emit_let_slot_claim(node, ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }
    if (transpiler_try_emit_let_slot_view_or_move(ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }
    if (transpiler_try_emit_let_slot_sugar(ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }
    if (transpiler_try_emit_box_family_let(ctx, name, init, ann,
            &ann_type_name)) {
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
                : (key_param != NULL && key_param->name != NULL
                    ? pergyra_strdup(key_param->name) : NULL);
            char *value = (value_param != NULL && value_param->constraint != NULL)
                ? render_type_name(value_param->constraint)
                : (value_param != NULL && value_param->name != NULL
                    ? pergyra_strdup(value_param->name) : NULL);
            if (key == NULL || key[0] == '\0'
                || value == NULL || value[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: HashMap binding '%s' requires explicit concrete HashMap<K, V> annotation",
                    name != NULL ? name : "<binding>");
                free(key);
                free(value);
                free(ann_type_name);
                return;
            }
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

    /* Array literal: let arr = [1, 2, 3] ??PgyArray_Int arr = ({ ... }); */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        const char *array_type_name = ann_type_name != NULL
            ? ann_type_name
            : infer_expression_type_name(ctx, init);
        const char *array_c_type = pergyra_type_to_c(array_type_name);
        const char *saved_expected_type = ctx->expected_type;
        char *init_expr;
        if (array_type_name == NULL
            || strcmp(array_type_name, "Array<Unknown>") == 0
            || array_c_type == NULL
            || strcmp(array_c_type, "void*") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: empty array literal binding '%s' requires an explicit Array<T> annotation",
                name != NULL ? name : "<binding>");
            free(ann_type_name);
            return;
        }
        ctx->expected_type = array_type_name;
        init_expr = emit_expression(init, ctx);
        ctx->expected_type = saved_expected_type;
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

    if (init != NULL
        && init->type == AST_UNARY
        && init->data.unary.op.type == TOKEN_QUESTION) {
        ASTNode *operand = init->data.unary.operand;
        const char *result_type = infer_expression_type_name(ctx, operand);
        const char *result_c_type;
        char *operand_expr;
        int try_id;

        if (result_type == NULL || strncmp(result_type, "Result<", 7) != 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C try lowering for let binding '%s' requires Result<T,E> operand type",
                name != NULL ? name : "<binding>");
            free(ann_type_name);
            return;
        }
        if (ctx->current_return_type[0] == '\0'
            || strncmp(ctx->current_return_type, "Result<", 7) != 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C try lowering for let binding '%s' requires an enclosing Result-returning function",
                name != NULL ? name : "<binding>");
            free(ann_type_name);
            return;
        }

        result_c_type = pergyra_type_to_c(result_type);
        operand_expr = emit_expression(operand, ctx);
        try_id = ctx->tmp_counter++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s __try_%d = %s;\n",
                      result_c_type, try_id, operand_expr);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (__try_%d.tag != PgyResultOk) return __try_%d;\n",
            try_id, try_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = __try_%d.ok;\n",
                      c_type, name, try_id);
        free(operand_expr);
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
        } else {
            const char *inferred = infer_expression_type_name(ctx, init);
            if (inferred != NULL)
                register_typed_var(ctx, name, inferred);
        }
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
        /* For generic classes, find_class_decl won't find "Node_Int" ??         * fall back to the original generic class declaration. */
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
    } else if (transpiler_c_type_uses_scalar_zero(c_type)) {
        /* Scalar / pointer default. */
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    } else {
        /* Aggregate default.  Plain `= 0` is not a valid struct initializer
         * in C99, so emit a compound literal zero.  Semantic currently
         * rejects uninitialized locals before reaching this path, but this
         * keeps the fallback well-formed as defense in depth. */
        codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, name, c_type);
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

#endif /* PGY_TRANSPILER_LET_EMIT_H */
