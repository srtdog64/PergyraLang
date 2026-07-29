/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM host target-machine creation and module target application.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_target_machine_internal.h"

static void
llvm_init_all_targets(void)
{
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
}

LLVMTargetMachineRef
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

void
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

#endif /* PGY_LLVM_ENABLED */
