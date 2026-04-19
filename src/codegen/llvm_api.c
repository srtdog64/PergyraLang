/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend public API and target-machine pipeline helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../common/string_compat.h"

static void
llvm_debug_stage(const char *stage)
{
    if (stage != NULL && getenv("PGY_DEBUG_LLVM_STAGE") != NULL)
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

static LLVMGenResult *
llvm_result_from_ctx_error(LLVMGenCtx *ctx)
{
    char msg[1024];

    if (ctx == NULL)
        return llvm_result_error("LLVM context is NULL");

    if (ctx->error_line > 0) {
        snprintf(msg, sizeof(msg), "line %u:%u: %s",
                 ctx->error_line, ctx->error_column, ctx->error_msg);
    } else {
        snprintf(msg, sizeof(msg), "%s", ctx->error_msg);
    }
    LLVMGenResult *res = llvm_result_error(msg);
    if (res != NULL) {
        if (ctx->error_code != NULL)
            res->error_code = pergyra_strdup(ctx->error_code);
        if (ctx->error_cause_ir != NULL)
            res->error_cause_ir = pergyra_strdup(ctx->error_cause_ir);
        if (ctx->error_fix_source != NULL)
            res->error_fix_source = pergyra_strdup(ctx->error_fix_source);
    }
    return res;
}

static LLVMGenResult *
llvm_verify_module_result(LLVMGenCtx *ctx)
{
    char *verify_error = NULL;

    if (ctx == NULL)
        return llvm_result_error("LLVM context is NULL");

    if (LLVMVerifyModule(ctx->module, LLVMReturnStatusAction, &verify_error)) {
        char msg[1024];
        if (getenv("PGY_DEBUG_LLVM_VERIFY") != NULL) {
            char *ir = LLVMPrintModuleToString(ctx->module);
            if (ir != NULL) {
                fprintf(stderr, "[llvm verify dump]\n%s\n", ir);
                LLVMDisposeMessage(ir);
            }
        }
        snprintf(msg, sizeof(msg), "LLVM verify failed: %s",
                 verify_error != NULL ? verify_error : "(unknown)");
        LLVMDisposeMessage(verify_error);
        return llvm_result_error(msg);
    }

    LLVMDisposeMessage(verify_error);
    return NULL;
}

static void
llvm_init_all_targets(void)
{
    static bool initialized = false;

    if (initialized)
        return;

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    initialized = true;
}

static LLVMTargetMachineRef
llvm_create_host_machine(char **triple_out, char **cpu_out, char **features_out)
{
    char *triple;
    char *cpu;
    char *features;
    LLVMTargetRef target = NULL;
    char *target_error = NULL;
    LLVMTargetMachineRef machine = NULL;

    if (triple_out != NULL)
        *triple_out = NULL;
    if (cpu_out != NULL)
        *cpu_out = NULL;
    if (features_out != NULL)
        *features_out = NULL;

    llvm_init_all_targets();

    triple = LLVMGetDefaultTargetTriple();
#ifdef __MINGW32__
    LLVMDisposeMessage(triple);
    triple = LLVMCreateMessage("x86_64-w64-windows-gnu");
#endif
    cpu = LLVMGetHostCPUName();
    features = LLVMGetHostCPUFeatures();

    if (triple != NULL && !LLVMGetTargetFromTriple(triple, &target, &target_error)) {
        machine = LLVMCreateTargetMachine(
            target,
            triple,
            cpu != NULL ? cpu : "generic",
            features != NULL ? features : "",
            LLVMCodeGenLevelAggressive,
            LLVMRelocDefault,
            LLVMCodeModelDefault);
    }

    if (target_error != NULL)
        LLVMDisposeMessage(target_error);

    if (machine == NULL) {
        if (triple != NULL)
            LLVMDisposeMessage(triple);
        if (cpu != NULL)
            LLVMDisposeMessage(cpu);
        if (features != NULL)
            LLVMDisposeMessage(features);
        return NULL;
    }

    if (triple_out != NULL)
        *triple_out = triple;
    else if (triple != NULL)
        LLVMDisposeMessage(triple);

    if (cpu_out != NULL)
        *cpu_out = cpu;
    else if (cpu != NULL)
        LLVMDisposeMessage(cpu);

    if (features_out != NULL)
        *features_out = features;
    else if (features != NULL)
        LLVMDisposeMessage(features);

    return machine;
}

static void
llvm_apply_target_machine(LLVMGenCtx *ctx, LLVMTargetMachineRef machine,
                          const char *triple)
{
    LLVMTargetDataRef layout;

    if (ctx == NULL || machine == NULL || triple == NULL)
        return;

    layout = LLVMCreateTargetDataLayout(machine);
    LLVMSetModuleDataLayout(ctx->module, layout);
    LLVMSetTarget(ctx->module, triple);
    LLVMDisposeTargetData(layout);
}

static void
llvm_run_optimization(LLVMGenCtx *ctx, LLVMTargetMachineRef machine,
                      const char *triple, bool release_opt)
{
    llvm_apply_target_machine(ctx, machine, triple);
    (void)release_opt;
}

static LLVMGenResult *
llvm_codegen_mir_only(const MIRProgram *mir, const char *module_name)
{
    llvm_debug_stage("codegen_with_mir:ctx_create");
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    LLVMGenResult *verify_result;

    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    ctx->mir = mir;

    llvm_debug_stage("codegen_with_mir:validate_mir");
    char *mir_error = NULL;
    if (!llvm_validate_mir_for_codegen(mir, &mir_error)) {
        LLVMGenResult *res = llvm_result_error(
            mir_error != NULL ? mir_error : "Invalid MIR program");
        free(mir_error);
        llvm_ctx_destroy(ctx);
        return res;
    }
    llvm_debug_stage("codegen_with_mir:emit_program_from_mir");
    if (!llvm_emit_program_from_mir(mir, ctx)) {
        LLVMGenResult *res = llvm_result_from_ctx_error(ctx);
        llvm_ctx_destroy(ctx);
        return res;
    }

    if (ctx->has_error) {
        LLVMGenResult *res = llvm_result_from_ctx_error(ctx);
        llvm_ctx_destroy(ctx);
        return res;
    }

    llvm_debug_stage("codegen_with_mir:verify");
    verify_result = llvm_verify_module_result(ctx);
    if (verify_result != NULL) {
        llvm_ctx_destroy(ctx);
        return verify_result;
    }

    {
        llvm_debug_stage("codegen_with_mir:print_module");
        char *ir = LLVMPrintModuleToString(ctx->module);
        char *ir_copy = pergyra_strdup(ir);
        LLVMGenResult *res;
        LLVMDisposeMessage(ir);

        res = llvm_result_success(ir_copy);
        if (res != NULL)
            res->uses_intent_observability = ctx->uses_intent_observability;
        llvm_debug_stage("codegen_with_mir:ctx_destroy");
        llvm_ctx_destroy(ctx);
        llvm_debug_stage("codegen_with_mir:return");
        return res;
    }
}

LLVMGenResult *
llvm_codegen_from_mir(const MIRProgram *mir, const char *module_name)
{
    return llvm_codegen_mir_only(mir, module_name);
}

static LLVMGenResult *
llvm_codegen_to_object_core(const MIRProgram *mir,
                            const char *module_name,
                            const char *output_path,
                            bool release_opt)
{
    llvm_debug_stage("codegen_to_object:ctx_create");
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    LLVMGenResult *verify_result;
    char *triple = NULL;
    char *cpu = NULL;
    char *features = NULL;
    LLVMTargetMachineRef machine;

    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    ctx->mir = mir;

    llvm_debug_stage("codegen_to_object:validate_mir");
    char *mir_error = NULL;
    if (!llvm_validate_mir_for_codegen(mir, &mir_error)) {
        LLVMGenResult *res = llvm_result_error(
            mir_error != NULL ? mir_error : "Invalid MIR program");
        free(mir_error);
        llvm_ctx_destroy(ctx);
        return res;
    }
    llvm_debug_stage("codegen_to_object:emit_program_from_mir");
    if (!llvm_emit_program_from_mir(mir, ctx)) {
        LLVMGenResult *res = llvm_result_from_ctx_error(ctx);
        llvm_ctx_destroy(ctx);
        return res;
    }

    if (ctx->has_error) {
        LLVMGenResult *res = llvm_result_from_ctx_error(ctx);
        llvm_ctx_destroy(ctx);
        return res;
    }

    llvm_debug_stage("codegen_to_object:verify");
    verify_result = llvm_verify_module_result(ctx);
    if (verify_result != NULL) {
        llvm_ctx_destroy(ctx);
        return verify_result;
    }

    {
        const char *dump_path = getenv("PGY_LLVM_DUMP_OBJ_IR");
        if (dump_path != NULL) {
            char *pre_opt = LLVMPrintModuleToString(ctx->module);
            FILE *fp = fopen(dump_path, "w");
            if (fp != NULL) {
                fputs(pre_opt, fp);
                fclose(fp);
            }
            LLVMDisposeMessage(pre_opt);
        }
    }

    llvm_debug_stage("codegen_to_object:create_machine");
    machine = llvm_create_host_machine(&triple, &cpu, &features);
    if (machine == NULL) {
        LLVMGenResult *res = llvm_result_error("Cannot create LLVM target machine");
        llvm_ctx_destroy(ctx);
        return res;
    }

    llvm_debug_stage("codegen_to_object:optimize");
    llvm_run_optimization(ctx, machine, triple, release_opt);
    llvm_apply_target_machine(ctx, machine, triple);

    {
        char *emit_error = NULL;
        llvm_debug_stage("codegen_to_object:emit_file");
        if (LLVMTargetMachineEmitToFile(machine, ctx->module,
                                        (char *)output_path,
                                        LLVMObjectFile, &emit_error)) {
            char msg[1024];
            LLVMGenResult *res;
            snprintf(msg, sizeof(msg), "Object emit failed: %s",
                     emit_error != NULL ? emit_error : "(unknown)");
            LLVMDisposeMessage(emit_error);
            LLVMDisposeTargetMachine(machine);
            if (triple != NULL)
                LLVMDisposeMessage(triple);
            if (cpu != NULL)
                LLVMDisposeMessage(cpu);
            if (features != NULL)
                LLVMDisposeMessage(features);
            res = llvm_result_error(msg);
            llvm_ctx_destroy(ctx);
            return res;
        }
    }

    llvm_debug_stage("codegen_to_object:dispose_machine");
    LLVMDisposeTargetMachine(machine);
    if (triple != NULL)
        LLVMDisposeMessage(triple);
    if (cpu != NULL)
        LLVMDisposeMessage(cpu);
    if (features != NULL)
        LLVMDisposeMessage(features);

    {
        LLVMGenResult *res = llvm_result_success(NULL);
        if (res != NULL)
            res->uses_intent_observability = ctx->uses_intent_observability;
        llvm_debug_stage("codegen_to_object:ctx_destroy");
        llvm_ctx_destroy(ctx);
        llvm_debug_stage("codegen_to_object:return");
        return res;
    }
}

LLVMGenResult *
llvm_codegen_to_object_from_mir(const MIRProgram *mir,
                                const char *module_name,
                                const char *output_path,
                                bool release_opt)
{
    return llvm_codegen_to_object_core(mir, module_name, output_path,
                                       release_opt);
}

void
llvm_gen_result_destroy(LLVMGenResult *res)
{
    if (res == NULL)
        return;

    free(res->error_message);
    free(res->error_code);
    free(res->error_cause_ir);
    free(res->error_fix_source);
    free(res->ir_text);
    free(res);
}

#endif
