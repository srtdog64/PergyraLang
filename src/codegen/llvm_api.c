/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend public API and target-machine pipeline helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_bitcode_freshness.h"
#include "../common/string_compat.h"
#include <llvm-c/IRReader.h>
#include <llvm-c/Linker.h>

static void
llvm_debug_stage(const char *stage)
{
    if (stage != NULL && llvm_debug_stage_enabled())
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

static LLVMGenResult *
llvm_result_from_ctx_error(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return llvm_result_error("LLVM context is NULL");

    if (ctx->error_line > 0) {
        return llvm_result_error_fmt_with_hints(
            ctx->error_code,
            ctx->error_cause_ir,
            ctx->error_fix_source,
            "line %u:%u: %s",
            ctx->error_line,
            ctx->error_column,
            ctx->error_msg[0] != '\0' ? ctx->error_msg : "LLVM backend error");
    }

    return llvm_result_error_fmt_with_hints(
        ctx->error_code,
        ctx->error_cause_ir,
        ctx->error_fix_source,
        "%s",
        ctx->error_msg[0] != '\0' ? ctx->error_msg : "LLVM backend error");
}

static LLVMGenResult *
llvm_verify_module_result(LLVMGenCtx *ctx)
{
    char *verify_error = NULL;

    if (ctx == NULL)
        return llvm_result_error("LLVM context is NULL");

    if (LLVMVerifyModule(ctx->module, LLVMReturnStatusAction, &verify_error)) {
        LLVMGenResult *res;
        if (llvm_debug_verify_enabled()) {
            char *ir = LLVMPrintModuleToString(ctx->module);
            if (ir != NULL) {
                fprintf(stderr, "[llvm verify dump]\n%s\n", ir);
                LLVMDisposeMessage(ir);
            }
        }
        res = llvm_result_error_fmt("LLVM verify failed: %s",
            verify_error != NULL ? verify_error : "(unknown)");
        if (verify_error != NULL)
            LLVMDisposeMessage(verify_error);
        return res;
    }

    if (verify_error != NULL)
        LLVMDisposeMessage(verify_error);
    return NULL;
}

static void
llvm_init_all_targets(void)
{
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
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

/*
 * Runtime panics are the cold error family: any function whose name carries
 * the "panic" marker traps and never returns.
 */
static bool
llvm_fn_is_panic(const char *fn_name)
{
    return fn_name != NULL && strstr(fn_name, "panic") != NULL;
}

/*
 * Checked integer division/modulo carry fail-closed guards (divide-by-zero and
 * INT_MIN / -1 overflow). When the runtime bitcode is linked and inlined, a -O3
 * pass would constant-fold a literal INT_MIN / -1 (instcombine rewrites
 * `sdiv x, -1` to a negation) and discard the guard, so the surface program
 * silently produces a wrong value instead of panicking. Mark these functions
 * noinline + optnone so the guard always executes at runtime, matching the C
 * backend whose separately compiled runtime is never folded into the caller.
 */
static bool
llvm_fn_is_checked_arith(const char *fn_name)
{
    return fn_name != NULL
        && (strstr(fn_name, "pgy_checked_div_") != NULL
            || strstr(fn_name, "pgy_checked_mod_") != NULL);
}

/*
 * Functions that provably never return to their caller. The panic family
 * qualifies, plus an exact-name table of terminal runtime entrypoints.
 * Matching is exact (not substring) so that returning lookalikes such as
 * pgy_intent_exit_export, which exits an intent scope and returns, are never
 * mismarked noreturn.
 */
static bool
llvm_fn_never_returns(const char *fn_name)
{
    static const char *const exact_never_return[] = {
        "pgy_exit",
    };
    size_t i;

    if (fn_name == NULL)
        return false;
    if (llvm_fn_is_panic(fn_name))
        return true;
    for (i = 0; i < sizeof(exact_never_return) / sizeof(exact_never_return[0]); i++) {
        if (strcmp(fn_name, exact_never_return[i]) == 0)
            return true;
    }
    return false;
}

/*
 * Runtime helpers that touch no memory at all: pure scalar math over float
 * arguments. Marking them readnone lets the optimizer constant-fold, CSE, and
 * hoist calls out of loops. Random is excluded because it carries hidden
 * generator state and is therefore not pure.
 */
static bool
llvm_fn_is_readnone_runtime(const char *fn_name)
{
    static const char *const readnone_runtime[] = {
        "Sqrt", "Pow", "Floor", "Ceil", "Round",
        "Sin", "Cos", "Tan", "Asin", "Acos", "Atan", "Atan2",
        "Exp", "MathLog", "Log10", "Log2",
    };
    size_t i;

    if (fn_name == NULL)
        return false;
    for (i = 0; i < sizeof(readnone_runtime) / sizeof(readnone_runtime[0]); i++) {
        if (strcmp(fn_name, readnone_runtime[i]) == 0)
            return true;
    }
    return false;
}

/*
 * Runtime helpers that only read memory reachable through their arguments and
 * return a scalar without allocating. Marking them readonly lets the optimizer
 * CSE repeated calls (StringIndexOf and pgy_string_equals dominate the
 * self-hosted lexer hot path) and hoist loop-invariant ones. Allocating string
 * builders such as Substring and StringConcat are intentionally excluded: their
 * allocation is an observable effect that must not be removed or duplicated.
 */
static bool
llvm_fn_is_readonly_runtime(const char *fn_name)
{
    static const char *const readonly_runtime[] = {
        "StringContains", "StringIndexOf", "pgy_string_equals",
        "ToInt", "ToFloat",
    };
    size_t i;

    if (fn_name == NULL)
        return false;
    for (i = 0; i < sizeof(readonly_runtime) / sizeof(readonly_runtime[0]); i++) {
        if (strcmp(fn_name, readonly_runtime[i]) == 0)
            return true;
    }
    return false;
}

static void
llvm_add_fn_attr(LLVMGenCtx *ctx, LLVMValueRef fn, unsigned kind)
{
    if (kind != 0)
        LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
            LLVMCreateEnumAttribute(ctx->context, kind, 0));
}

static bool
llvm_module_has_runtime_call_use(LLVMGenCtx *ctx, const char *fn_name)
{
    LLVMValueRef fn;
    LLVMUseRef use;

    if (ctx == NULL || ctx->module == NULL || fn_name == NULL)
        return false;
    fn = LLVMGetNamedFunction(ctx->module, fn_name);
    if (fn == NULL)
        return false;
    for (use = LLVMGetFirstUse(fn); use != NULL; use = LLVMGetNextUse(use)) {
        if (LLVMGetUser(use) != NULL)
            return true;
    }
    return false;
}

/*
 * Pull the runtime into the module as LLVM IR before optimization so the
 * inliner can fold runtime primitives (Substring, StringConcat, ...) the way
 * the C backend's static-inline runtime is folded by gcc. The bitcode path is
 * taken from PGY_RUNTIME_BC or, failing that, the build-time PGY_RUNTIME_LIB_BC.
 * Every failure path (no path configured, file absent, parse or link error) is
 * a silent no-op that leaves the runtime as external calls, so a toolchain
 * without the prebuilt bitcode keeps working exactly as before.
 */
/* Turn a function definition into an external declaration by deleting its
 * body. Used to keep correctness-critical runtime out of the inlined bitcode
 * so calls resolve to the separately compiled runtime object instead. */
static void
llvm_strip_function_body(LLVMValueRef fn)
{
    LLVMBasicBlockRef bb;
    while ((bb = LLVMGetFirstBasicBlock(fn)) != NULL)
        LLVMDeleteBasicBlock(bb);
}

/*
 * The runtime bitcode is inlined for speed, but two families MUST NOT be folded
 * into the caller: fail-closed checked arithmetic (whose divide-by-zero and
 * INT_MIN/-1 guards -O3 would constant-fold away on literal operands) and the
 * exported panic entrypoints (whose inlined copies mis-lower stderr/abort and
 * crash with an access violation instead of printing and aborting). Stripping
 * their bodies before linking leaves external declarations, so they resolve to
 * the gcc-built runtime object, identical to the C backend, which never folds
 * its separately compiled runtime. Hot primitives such as Substring and
 * StringConcat are untouched and still inline.
 *
 * Static-inline pgy_runtime_panic_emit is excluded from that strip policy. It
 * is not an external ABI symbol, so its body must stay in bitcode when stale
 * local bitcode still references it.
 */
static void
llvm_exclude_critical_runtime_from_bitcode(LLVMModuleRef runtime_module)
{
    for (LLVMValueRef fn = LLVMGetFirstFunction(runtime_module);
         fn != NULL; fn = LLVMGetNextFunction(fn)) {
        const char *name = LLVMGetValueName(fn);
        bool strip_noreturn = llvm_fn_never_returns(name);
        if (name != NULL && strcmp(name, "pgy_runtime_panic_emit") == 0)
            strip_noreturn = false;
        if (!LLVMIsDeclaration(fn)
            && (llvm_fn_is_checked_arith(name) || strip_noreturn))
            llvm_strip_function_body(fn);
    }
}

static void
llvm_link_runtime_bitcode(LLVMGenCtx *ctx)
{
    const char *bc_path;
    LLVMMemoryBufferRef buffer = NULL;
    LLVMModuleRef runtime_module = NULL;
    const char *layout;
    char *message = NULL;

    if (ctx == NULL || ctx->module == NULL)
        return;
    if (llvm_module_has_runtime_call_use(ctx, "pgy_exit"))
        return;

    bc_path = getenv("PGY_RUNTIME_BC");
#ifdef PGY_RUNTIME_LIB_BC
    if (bc_path == NULL || bc_path[0] == '\0')
        bc_path = PGY_RUNTIME_LIB_BC;
#endif
    if (bc_path == NULL || bc_path[0] == '\0')
        return;
    if (!llvm_runtime_bitcode_is_fresh(bc_path))
        return;

    if (LLVMCreateMemoryBufferWithContentsOfFile(bc_path, &buffer, &message)) {
        if (message != NULL)
            LLVMDisposeMessage(message);
        return;
    }

    /* Parse takes ownership of the buffer on both success and failure. */
    if (LLVMParseIRInContext(ctx->context, buffer, &runtime_module, &message)) {
        if (message != NULL)
            LLVMDisposeMessage(message);
        return;
    }

    LLVMSetTarget(runtime_module, LLVMGetTarget(ctx->module));
    layout = LLVMGetDataLayoutStr(ctx->module);
    if (layout != NULL)
        LLVMSetDataLayout(runtime_module, layout);

    llvm_exclude_critical_runtime_from_bitcode(runtime_module);

    /* Link consumes runtime_module. The internalize pass below then gives the
     * merged definitions internal linkage so the inliner can fold and the dead
     * remainder is stripped. */
    LLVMLinkModules2(ctx->module, runtime_module);
}

static void
llvm_run_optimization(LLVMGenCtx *ctx, LLVMTargetMachineRef machine,
                      const char *triple, bool release_opt)
{
    llvm_apply_target_machine(ctx, machine, triple);
    llvm_link_runtime_bitcode(ctx);

    unsigned noreturn_kind = LLVMGetEnumAttributeKindForName("noreturn", 8);
    unsigned cold_kind = LLVMGetEnumAttributeKindForName("cold", 4);
    unsigned nounwind_kind = LLVMGetEnumAttributeKindForName("nounwind", 8);
    unsigned willreturn_kind = LLVMGetEnumAttributeKindForName("willreturn", 10);
    unsigned readnone_kind = LLVMGetEnumAttributeKindForName("readnone", 8);
    unsigned readonly_kind = LLVMGetEnumAttributeKindForName("readonly", 8);
    unsigned noinline_kind = LLVMGetEnumAttributeKindForName("noinline", 8);
    unsigned optnone_kind = LLVMGetEnumAttributeKindForName("optnone", 7);
    for (LLVMValueRef fn = LLVMGetFirstFunction(ctx->module);
         fn != NULL; fn = LLVMGetNextFunction(fn)) {
        const char *fn_name = LLVMGetValueName(fn);
        if (llvm_fn_never_returns(fn_name))
            llvm_add_fn_attr(ctx, fn, noreturn_kind);
        if (llvm_fn_is_panic(fn_name))
            llvm_add_fn_attr(ctx, fn, cold_kind);
        /* Keep fail-closed checked arithmetic out of the inliner/folder so its
         * overflow and divide-by-zero guards survive optimization. optnone
         * implies (and requires) noinline and excludes the body from IPA. */
        if (!LLVMIsDeclaration(fn) && llvm_fn_is_checked_arith(fn_name)) {
            llvm_add_fn_attr(ctx, fn, noinline_kind);
            llvm_add_fn_attr(ctx, fn, optnone_kind);
        }
        /*
         * Memory-effect attributes apply only to declarations (the external
         * runtime), never to user definitions that might shadow a builtin name
         * with a side-effecting body.
         */
        if (LLVMIsDeclaration(fn)) {
            llvm_add_fn_attr(ctx, fn, nounwind_kind);
            if (!llvm_fn_never_returns(fn_name))
                llvm_add_fn_attr(ctx, fn, willreturn_kind);
            if (llvm_fn_is_readnone_runtime(fn_name))
                llvm_add_fn_attr(ctx, fn, readnone_kind);
            else if (llvm_fn_is_readonly_runtime(fn_name))
                llvm_add_fn_attr(ctx, fn, readonly_kind);
            continue;
        }
        if (fn_name != NULL && strcmp(fn_name, "main") == 0)
            continue;
        LLVMSetLinkage(fn, LLVMInternalLinkage);
    }

    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    if (options == NULL)
        return;
    const char *pipeline = release_opt ? "default<O3>" : "default<O2>";
    LLVMErrorRef err = LLVMRunPasses(ctx->module, pipeline, machine, options);
    if (err != NULL) {
        char *msg = LLVMGetErrorMessage(err);
        if (msg != NULL)
            LLVMDisposeErrorMessage(msg);
    }
    LLVMDisposePassBuilderOptions(options);
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
    ctx->uses_intent_observability =
        llvm_active_uses_intent_observability(ctx);

    llvm_debug_stage("codegen_with_mir:validate_mir");
    verify_result = llvm_validate_mir_for_codegen(mir);
    if (verify_result != NULL) {
        llvm_ctx_destroy(ctx);
        return verify_result;
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
    ctx->uses_intent_observability =
        llvm_active_uses_intent_observability(ctx);

    llvm_debug_stage("codegen_to_object:validate_mir");
    verify_result = llvm_validate_mir_for_codegen(mir);
    if (verify_result != NULL) {
        llvm_ctx_destroy(ctx);
        return verify_result;
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
            LLVMGenResult *res;
            res = llvm_result_error_fmt("Object emit failed: %s",
                emit_error != NULL ? emit_error : "(unknown)");
            LLVMDisposeMessage(emit_error);
            LLVMDisposeTargetMachine(machine);
            if (triple != NULL)
                LLVMDisposeMessage(triple);
            if (cpu != NULL)
                LLVMDisposeMessage(cpu);
            if (features != NULL)
                LLVMDisposeMessage(features);
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

    pgy_arena_destroy(&res->owned_arena);
    free(res);
}

#endif
