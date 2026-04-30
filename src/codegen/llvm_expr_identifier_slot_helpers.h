static char *
llvm_normalize_banner_string_literal_scratch(const char *src, PgyArena *arena)
{
    const char *cursor = src;
    const char *end;
    size_t min_indent = 0;
    bool found_content_line = false;
    char *output = NULL;
    size_t output_cap = 0;
    size_t output_len = 0;

    if (src == NULL)
        return NULL;

    if (cursor[0] == '\n')
        cursor++;
    else if (cursor[0] == '\r') {
        cursor += (cursor[1] == '\n') ? 2 : 1;
    }

    end = cursor + strlen(cursor);
    while (end > cursor && (end[-1] == '\n' || end[-1] == '\r'))
        end--;

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        if (!llvm_line_is_empty_with_only_ws(line, line_end)) {
            size_t indent = llvm_count_banner_line_indent(line, line_end);
            if (!found_content_line || indent < min_indent) {
                min_indent = indent;
                found_content_line = true;
            }
        }

        if (line_end == end)
            break;
        if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
            line = line_end + 2;
        else
            line = line_end + 1;
    }

    if (!found_content_line)
        min_indent = 0;

    output_cap = (size_t)(end - cursor + 1);
    if (output_cap == 0)
        output_cap = 1;
    output = pgy_arena_alloc(arena, output_cap);
    if (output == NULL)
        return NULL;

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        const char *body_start = line;
        if (llvm_line_is_empty_with_only_ws(line, line_end))
            body_start = line_end;
        else {
            for (size_t i = 0; i < min_indent && body_start < line_end; i++) {
                if (*body_start == ' ' || *body_start == '\t')
                    body_start++;
                else
                    break;
            }
        }

        if (line_end > body_start) {
            size_t seg_len = (size_t)(line_end - body_start);
            if (output_len + seg_len + 2 > output_cap) {
                size_t new_cap = (output_cap * 2 > output_len + seg_len + 2)
                                 ? output_cap * 2
                                 : output_len + seg_len + 2;
                char *grown = pgy_arena_alloc(arena, new_cap);
                if (grown == NULL)
                    break;
                memcpy(grown, output, output_len);
                output = grown;
                output_cap = new_cap;
            }
            memcpy(output + output_len, body_start, seg_len);
            output_len += seg_len;
        }

        if (line_end < end) {
            if (output_len + 2 > output_cap) {
                size_t new_cap = output_cap * 2;
                char *grown = pgy_arena_alloc(arena, new_cap);
                if (grown == NULL)
                    break;
                memcpy(grown, output, output_len);
                output = grown;
                output_cap = new_cap;
            }
            output[output_len++] = '\n';
            if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
                line = line_end + 2;
            else
                line = line_end + 1;
        } else {
            break;
        }
    }

    if (output_len >= output_cap) {
        char *grown = pgy_arena_alloc(arena, output_len + 1);
        if (grown != NULL) {
            memcpy(grown, output, output_len);
            output = grown;
            output_cap = output_len + 1;
        }
    }
    if (output == NULL)
        return NULL;

    output[output_len] = '\0';
    return output;
}

static LLVMValueRef
llvm_emit_boolean(ASTNode *node, LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, node->data.boolean.value ? 1 : 0, 0);
}

static LLVMValueRef
llvm_direct_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                      const char *inner)
{
    LLVMTypeRef inner_ty;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;
    LLVMValueRef value_ptr;

    if (slot_var == NULL || inner == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        slot_ty = llvm_slot_struct_type(ctx, inner);
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 0, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, inner_ty, value_ptr, llvm_tmp_name(ctx));
}

static void
llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                       LLVMValueRef value);

static void
llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var);

static void
llvm_direct_secure_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                              LLVMValueRef value)
{
    llvm_direct_slot_write(ctx, slot_var, value);
}

static LLVMValueRef
llvm_direct_secure_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                             const char *inner)
{
    LLVMTypeRef inner_ty;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;
    LLVMValueRef value_ptr;

    if (slot_var == NULL || inner == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);
    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        slot_ty = llvm_secure_slot_struct_type(ctx, inner);
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 0, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, inner_ty, value_ptr, llvm_tmp_name(ctx));
}

