                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *relation_decl = find_relation_decl(ctx, fn);
            ASTNode *effect_decl = find_effect_decl(ctx, fn);
            ASTNode *overlay_decl = relation_decl != NULL ? relation_decl : effect_decl;
            if (overlay_decl != NULL) {
                size_t argc = call->data.call.arg_count;
                size_t slot_count = overlay_decl->type == AST_RELATION_DECL
                    ? overlay_decl->data.relation_decl.slot_count
                    : overlay_decl->data.effect_decl.slot_count;
                size_t shared_count = overlay_decl->type == AST_RELATION_DECL
                    ? overlay_decl->data.relation_decl.shared_count
                    : overlay_decl->data.effect_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < slot_count) {
                        ASTNode *slot = overlay_decl->type == AST_RELATION_DECL
                            ? overlay_decl->data.relation_decl.slots[i]
                            : overlay_decl->data.effect_decl.slots[i];
                        field_name = slot != NULL ? slot->data.domain_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = overlay_decl->type == AST_RELATION_DECL
                            ? overlay_decl->data.relation_decl.shared_fields[i - slot_count]
                            : overlay_decl->data.effect_decl.shared_fields[i - slot_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < shared_count; i++) {
                    size_t absolute_index = slot_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = overlay_decl->type == AST_RELATION_DECL
                        ? overlay_decl->data.relation_decl.shared_fields[i]
                        : overlay_decl->data.effect_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared->data.party_shared.name;
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                for (size_t i = 0; i < slot_count; i++) {
                    ASTNode *slot = overlay_decl->type == AST_RELATION_DECL
                        ? overlay_decl->data.relation_decl.slots[i]
                        : overlay_decl->data.effect_decl.slots[i];
                    const char *slot_name = slot != NULL
                        ? slot->data.domain_slot.slot_name
                        : NULL;
                    bool projection_slot = slot != NULL
                        && (slot->data.domain_slot.is_tobject
                            || domain_slot_is_projection_target_local(
                                slot,
                                overlay_decl->type == AST_RELATION_DECL
                                    ? overlay_decl->data.relation_decl.refreshes
                                    : overlay_decl->data.effect_decl.refreshes,
                                overlay_decl->type == AST_RELATION_DECL
                                    ? overlay_decl->data.relation_decl.refresh_count
                                    : overlay_decl->data.effect_decl.refresh_count));
                    if (!projection_slot || slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *zone_decl = find_zone_decl(ctx, fn);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL) {
                size_t argc = call->data.call.arg_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc
                        && i < zone_decl->data.zone_decl.slot_count
                               + zone_decl->data.zone_decl.shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < zone_decl->data.zone_decl.slot_count) {
                        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                        field_name = slot != NULL ? slot->data.domain_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = zone_decl->data.zone_decl.shared_fields[
                            i - zone_decl->data.zone_decl.slot_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < zone_decl->data.zone_decl.shared_count; i++) {
                    size_t absolute_index = zone_decl->data.zone_decl.slot_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = zone_decl->data.zone_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
                    ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                    const char *slot_name = slot != NULL
                        ? slot->data.domain_slot.slot_name
                        : NULL;
                    bool projection_slot = slot != NULL
                        && (slot->data.domain_slot.is_tobject
                            || domain_slot_is_projection_target_local(
                                slot,
                                zone_decl->data.zone_decl.refreshes,
                                zone_decl->data.zone_decl.refresh_count));
                    if (!projection_slot || slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *world_decl = find_world_decl(ctx, fn);
            if (world_decl != NULL && world_decl->type == AST_WORLD_DECL) {
                size_t argc = call->data.call.arg_count;
                CodeBuf *fields = codebuf_create();
                size_t exposed = world_decl->data.world_decl.roster_count
                    + world_decl->data.world_decl.zone_count
                    + world_decl->data.world_decl.shared_count;
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < world_decl->data.world_decl.roster_count) {
                        ASTNode *slot = world_decl->data.world_decl.rosters[i];
                        field_name = slot != NULL ? slot->data.world_roster.slot_name : "field";
                    } else if (i < world_decl->data.world_decl.roster_count
                                   + world_decl->data.world_decl.zone_count) {
                        ASTNode *slot = world_decl->data.world_decl.zones[
                            i - world_decl->data.world_decl.roster_count];
                        field_name = slot != NULL ? slot->data.world_zone.slot_name : "field";
                    } else {
                        ASTNode *shared = world_decl->data.world_decl.shared_fields[
                            i - world_decl->data.world_decl.roster_count
                              - world_decl->data.world_decl.zone_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < world_decl->data.world_decl.shared_count; i++) {
                    size_t absolute_index = world_decl->data.world_decl.roster_count
                        + world_decl->data.world_decl.zone_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = world_decl->data.world_decl.shared_fields[i];
                    if (shared == NULL || shared->data.party_shared.initializer == NULL)
                        continue;
                    field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    init_expr = emit_expression(shared->data.party_shared.initializer, ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
                    ASTNode *zone = world_decl->data.world_decl.zones[i];
                    const char *slot_name = zone != NULL
                        ? zone->data.world_zone.slot_name
                        : NULL;
                    if (slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__zone_dirty_%s = true", slot_name);
                }
                if (fields->len > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".__world_derived_dirty = true");
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }

        const char *qualified = lookup_enum_variant_qualified_name(ctx, fn);
        if (qualified != NULL) {
            /* Emit: EnumName_VariantName(args...) */
            size_t argc = call->data.call.arg_count;
            char **arg_strs = calloc(argc > 0 ? argc : 1, sizeof(char *));
            for (size_t i = 0; i < argc; i++)
                arg_strs[i] = emit_expression(call->data.call.arguments[i], ctx);
            /* Build argument list string */
            size_t buf_len = strlen(qualified) + 3;
            for (size_t i = 0; i < argc; i++) {
                if (arg_strs[i] == NULL) {
                    for (size_t j = 0; j < i; j++)
                        free(arg_strs[j]);
                    free(arg_strs);
                    return NULL;
                }
                buf_len += strlen(arg_strs[i]) + 2;
            }
            char *result = malloc(buf_len);
            if (result == NULL) {
                for (size_t i = 0; i < argc; i++)
                    free(arg_strs[i]);
                free(arg_strs);
                return NULL;
            }
            {
                size_t offset = 0;
                size_t qual_len = strlen(qualified);
                memcpy(result + offset, qualified, qual_len);
                offset += qual_len;
                result[offset++] = '(';
                for (size_t i = 0; i < argc; i++) {
                    if (i > 0) {
                        result[offset++] = ',';
                        result[offset++] = ' ';
                    }
                    {
                        size_t arg_len = strlen(arg_strs[i]);
                        memcpy(result + offset, arg_strs[i], arg_len);
                        offset += arg_len;
                    }
                    free(arg_strs[i]);
                }
                result[offset++] = ')';
                result[offset] = '\0';
            }
            free(arg_strs);
            return result;
        }
    }

    /* Result<T, E> built-in functions:
     * - Ok/Err require explicit Result context from the surrounding type.
     * - IsOk/IsErr/Unwrap/UnwrapOr may also derive suffix from their Result
     *   operand when the surrounding expression context is not specific. */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        bool is_result_ctor = false;
        bool is_result_consumer = false;
        char result_suffix[128] = {0};
        bool have_result_suffix = transpiler_result_suffix_from_context(
            ctx, result_suffix, sizeof(result_suffix));
        is_result_ctor = strcmp(fn, "Ok") == 0 || strcmp(fn, "Err") == 0;
        is_result_consumer = strcmp(fn, "IsOk") == 0
            || strcmp(fn, "IsErr") == 0
            || strcmp(fn, "Unwrap") == 0
            || strcmp(fn, "UnwrapOr") == 0;

        if (!have_result_suffix && is_result_consumer
            && call->data.call.arg_count >= 1
            && call->data.call.arguments[0] != NULL) {
            const char *arg_type = infer_expression_type_name(
                ctx, call->data.call.arguments[0]);
            have_result_suffix = transpiler_result_suffix_from_type_name(
                arg_type, result_suffix, sizeof(result_suffix));
        }

        if ((is_result_ctor || is_result_consumer) && !have_result_suffix) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot derive Result<T, E> specialization for %s(); add explicit Result<T, E> type context",
                fn);
            return pergyra_strdup("0");
        }

        if (strcmp(fn, "Ok") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Ok_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Err") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Err_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsOk") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("IsOk_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsErr") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("IsErr_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Unwrap") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Unwrap_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOr") == 0 && call->data.call.arg_count == 2) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *fallback = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("UnwrapOr_%s(%s, %s)", result_suffix, arg, fallback);
            free(arg);
            free(fallback);
            return result;
        }
        if (strcmp(fn, "Some") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char *result = strdup_fmt("Some_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "None") == 0 && call->data.call.arg_count == 0) {
            /* Use expected target type from enclosing let declaration if available */
            const char *inner = "Int";
            if (ctx->expected_type != NULL && strncmp(ctx->expected_type, "Option<", 7) == 0) {
                inner = slot_inner_type_name(ctx->expected_type);
            }
            return strdup_fmt("None_%s()", inner);
        }
        if (strcmp(fn, "IsSome") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("IsSome_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsNone") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("IsNone_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOption") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("UnwrapOption_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
    }

    /* Standard library built-in functions */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;

        /* Math functions */
        if (strcmp(fn, "Abs") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Min") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Max") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        /* String functions */
        if (strcmp(fn, "StringLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Contains") == 0 || strcmp(fn, "StringContains") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringContains(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Replace") == 0 || strcmp(fn, "StringReplace") == 0)
            && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *old_s = emit_expression(call->data.call.arguments[1], ctx);
            char *new_s = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
            free(s); free(old_s); free(new_s);
            return result;
        }
        if (strcmp(fn, "Substring") == 0 && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *start = emit_expression(call->data.call.arguments[1], ctx);
            char *len = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
            free(s); free(start); free(len);
            return result;
        }
        if ((strcmp(fn, "Trim") == 0 || strcmp(fn, "StringTrim") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("StringTrim(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Upper") == 0 || strcmp(fn, "ToUpper") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToUpper(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Lower") == 0 || strcmp(fn, "ToLower") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToLower(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Concat") == 0 || strcmp(fn, "StringConcat") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        /* StringSplit / StringJoin / ToInt / ToFloat / Math */
        if ((strcmp(fn, "StringSplit") == 0 || strcmp(fn, "Split") == 0)
            && call->data.call.arg_count == 2) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
            free(s); free(d);
            return result;
        }
        if ((strcmp(fn, "StringJoin") == 0 || strcmp(fn, "Join") == 0)
            && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *sep = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
            free(arr); free(sep);
            return result;
        }
        if (strcmp(fn, "ToInt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToInt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ToFloat") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToFloat(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Sqrt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Sqrt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Pow") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Pow(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Floor") == 0 || strcmp(fn, "Ceil") == 0
            || strcmp(fn, "Round") == 0
            || strcmp(fn, "Sin") == 0 || strcmp(fn, "Cos") == 0
            || strcmp(fn, "Tan") == 0 || strcmp(fn, "Asin") == 0
            || strcmp(fn, "Acos") == 0 || strcmp(fn, "Atan") == 0
            || strcmp(fn, "Exp") == 0 || strcmp(fn, "MathLog") == 0
            || strcmp(fn, "Log10") == 0 || strcmp(fn, "Log2") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("%s(%s)", fn, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Atan2") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Atan2(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Clamp") == 0 && call->data.call.arg_count == 3) {
            char *val = emit_expression(call->data.call.arguments[0], ctx);
            char *lo = emit_expression(call->data.call.arguments[1], ctx);
            char *hi = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Clamp(%s, %s, %s)", val, lo, hi);
            free(val); free(lo); free(hi);
            return result;
        }
        if (strcmp(fn, "PI") == 0) return pergyra_strdup("PGY_PI");
        if (strcmp(fn, "E") == 0 && call->data.call.arg_count == 0)
            return pergyra_strdup("PGY_E");
        if (strcmp(fn, "Random") == 0) {
            if (call->data.call.arg_count >= 1) {
                char *arg = emit_expression(call->data.call.arguments[0], ctx);
                char *result = strdup_fmt("Random(%s)", arg);
                free(arg);
                return result;
            }
            return pergyra_strdup("Random(100)");
        }
        if (strcmp(fn, "SeedRandom") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("SeedRandom(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)(%s.length))", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayPush") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *suffix = infer_expression_type_name(
                ctx, call->data.call.arguments[1]);
            char *result = strdup_fmt(
                "pgy_array_push_%s(&%s, %s)", suffix, arr, val);
            free(arr); free(val);
            return result;
        }
        if (strcmp(fn, "ArraySet") == 0 && call->data.call.arg_count == 3) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *idx = emit_expression(call->data.call.arguments[1], ctx);
            char *val = emit_expression(call->data.call.arguments[2], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            char *result = strdup_fmt(
                "pgy_array_set_%s(&%s, %s, %s)", inner, arr, idx, val);
            free(arr); free(idx); free(val);
            return result;
        }
        if (strcmp(fn, "ArrayPop") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s).length > 0 ? (%s).length-- : 0)",
                arr, arr);
            free(arr);
            return result;
        }
        /* ArraySort ??hybrid sort using AlphaDev kernels for small arrays */
        if (strcmp(fn, "ArraySort") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            char *result = strdup_fmt(
                "({ pgy_array_sort_%s((%s).data, (%s).length); %s; })",
                inner, arr, arr, arr);
            free(arr);
            return result;
        }
        /* ArrayMap ??apply function to each element, return new array */
        if (strcmp(fn, "ArrayMap") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_map_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _mi = 0; _mi < (%s).length; _mi++) "
                "pgy_array_push_%s(&_pgy_map_%d, %s((%s).data[_mi])); "
                "_pgy_map_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                inner, tmp_id, fn_arg, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        /* ArrayFilter ??keep elements where predicate returns true */
        if (strcmp(fn, "ArrayFilter") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_filt_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _fi = 0; _fi < (%s).length; _fi++) "
                "if (%s((%s).data[_fi])) "
                "pgy_array_push_%s(&_pgy_filt_%d, (%s).data[_fi]); "
                "_pgy_filt_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                fn_arg, arr,
                inner, tmp_id, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        /* ArrayReverse ??in-place reverse */
        if (strcmp(fn, "ArrayReverse") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = NULL;
            const char *c_type = NULL;
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0) {
                inner = slot_inner_type_name(arr_type);
                c_type = pergyra_type_to_c(inner);
            }
            if (inner == NULL || c_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine element type for ArrayReverse; explicit concrete Array<T> input is required");
                free(arr);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "({ for (size_t _ri = 0; _ri < (%s).length / 2; _ri++) { "
                "%s _tmp = (%s).data[_ri]; "
                "(%s).data[_ri] = (%s).data[(%s).length - 1 - _ri]; "
                "(%s).data[(%s).length - 1 - _ri] = _tmp; } %s; })",
                arr, c_type, arr, arr, arr, arr, arr, arr, arr);
            free(arr);
            return result;
        }
        /* Channel builtins */
        if (strcmp(fn, "TryRecv") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_try_recv_%s(&%s, &_pgy_recv_tmp) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, inner, inner);
            free(ch);
            return result;
        }
        if (strcmp(fn, "RecvTimeout") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *timeout = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_recv_timeout_%s(&%s, &_pgy_recv_tmp, (uint64_t)(%s)) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, timeout, inner, inner);
            free(ch);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "TrySend") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "TrySendStatus") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_status_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "SendTimeout") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "SendTimeoutStatus") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_status_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "Cancel") == 0 && call->data.call.arg_count == 1) {
            char *task = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_task_cancel(%s)", task);
            free(task);
            return result;
        }
        if (strcmp(fn, "IsCancelled") == 0 && call->data.call.arg_count == 0) {
            return strdup_fmt("pgy_task_is_cancelled()");
        }
        if (strcmp(fn, "ChannelClose") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_close_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelReady") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_ready_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelLength") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_length_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelCapacity") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_capacity_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelSpace") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_space_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelFull") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_full_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelClosed") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_closed_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        /* Clone: explicit copy of Slot */
        if (strcmp(fn, "Clone") == 0 && call->data.call.arg_count == 1) {
            char *src = emit_expression(call->data.call.arguments[0], ctx);
            const char *tn = infer_expression_type_name(
                ctx, call->data.call.arguments[0]);
            if (tn != NULL && strncmp(tn, "Slot<", 5) == 0) {
                const char *inner = slot_inner_type_name(tn);
                char *result = strdup_fmt(
                    "({ PgySlot_%s _c = pgy_claim_%s(); "
                    "pgy_write_%s(&_c, pgy_read_%s(&%s)); _c; })",
                    inner, inner, inner, inner, src);
                free(src);
                return result;
            }
            return src;
        }
        /* Print (no newline) vs Log (with newline) */
        if (strcmp(fn, "Print") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("printf(\"%%s\", %s)", arg);
            free(arg);
            return result;
        }
        /* ToString */
        if (strcmp(fn, "ToString") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *arg_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = NULL;
            if (arg_type != NULL && strcmp(arg_type, "String") == 0) {
                result = pergyra_strdup(arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Bool") == 0) {
                result = strdup_fmt("pgy_bool_to_string(%s)", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Float") == 0) {
                result = strdup_fmt("pgy_float_to_string(%s)", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Double") == 0) {
                result = strdup_fmt("pgy_float_to_string((float)(%s))", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Long") == 0) {
                result = strdup_fmt("pgy_int_to_string((int32_t)(%s))", arg);
            } else {
                result = strdup_fmt("pgy_int_to_string(%s)", arg);
            }
            free(arg);
            return result;
