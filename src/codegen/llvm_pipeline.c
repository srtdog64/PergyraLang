/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend pipeline helpers split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include <string.h>

#include "llvm_internal.h"

static void
llvm_pipeline_debug_stage(const char *stage)
{
    if (stage != NULL && llvm_debug_stage_enabled())
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

/* LLVM DEBUG_METADATA_VERSION / DWARF version for the module flags. */
#define PGY_DEBUG_METADATA_VERSION 3
#define PGY_DWARF_VERSION 4

static void
llvm_debug_add_flag_u32(LLVMGenCtx *ctx, const char *key, uint32_t value)
{
    LLVMMetadataRef md = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), value, 0));
    LLVMAddModuleFlag(ctx->module, LLVMModuleFlagBehaviorWarning,
                      key, strlen(key), md);
}

static void
llvm_debug_init(LLVMGenCtx *ctx)
{
    const char *path;
    const char *base;
    const char *slash;
    char dir[1024];
    size_t dirlen;

    ctx->di_enabled = false;
    ctx->di_builder = NULL;
    ctx->di_file = NULL;
    ctx->di_cu = NULL;
    ctx->di_scope = NULL;
    path = llvm_active_source_path(ctx);
    if (path == NULL)
        return;
    slash = strrchr(path, '/');
    if (slash != NULL) {
        base = slash + 1;
        dirlen = (size_t)(slash - path);
        if (dirlen >= sizeof(dir))
            dirlen = sizeof(dir) - 1;
        memcpy(dir, path, dirlen);
        dir[dirlen] = '\0';
    } else {
        base = path;
        dir[0] = '.';
        dir[1] = '\0';
        dirlen = 1;
    }
    llvm_debug_add_flag_u32(ctx, "Dwarf Version", PGY_DWARF_VERSION);
    llvm_debug_add_flag_u32(ctx, "Debug Info Version",
                            PGY_DEBUG_METADATA_VERSION);
    ctx->di_builder = LLVMCreateDIBuilder(ctx->module);
    ctx->di_file = LLVMDIBuilderCreateFile(ctx->di_builder, base, strlen(base),
                                           dir, dirlen);
    ctx->di_cu = LLVMDIBuilderCreateCompileUnit(
        ctx->di_builder, LLVMDWARFSourceLanguageC, ctx->di_file,
        "pergyra", 7, 0, "", 0, 0, "", 0, LLVMDWARFEmissionFull,
        0, 0, 0, "", 0, "", 0);
    ctx->di_enabled = true;
}

static void
llvm_debug_finalize(LLVMGenCtx *ctx)
{
    if (ctx->di_builder == NULL)
        return;
    LLVMSetCurrentDebugLocation2(ctx->builder, NULL);
    LLVMDIBuilderFinalize(ctx->di_builder);
    LLVMDisposeDIBuilder(ctx->di_builder);
    ctx->di_builder = NULL;
    ctx->di_scope = NULL;
}

void
llvm_debug_begin_function(LLVMGenCtx *ctx, const char *name,
                          LLVMValueRef fn, unsigned line)
{
    LLVMMetadataRef sub_type;
    LLVMMetadataRef sp;

    if (ctx == NULL || !ctx->di_enabled || fn == NULL)
        return;
    if (name == NULL)
        name = "fn";
    if (line == 0)
        line = 1;
    sub_type = LLVMDIBuilderCreateSubroutineType(ctx->di_builder, ctx->di_file,
                                                 NULL, 0, LLVMDIFlagZero);
    sp = LLVMDIBuilderCreateFunction(
        ctx->di_builder, ctx->di_file, name, strlen(name), name, strlen(name),
        ctx->di_file, line, sub_type, 0, 1, line, LLVMDIFlagZero, 0);
    LLVMSetSubprogram(fn, sp);
    ctx->di_scope = sp;
    llvm_debug_set_line(ctx, line);
}

void
llvm_debug_set_line(LLVMGenCtx *ctx, unsigned line)
{
    LLVMMetadataRef loc;

    if (ctx == NULL || !ctx->di_enabled || ctx->di_scope == NULL || line == 0)
        return;
    loc = LLVMDIBuilderCreateDebugLocation(ctx->context, line, 0,
                                           ctx->di_scope, NULL);
    LLVMSetCurrentDebugLocation2(ctx->builder, loc);
}

bool
llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx)
{
    LLVMMIRRoutineInventory routine_inventory;

    if (mir == NULL || ctx == NULL)
        return false;
    llvm_debug_init(ctx);
    llvm_active_routine_inventory(ctx, &routine_inventory);

    /* MIR-only backend entry:
     *
     * function/intent/method bodies lower from MIR routines,
     * and declaration / top-level orchestration state is read from
     * MIR-carried inventory rather than the original HIR program.
     *
     * Remaining debt is not "original HIR dependency" anymore; it is
     * that declaration inventory is still AST-carried inside MIRProgram
     * instead of a dedicated declaration IR.
     */
    llvm_pipeline_debug_stage("emit_program_from_mir:declare_runtime");
    llvm_declare_runtime(ctx);

    llvm_pipeline_debug_stage("emit_program_from_mir:register_decl_items");
    llvm_register_active_nominal_types(ctx);
    if (ctx->has_error)
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_domain_passes");
    llvm_emit_domain_passes(ctx);

    llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_funcs");
    if (!llvm_forward_declare_function_routines_from_inventory(
            ctx, &routine_inventory))
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_intents");
    llvm_forward_declare_intent_routines_from_inventory(
        ctx, &routine_inventory);
    if (ctx->has_error)
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_function_routines");
    if (!llvm_emit_function_routines_from_inventory(ctx, &routine_inventory))
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_residual_decls");
    if (!llvm_validate_function_routine_bodies_from_inventory(
            ctx, &routine_inventory))
        return false;
    llvm_emit_intent_routines_from_inventory(ctx, &routine_inventory);
    if (ctx->has_error)
        return false;

    if (!llvm_emit_class_method_bodies_from_inventory(ctx))
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_main_wrapper");
    llvm_emit_main_wrapper(ctx);
    llvm_debug_finalize(ctx);
    llvm_pipeline_debug_stage("emit_program_from_mir:end");
    return !ctx->has_error;
}

#endif