static void
llvm_direct_secure_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef token_ptr;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;

    llvm_direct_slot_release(ctx, slot_var);
    if (slot_var == NULL)
        return;

    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        const char *inner = llvm_lookup_slot_inner(ctx, slot_var->name);
        if (inner == NULL || inner[0] == '\0') {
            if (!ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, NULL,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM secure slot release for '%s' requires concrete SecureSlot<T> metadata",
                    slot_var->name != NULL ? slot_var->name : "<slot>");
            }
            return;
        }
        slot_ty = llvm_secure_slot_struct_type(ctx, inner);
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 2, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
}

static LLVMTypeRef
llvm_required_slot_struct_type_for_var(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                                       bool secure)
{
    const char *inner;
    if (ctx == NULL || slot_var == NULL)
        return NULL;
    inner = llvm_lookup_slot_inner(ctx, slot_var->name);
    if (inner == NULL || inner[0] == '\0') {
        if (!ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM slot helper for '%s' requires concrete Slot<T> metadata",
                slot_var->name != NULL ? slot_var->name : "<slot>");
        }
        return NULL;
    }
    return secure ? llvm_secure_slot_struct_type(ctx, inner)
                  : llvm_slot_struct_type(ctx, inner);
}

static void
llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                       LLVMValueRef value)
{
    LLVMValueRef value_ptr;
    LLVMValueRef occupied_ptr;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;

    if (slot_var == NULL || value == NULL)
        return;

    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        bool secure = llvm_lookup_slot_is_secure(ctx, slot_var->name);
        slot_ty = llvm_required_slot_struct_type_for_var(ctx, slot_var, secure);
        if (slot_ty == NULL)
            return;
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 0, llvm_tmp_name(ctx));
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, value, value_ptr);
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        occupied_ptr);
}

static void
llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef occupied_ptr;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;

    if (slot_var == NULL)

        return;

    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        bool secure = llvm_lookup_slot_is_secure(ctx, slot_var->name);
        slot_ty = llvm_required_slot_struct_type_for_var(ctx, slot_var, secure);
        if (slot_ty == NULL)
            return;
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
        occupied_ptr);
}

static const char *
llvm_derive_slot_inner_from_current_decl(LLVMGenCtx *ctx,
                                         const char *source_name,
                                         bool *secure_out)
{
    const char *current_name;
    ASTNode *current_decl;

    if (secure_out != NULL)
        *secure_out = false;
    if (ctx == NULL || source_name == NULL || ctx->current_function == NULL)
        return NULL;

    current_name = LLVMGetValueName(ctx->current_function);
    current_decl = current_name != NULL
        ? llvm_find_function_decl(ctx, current_name) : NULL;
    if (current_decl == NULL || current_decl->type != AST_FUNC_DECL)
        return NULL;

    for (size_t i = 0; i < current_decl->data.func_decl.param_count; i++) {
        FuncParam *p = current_decl->data.func_decl.params[i];
        const char *type_name;
        GenericParams *generic_args;
        const char *inner_name;

        if (p == NULL || p->name == NULL || strcmp(p->name, source_name) != 0
            || p->type == NULL || p->type->type != AST_TYPE
            || p->type->data.type.name == NULL) {
            continue;
        }

        type_name = p->type->data.type.name;
        if (strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0)
            continue;

        generic_args = p->type->data.type.generic_args;
        if (generic_args == NULL || generic_args->count == 0
            || generic_args->params == NULL || generic_args->params[0] == NULL) {
            continue;
        }

        inner_name = generic_args->params[0]->name;
        if (inner_name == NULL && generic_args->params[0]->constraint != NULL
            && generic_args->params[0]->constraint->type == AST_TYPE) {
            inner_name = generic_args->params[0]->constraint->data.type.name;
        }
        if (inner_name == NULL)
            continue;

        if (secure_out != NULL)
            *secure_out = (strcmp(type_name, "SecureSlot") == 0);
        return inner_name;
    }

    return NULL;
}

