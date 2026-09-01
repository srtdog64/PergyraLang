#include "driver_self_host_selection_owner.h"

static bool
driver_plain_binary_target_requested(const DriverFlags *flags,
                                     BackendKind backend)
{
    return flags != NULL
        && flags->backend == backend
        && !flags->emit_c_only
        && !flags->emit_llvm_ir
        && !flags->dump_tokens
        && !flags->dump_ast
        && !flags->dump_capability_manifest
        && !flags->dump_dir
        && !flags->dump_rir
        && !flags->dump_rir_json
        && !flags->dump_machine_manifest_json
        && !flags->dump_air
        && !flags->dump_air_json
        && !flags->dump_mir
        && !flags->dump_mir_json
        && !flags->dump_hir
        && !flags->check_only
        && !flags->repl;
}

bool
driver_plain_c_binary_target_requested(const DriverFlags *flags)
{
    return driver_plain_binary_target_requested(flags, BACKEND_C);
}

bool
driver_plain_llvm_binary_target_requested(const DriverFlags *flags)
{
    return driver_plain_binary_target_requested(flags, BACKEND_LLVM);
}

bool
driver_self_host_machine_manifest_request_supported(const DriverFlags *flags)
{
    return flags != NULL
        && flags->source_path == NULL && flags->output_path == NULL
        && flags->dump_machine_manifest_json
        && !flags->emit_c_only && !flags->emit_llvm_ir && !flags->do_run
        && !flags->dump_tokens && !flags->dump_ast
        && !flags->dump_capability_manifest
        && !flags->dump_dir && !flags->dump_rir && !flags->dump_rir_json
        && !flags->dump_air && !flags->dump_air_json
        && !flags->dump_mir && !flags->dump_mir_json && !flags->dump_hir
        && !flags->check_only && !flags->verbose && !flags->repl
        && !flags->emit_debug_lines
        && flags->opt_profile == PGY_OPT_RELEASE
        && flags->diag_format == DIAG_FORMAT_TEXT
        && flags->runtime_mode == RUNTIME_DEFAULT
        && flags->machine_layer_physical_manifest == NULL;
}

const char *
driver_self_host_source_stdout_mode(const DriverFlags *flags)
{
    if (flags == NULL || flags->source_path == NULL || flags->output_path != NULL
        || (flags->dump_tokens ? 1 : 0) + (flags->dump_ast ? 1 : 0)
            + (flags->dump_capability_manifest ? 1 : 0)
            + (flags->dump_dir ? 1 : 0) != 1
        || flags->emit_c_only || flags->emit_llvm_ir || flags->do_run
        || flags->dump_rir || flags->dump_rir_json
        || flags->dump_machine_manifest_json || flags->dump_air
        || flags->dump_air_json || flags->dump_mir || flags->dump_mir_json
        || flags->test_native_mir_json_oracle || flags->dump_hir
        || flags->check_only || flags->verbose || flags->repl
        || flags->emit_debug_lines
        || flags->diag_format != DIAG_FORMAT_TEXT
        || flags->runtime_mode != RUNTIME_DEFAULT
        || flags->machine_layer_physical_manifest != NULL)
        return NULL;
    if (flags->dump_tokens)
        return "--tokens";
    if (flags->dump_ast)
        return "--ast";
    return flags->dump_capability_manifest
        ? "--emit-capability-manifest-verified" : "--emit-dir-verified";
}

bool
driver_self_host_mir_json_request_supported(const DriverFlags *flags)
{
    return flags != NULL
        && flags->source_path != NULL
        && flags->output_path == NULL
        && flags->dump_mir_json
        && !flags->test_native_mir_json_oracle
        && !flags->emit_c_only && !flags->emit_llvm_ir && !flags->do_run
        && !flags->dump_tokens && !flags->dump_ast
        && !flags->dump_capability_manifest
        && !flags->dump_dir && !flags->dump_rir && !flags->dump_rir_json
        && !flags->dump_machine_manifest_json
        && !flags->dump_air && !flags->dump_air_json
        && !flags->dump_mir && !flags->dump_hir
        && !flags->check_only && !flags->verbose && !flags->repl
        && !flags->emit_debug_lines
        && flags->diag_format == DIAG_FORMAT_TEXT
        && flags->runtime_mode == RUNTIME_DEFAULT
        && flags->machine_layer_physical_manifest == NULL;
}

bool
driver_self_host_c_artifact_request_supported(const DriverFlags *flags)
{
    return flags != NULL
        && flags->backend == BACKEND_C
        && !flags->emit_llvm_ir
        && !flags->dump_tokens
        && !flags->dump_ast
        && !flags->dump_capability_manifest
        && !flags->dump_dir
        && !flags->dump_rir
        && !flags->dump_rir_json
        && !flags->dump_machine_manifest_json
        && !flags->dump_air
        && !flags->dump_air_json
        && !flags->dump_mir
        && !flags->dump_mir_json
        && !flags->dump_hir
        && !flags->check_only
        && !flags->repl
        && !flags->emit_debug_lines
        && (flags->diag_format == DIAG_FORMAT_TEXT || flags->diag_format == DIAG_FORMAT_JSON)
        && flags->runtime_mode == RUNTIME_DEFAULT
        && flags->machine_layer_physical_manifest == NULL;
}

const char *driver_self_host_unowned_ir_option(const DriverFlags *flags)
{
    return flags == NULL ? NULL : flags->dump_rir ? "--rir" : flags->dump_rir_json ? "--rir-json" :
        flags->dump_air ? "--air" : flags->dump_air_json ? "--air-json" : flags->dump_hir ?
        (flags->hir_dump_mode == HIR_DUMP_CFG ? "--hir-cfg" :
         flags->hir_dump_mode == HIR_DUMP_DOM ? "--hir-dom" : flags->hir_dump_mode == HIR_DUMP_SSA ? "--hir-ssa" : "--hir") : NULL;
}
