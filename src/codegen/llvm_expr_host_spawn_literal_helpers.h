static LLVMValueRef
llvm_current_self_base_ptr(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL || cls == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    if (self_var->type == LLVMPointerType(cls->struct_type, 0))
        return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
            llvm_tmp_name(ctx));
    return self_var->alloca;
}

static LLVMValueRef
llvm_identifier_base_ptr(LLVMGenCtx *ctx, const char *name, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *var;

    if (ctx == NULL || name == NULL || cls == NULL)
        return NULL;

    var = llvm_scope_lookup(ctx, name);
    if (var != NULL) {
        LLVMValueRef base_ptr = var->alloca;
        if (var->type == LLVMPointerType(cls->struct_type, 0)) {
            base_ptr = LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                llvm_tmp_name(ctx));
        }
        return base_ptr;
    }

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *parent_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        int parent_field_idx;
        LLVMValueRef self_ptr;
        if (parent_cls == NULL)
            return NULL;
        parent_field_idx = llvm_class_field_index(parent_cls, name);
        if (parent_field_idx < 0)
            return NULL;
        self_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
        if (self_ptr == NULL)
            return NULL;
        return LLVMBuildStructGEP2(ctx->builder, parent_cls->struct_type, self_ptr,
            (unsigned)parent_field_idx, llvm_tmp_name(ctx));
    }

    return NULL;
}

static LLVMValueRef
llvm_emit_projection_from_binding(LLVMGenCtx *ctx,
                                  const char *target_class_name,
                                  const char *source_name)
{
    const char *source_class_name;
    LLVMClassTypeEntry *target_cls;
    LLVMClassTypeEntry *source_cls;
    ASTNode *source_decl;
    LLVMVarEntry *source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (ctx == NULL || target_class_name == NULL || source_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    target_cls = llvm_lookup_class(ctx, target_class_name);
    source_var = llvm_scope_lookup(ctx, source_name);
    source_class_name = llvm_lookup_var_class(ctx, source_name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    source_decl = source_class_name != NULL
        ? llvm_find_projection_nominal_decl(ctx, source_class_name) : NULL;
    if (target_cls == NULL || source_var == NULL || source_cls == NULL || source_decl == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    source_base = source_var->alloca;
    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
            source_var->alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMGetUndef(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *field = &target_cls->fields[i];
        LLVMValueRef field_val = llvm_load_projection_path_value(ctx, source_decl,
            source_cls, source_base, field->field_name);
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_val,
            (unsigned)field->index, llvm_tmp_name(ctx));
    }
    return projected;
}

static ASTNode *
llvm_current_host_method_decl(LLVMGenCtx *ctx, const char *method_name)
{
    const char *host_name;

    host_name = llvm_current_host_class_name(ctx);

    if (ctx == NULL || method_name == NULL || host_name == NULL)
        return NULL;
    return llvm_find_nominal_host_method_decl(ctx, host_name,
                                              method_name);
}

static LLVMValueRef
llvm_current_self_call_arg(LLVMGenCtx *ctx)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
        llvm_tmp_name(ctx));
}

#include "llvm_expr_spawn_call_helpers.h"

static const char *
llvm_operator_overload_suffix(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "sub";
    case TOKEN_STAR: return "mul";
    case TOKEN_SLASH: return "div";
    case TOKEN_PERCENT: return "mod";
    case TOKEN_EQUAL: return "eq";
    case TOKEN_NOT_EQUAL: return "ne";
    case TOKEN_LESS: return "lt";
    case TOKEN_LESS_EQUAL: return "le";
    case TOKEN_GREATER: return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default: return NULL;
    }
}