static LLVMVarEntry *
llvm_resolve_slot_target(LLVMGenCtx *ctx, ASTNode *slot_arg,
                         const char **inner_out,
                         const char **source_name_out,
                         bool *secure_out)
{
    const char *inner = NULL;
    const char *source_name = NULL;
    bool is_secure = false;

    if (slot_arg == NULL)
        return NULL;

    if (slot_arg->type == AST_IDENTIFIER) {
        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, slot_arg->data.identifier.name);
        if (view != NULL) {
            source_name = view->source_slot;
            inner = view->inner_type;
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        } else {
            source_name = slot_arg->data.identifier.name;
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    } else if (slot_arg->type == AST_CALL
               && slot_arg->data.call.callee != NULL
               && slot_arg->data.call.callee->type == AST_IDENTIFIER
               && slot_arg->data.call.arg_count >= 1
               && slot_arg->data.call.arguments[0] != NULL
               && slot_arg->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee = slot_arg->data.call.callee->data.identifier.name;
        if (callee != NULL
            && (strcmp(callee, "ViewRead") == 0
                || strcmp(callee, "ViewWrite") == 0
                || strcmp(callee, "Move") == 0)) {
            source_name = slot_arg->data.call.arguments[0]->data.identifier.name;
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    }

    if (inner == NULL && source_name != NULL) {
        const char *derived_inner =
            llvm_derive_slot_inner_from_current_decl(ctx, source_name, &is_secure);
        if (derived_inner != NULL)
            inner = derived_inner;
    }

    if (source_name == NULL) {
        llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_BINDING_MISSING, PGY_FIX_USE_SLOT_BOUND_IDENTIFIER, "LLVM slot operation requires an identifier/view/move source with a known slot binding");
        return NULL;
    }
    if (inner == NULL) {
        llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM slot operation on '%s' requires a concrete slot inner type; silent Int fallback is no longer allowed",
            source_name);
        return NULL;
    }
    if (inner_out != NULL)
        *inner_out = inner;
    if (source_name_out != NULL)
        *source_name_out = source_name;
    if (secure_out != NULL)
        *secure_out = is_secure;
    return source_name != NULL ? llvm_scope_lookup(ctx, source_name) : NULL;
}

static LLVMValueRef
llvm_slot_runtime_arg(LLVMGenCtx *ctx, LLVMVarEntry *var)
{
    if (ctx == NULL || var == NULL)
        return NULL;
    if (var->type != NULL && LLVMGetTypeKind(var->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                              llvm_tmp_name(ctx));
    }
    return var->alloca;
}

static LLVMValueRef
llvm_emit_identifier(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.identifier.name;
    LLVMProjectionBorrowEntry *projection_borrow;

    /* Slot sugar: auto-Read — call pgy_read_T(&slot) instead of loading struct */
    if (!ctx->suppress_slot_auto_read) {
        const char *inner = llvm_lookup_slot_inner(ctx, name);
        if (inner != NULL) {
            LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
            if (var != NULL) {
                bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
                char fn_name[64];
                snprintf(fn_name, sizeof(fn_name),
                    is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                if (fn != NULL) {
                    if (is_secure) {
                        LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, name);
                        if (token_var != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, var),
                                token_var->alloca
                            };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                                 args, 2, llvm_tmp_name(ctx));
                        }
                    }
                    LLVMValueRef args[] = { llvm_slot_runtime_arg(ctx, var) };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                         args, 1, llvm_tmp_name(ctx));
                }
                if (is_secure)
                    return llvm_direct_secure_slot_read(ctx, var, inner);
                return llvm_direct_slot_read(ctx, var, inner);
            }
        }
    }

    /* Look up in scope */
    projection_borrow = llvm_lookup_projection_borrow(ctx, name);
    if (projection_borrow != NULL) {
        return llvm_emit_projection_from_binding(ctx,
            projection_borrow->class_name,
            projection_borrow->source_name);
    }

    LLVMVarEntry *entry = llvm_scope_lookup(ctx, name);
    if (entry != NULL)
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));

    if (llvm_current_host_class_name(ctx) != NULL && strcmp(name, "self") != 0) {
        LLVMClassTypeEntry *cls =
            llvm_lookup_class(ctx, llvm_current_host_class_name(ctx));
        if (cls != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, cls);
                if (base_ptr == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, base_ptr, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }
        }
    }

    /* Look up as function (for passing as value) */
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, name);
    if (fn != NULL)
        return fn->fn;

    /* Bare enum variant identifier */
    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0);
    }

    /* Unknown — default to 0 */
    return LLVMConstInt(ctx->type_i32, 0, 0);
}
