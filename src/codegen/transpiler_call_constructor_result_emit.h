static char *
emit_call_domain_constructor(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Tagged union variant constructors: Circle(42) ??Shape_Circle(42) */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        ASTNode *class_decl = find_class_decl(ctx, fn);
        if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
            const char *ctor_type = fn;
            size_t argc = call->data.call.arg_count;
            CodeBuf *fields = codebuf_create();
            if (class_has_generic_params(class_decl)) {
                ASTNode synthetic_type = {0};
                synthetic_type.type = AST_TYPE;
                synthetic_type.data.type.name = (char *)fn;
                synthetic_type.data.type.generic_args = NULL;
                {
                    const char *spec_name =
                        ensure_generic_class_specialization(ctx, class_decl, &synthetic_type);
                    if (spec_name != NULL)
                        ctor_type = spec_name;
                }
            }
            for (size_t i = 0; i < argc && i < class_decl->data.class_decl.field_count; i++) {
                ClassField *field = class_decl->data.class_decl.fields[i];
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".%s = %s",
                    field != NULL && field->name != NULL ? field->name : "field",
                    arg != NULL ? arg : "0");
                free(arg);
            }
            char *result;
            if (fields->len > 0)
                result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
            else
                result = strdup_fmt("(%s){0}", ctor_type);
            codebuf_destroy(fields);
            return result;
        }
        {
            ASTNode *party_decl = find_party_decl(ctx, fn);
            if (party_decl != NULL && party_decl->type == AST_PARTY_DECL) {
                size_t argc = call->data.call.arg_count;
                size_t shared_count = party_decl->data.party_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < shared_count; i++) {
                    ASTNode *shared = party_decl->data.party_decl.shared_fields[i];
                    const char *field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < shared_count; i++) {
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (i < argc)
                        continue;
                    shared = party_decl->data.party_decl.shared_fields[i];
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
            ASTNode *roster_decl = find_roster_decl(ctx, fn);
            if (roster_decl != NULL && roster_decl->type == AST_ROSTER_DECL) {
                size_t argc = call->data.call.arg_count;
                size_t exposed = roster_decl->data.roster_decl.party_count
                    + roster_decl->data.roster_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < roster_decl->data.roster_decl.party_count) {
                        ASTNode *slot = roster_decl->data.roster_decl.party_slots[i];
                        field_name = slot != NULL ? slot->data.roster_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = roster_decl->data.roster_decl.shared_fields[
                            i - roster_decl->data.roster_decl.party_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < roster_decl->data.roster_decl.shared_count; i++) {
                    size_t absolute_index = roster_decl->data.roster_decl.party_count + i;
                    ASTNode *shared;
                    const char *field_name;
                    char *init_expr;
                    if (absolute_index < argc)
                        continue;
                    shared = roster_decl->data.roster_decl.shared_fields[i];
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

    return NULL;
}

static char *
emit_call_result_option_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
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
            return transpiler_emit_none_with_context(ctx, call);
        }
        if (strcmp(fn, "IsSome") == 0 && call->data.call.arg_count == 1) {
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            if (opt_type == NULL || strncmp(opt_type, "Option<", 7) != 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: IsSome requires Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return pergyra_strdup("false");
            }
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = slot_inner_type_name(opt_type);
            char *result = strdup_fmt("IsSome_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsNone") == 0 && call->data.call.arg_count == 1) {
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            if (opt_type == NULL || strncmp(opt_type, "Option<", 7) != 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: IsNone requires Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return pergyra_strdup("false");
            }
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = slot_inner_type_name(opt_type);
            char *result = strdup_fmt("IsNone_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOption") == 0 && call->data.call.arg_count == 1) {
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            if (opt_type == NULL || strncmp(opt_type, "Option<", 7) != 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: UnwrapOption requires Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return pergyra_strdup("0");
            }
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = slot_inner_type_name(opt_type);
            char *result = strdup_fmt("UnwrapOption_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
    }

    return NULL;
}