static const char *
llvm_expr_custom_type_name(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_IDENTIFIER: {
        const char *name = node->data.identifier.name;
        const char *class_name = llvm_lookup_var_class(ctx, name);
        if (class_name != NULL)
            return class_name;
        if (strcmp(name, "self") != 0) {
            const char *host_name = llvm_current_host_class_name(ctx);
            LLVMClassTypeEntry *host_cls = host_name != NULL
                ? llvm_lookup_class(ctx, host_name) : NULL;
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty = host_cls->fields[field_idx].field_type;
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == field_ty)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        {
            LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
            if (variant != NULL)
                return variant->enum_name;
        }
        return NULL;
    }
    case AST_MEMBER_ACCESS:
        if (llvm_is_upper_ident(node->data.member.object)) {
            LLVMEnumVariantEntry *variant =
                llvm_lookup_enum_variant_qualified(ctx,
                    node->data.member.object->data.identifier.name,
                    node->data.member.name);
            if (variant != NULL)
                return variant->enum_name;
        }
        {
            const char *base_name =
                llvm_expr_custom_type_name(node->data.member.object, ctx);
            LLVMClassTypeEntry *base_cls = NULL;
            if (base_name != NULL)
                base_cls = llvm_lookup_class(ctx, base_name);
            if (base_cls != NULL) {
                int field_idx = llvm_class_field_index(base_cls,
                    node->data.member.name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty = base_cls->fields[field_idx].field_type;
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == field_ty)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        return NULL;
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee = node->data.call.callee->data.identifier.name;
            if (llvm_lookup_class(ctx, callee) != NULL)
                return callee;
            {
                LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee);
                if (variant != NULL)
                    return variant->enum_name;
            }
            {
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, callee);
                ASTNode *host_decl = llvm_current_host_decl(ctx);
                const char *host_name = llvm_decl_node_name(host_decl);
                if (fn == NULL && host_name != NULL) {
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        host_name, callee);
                    fn = llvm_lookup_function(ctx, full_name);
                }
                if (fn != NULL) {
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == fn->ret_type)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        return NULL;
    default:
        return NULL;
    }
}

static LLVMClassTypeEntry *
llvm_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef ty)
{
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (ctx->class_types[i].struct_type == ty)
            return &ctx->class_types[i];
    }
    return NULL;
}

ASTNode *
llvm_find_enum_decl(LLVMGenCtx *ctx, const char *enum_name)
{
    ASTNode **types = NULL;
    size_t type_count = 0;
    ASTNode *decl;

    if (ctx == NULL || enum_name == NULL)
        return NULL;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL, enum_name);
    if (decl != NULL)
        return decl;
    llvm_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && stmt->data.enum_decl.name != NULL
            && strcmp(stmt->data.enum_decl.name, enum_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static LLVMValueRef
llvm_emit_number(ASTNode *node, LLVMGenCtx *ctx)
{
    double val = node->data.number.value;

    /* Explicit 'L' suffix -> always i64 */
    if (node->data.number.is_long) {
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);
    }

    /* Check if integer fits in i32 */
    if (val == (int64_t)val && val >= -2147483648.0 && val <= 2147483647.0)
        return LLVMConstInt(ctx->type_i32, (unsigned long long)(int32_t)val, 1);

    /* Check if integer fits in i64 (beyond i32 range) */
    if (val == (double)(int64_t)val
        && val >= -9.2233720368547758e+18
        && val <=  9.2233720368547758e+18)
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);

    return LLVMConstReal(ctx->type_f64, val);
}

static LLVMValueRef
llvm_emit_string(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *str = node->data.string.value;
    LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str,
                                                    llvm_tmp_name(ctx));
    return global;
}

static size_t
llvm_count_banner_line_indent(const char *line_start, const char *line_end)
{
    size_t indent = 0;

    while (line_start < line_end) {
        if (*line_start == ' ' || *line_start == '\t') {
            indent++;
            line_start++;
            continue;
        }
        break;
    }
    return indent;
}

static bool
llvm_line_is_empty_with_only_ws(const char *line_start, const char *line_end)
{
    for (const char *p = line_start; p < line_end; p++) {
        if (*p != ' ' && *p != '\t')
            return false;
    }
    return true;
}

static char *llvm_normalize_banner_string_literal_scratch(const char *src,
                                                          PgyArena *arena);
