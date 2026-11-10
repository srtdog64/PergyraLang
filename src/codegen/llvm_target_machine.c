/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM host target-machine creation and module target application.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_target_machine_internal.h"

#include <string.h>

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
llvm_create_host_machine(bool release_opt, char **triple_out,
                         char **cpu_out, char **features_out)
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
            /* Dev already carries verified, O2-optimized IR. Re-running
             * aggressive machine passes on every compiler-scale routine
             * defeats its bounded edit loop; release keeps that policy. */
            release_opt ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone,
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

    /* An emitted caller must carry the target's actual features before it
     * consumes Clang runtime bitcode. Otherwise the inliner compares an empty
     * caller feature set with the runtime's concrete target and refuses it. */
    char *cpu = LLVMGetTargetMachineCPU(machine);
    char *features = LLVMGetTargetMachineFeatureString(machine);
    for (LLVMValueRef fn = LLVMGetFirstFunction(ctx->module);
         fn != NULL; fn = LLVMGetNextFunction(fn)) {
        if (LLVMIsDeclaration(fn))
            continue;
        if (LLVMGetStringAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                "target-cpu", 10) == NULL && cpu != NULL)
            LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                LLVMCreateStringAttribute(ctx->context, "target-cpu", 10,
                    cpu, (unsigned)strlen(cpu)));
        if (LLVMGetStringAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                "target-features", 15) == NULL && features != NULL)
            LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                LLVMCreateStringAttribute(ctx->context, "target-features", 15,
                    features, (unsigned)strlen(features)));
    }
    LLVMDisposeMessage(cpu);
    LLVMDisposeMessage(features);
}

#endif /* PGY_LLVM_ENABLED */
