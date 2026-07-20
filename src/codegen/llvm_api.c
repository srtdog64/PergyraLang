/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend public API and target-machine pipeline helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_attrs.h"
#include "llvm_runtime_bitcode_freshness.h"
#include "../common/string_compat.h"
#include "../compiler/verified_projection_plan.h"
#include <llvm-c/IRReader.h>
#include <llvm-c/Linker.h>

static void
llvm_debug_stage(const char *stage)
{
    if (stage != NULL && llvm_debug_stage_enabled())
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

bool
llvm_machine_layer_projection_is_bound(const LLVMGenCtx *ctx)
{
    return ctx != NULL
        && ctx->projection_plan != NULL
        && ctx->projection_plan->verified
        && ctx->projection_plan->target == PGY_PROJECTION_TARGET_LLVM
        && ctx->projection_plan->machine_layer_manifest_fingerprint != 0
        && ctx->projection_plan->machine_layer_physical_manifest_fingerprint != 0;
}

static bool
llvm_apply_intent_observability_projection_plan(LLVMGenCtx *ctx)
{
    PgyVerifiedProjectionPlanRow row;

    if (ctx == NULL || ctx->projection_plan == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "%s", "LLVM backend: verified projection plan required");
        return false;
    }
    row = *ctx->projection_plan;
    if (!row.verified || row.target != PGY_PROJECTION_TARGET_LLVM) {
        llvm_set_mir_inventory_missing(ctx, "%s",
            "LLVM backend: projection plan is not verified for LLVM");
        return false;
    }
    if (!pgy_verified_projection_plan_identity_ready(&row)) {
        llvm_set_mir_inventory_missing(ctx, "%s",
            "LLVM backend: verified projection plan identity is invalid");
        return false;
    }
    if (row.air_certificate_schema == NULL
        || row.air_certificate_fingerprint == 0) {
        llvm_set_mir_inventory_missing(ctx, "%s",
            "LLVM backend: AIR evidence certificate is missing from projection plan");
        return false;
    }
    if (row.target_capability_fingerprint == 0) {
        llvm_set_mir_inventory_missing(ctx, "%s",
            "LLVM backend: target capability fingerprint is missing");
        return false;
    }
    if (row.machine_layer_manifest_fingerprint == 0) {
        llvm_set_mir_inventory_missing(ctx, "%s",
            "LLVM backend: machine-layer manifest fingerprint is missing");
        return false;
    }
    if (row.machine_layer_physical_manifest_fingerprint == 0) {
        llvm_set_mir_inventory_missing(ctx,
            "%s", "LLVM backend: physical machine declaration fingerprint is missing");
        return false;
    }
    ctx->uses_intent_observability =
        row.disposition == PGY_PROJECTION_MATERIALIZE;
    return true;
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
        /* Only external symbols may be stripped: replacing a static
         * (internal-linkage) body with a declaration cannot be resolved by
         * the linker against the runtime object and broke HashMap on the
         * LLVM backend once (pgy_map_grow_raw_export). Enforce that
         * structurally here instead of relying on predicate discipline
         * alone (docs/189 C5). */
        if (LLVMGetLinkage(fn) != LLVMExternalLinkage)
            continue;
        if (!LLVMIsDeclaration(fn)
            && (llvm_fn_is_checked_arith(name)
                || llvm_fn_is_lifecycle_runtime(name)
                || llvm_fn_is_capability_runtime(name)
                || llvm_fn_is_budget_runtime(name)
                || llvm_fn_is_bounds_checked_accessor(name)
                || llvm_fn_is_stateful_runtime(name)
                || strip_noreturn))
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
llvm_codegen_mir_only(const MIRProgram *mir,
                      const PgyVerifiedProjectionPlanRow *projection_plan,
                      const PgySpawnLanePlan *spawn_lane_plan,
                      const char *module_name)
{
    llvm_debug_stage("codegen_with_mir:ctx_create");
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    LLVMGenResult *verify_result;

    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    ctx->mir = mir;
    ctx->projection_plan = projection_plan;
    ctx->spawn_lane_plan = spawn_lane_plan;
    if (spawn_lane_plan == NULL || !spawn_lane_plan->verified
        || spawn_lane_plan->revision != PGY_SPAWN_LANE_PLAN_REVISION) {
        llvm_ctx_destroy(ctx);
        return llvm_result_error(
            "LLVM backend: verified spawn-lane plan required");
    }

    llvm_debug_stage("codegen_with_mir:validate_mir");
    verify_result = llvm_validate_mir_for_codegen(mir);
    if (verify_result != NULL) {
        llvm_ctx_destroy(ctx);
        return verify_result;
    }
    if (!llvm_apply_intent_observability_projection_plan(ctx)) {
        LLVMGenResult *res = llvm_result_from_ctx_error(ctx);
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
    return llvm_codegen_mir_only(mir, NULL, NULL, module_name);
}

LLVMGenResult *
llvm_codegen_from_mir_with_projection_plan(
    const MIRProgram *mir,
    const PgyVerifiedProjectionPlanRow *projection_plan,
    const PgySpawnLanePlan *spawn_lane_plan,
    const char *module_name)
{
    return llvm_codegen_mir_only(mir, projection_plan, spawn_lane_plan,
                                 module_name);
}

static LLVMGenResult *
llvm_codegen_to_object_core(const MIRProgram *mir,
                            const PgyVerifiedProjectionPlanRow *projection_plan,
                            const PgySpawnLanePlan *spawn_lane_plan,
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
    ctx->projection_plan = projection_plan;
    ctx->spawn_lane_plan = spawn_lane_plan;
    if (spawn_lane_plan == NULL || !spawn_lane_plan->verified
        || spawn_lane_plan->revision != PGY_SPAWN_LANE_PLAN_REVISION) {
        llvm_ctx_destroy(ctx);
        return llvm_result_error(
            "LLVM backend: verified spawn-lane plan required");
    }

    llvm_debug_stage("codegen_to_object:validate_mir");
    verify_result = llvm_validate_mir_for_codegen(mir);
    if (verify_result != NULL) {
        llvm_ctx_destroy(ctx);
        return verify_result;
    }
    if (!llvm_apply_intent_observability_projection_plan(ctx)) {
        LLVMGenResult *res = llvm_result_from_ctx_error(ctx);
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
    return llvm_codegen_to_object_core(mir, NULL, NULL, module_name,
                                       output_path, release_opt);
}

LLVMGenResult *
llvm_codegen_to_object_from_mir_with_projection_plan(
                                         const MIRProgram *mir,
                                         const PgyVerifiedProjectionPlanRow *projection_plan,
                                         const PgySpawnLanePlan *spawn_lane_plan,
                                         const char *module_name,
                                         const char *output_path,
                                         bool release_opt)
{
    return llvm_codegen_to_object_core(mir, projection_plan, spawn_lane_plan,
                                       module_name, output_path, release_opt);
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
