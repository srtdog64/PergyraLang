/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend generic/temp helper ownership.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

const char *
llvm_tmp_name(LLVMGenCtx *ctx)
{
    static char bufs[8][32];
    static unsigned slot = 0;
    char *buf;

    if (ctx == NULL)
        return "t";
    buf = bufs[slot++ % (sizeof(bufs) / sizeof(bufs[0]))];
    snprintf(buf, 32, "t%d", ctx->tmp_counter++);
    return buf;
}

ASTNode *
llvm_lookup_generic_template(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->generic_template_count; i++) {
        if (strcmp(ctx->generic_templates[i].name, name) == 0)
            return ctx->generic_templates[i].ast;
    }
    return NULL;
}

bool
llvm_register_generic_template_decl(LLVMGenCtx *ctx, ASTNode *func_decl)
{
    if (ctx == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    const char *name = ast_declaration_name(func_decl);
    if (name == NULL)
        return false;
    if (llvm_lookup_generic_template(ctx, name) != NULL)
        return true;

    if (ctx->generic_template_count >= ctx->generic_template_capacity) {
        int new_capacity = ctx->generic_template_capacity == 0
            ? 16
            : ctx->generic_template_capacity * 2;
        LLVMGenericTemplate *new_templates =
            realloc(ctx->generic_templates,
                    (size_t)new_capacity * sizeof(LLVMGenericTemplate));
        if (new_templates == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_OOM,
                PGY_CAUSE_LLVM_MEMORY_EXHAUSTED,
                PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT,
                "out of memory growing generic_templates");
            return false;
        }
        memset(new_templates + ctx->generic_template_capacity, 0,
               (size_t)(new_capacity - ctx->generic_template_capacity)
                   * sizeof(LLVMGenericTemplate));
        ctx->generic_templates = new_templates;
        ctx->generic_template_capacity = new_capacity;
    }

    ctx->generic_templates[ctx->generic_template_count].name = name;
    ctx->generic_templates[ctx->generic_template_count].ast = func_decl;
    ctx->generic_template_count++;
    return true;
}

bool
llvm_mono_already_emitted(LLVMGenCtx *ctx, const char *mangled)
{
    for (int i = 0; i < ctx->mono_count; i++) {
        if (strcmp(ctx->mono_instances[i].name, mangled) == 0)
            return true;
    }
    return false;
}

void
llvm_register_mono(LLVMGenCtx *ctx, const char *mangled)
{
    PGY_DYNARR_ENSURE(ctx->mono_instances, ctx->mono_count,
                       ctx->mono_capacity, LLVMMonoInstance);

    ctx->mono_instances[ctx->mono_count].name = pergyra_strdup(mangled);
    ctx->mono_count++;
}

const char *
llvm_type_to_suffix(LLVMGenCtx *ctx, LLVMTypeRef ty)
{
    if (ty == ctx->type_i32)    return "Int";
    if (ty == ctx->type_i64)    return "Long";
    if (ty == ctx->type_f32)    return "Float";
    if (ty == ctx->type_f64)    return "Double";
    if (ty == ctx->type_i1)     return "Bool";
    if (ty == ctx->type_i8ptr)  return "String";
    return NULL;
}

LLVMValueRef
llvm_create_entry_alloca(LLVMGenCtx *ctx, LLVMTypeRef type, const char *name)
{
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(ctx->current_function);
    LLVMValueRef first = LLVMGetFirstInstruction(entry);

    LLVMBuilderRef tmp_builder = LLVMCreateBuilderInContext(ctx->context);
    if (first != NULL)
        LLVMPositionBuilderBefore(tmp_builder, first);
    else
        LLVMPositionBuilderAtEnd(tmp_builder, entry);

    LLVMValueRef alloca = LLVMBuildAlloca(tmp_builder, type, name);
    LLVMDisposeBuilder(tmp_builder);
    return alloca;
}

#endif
