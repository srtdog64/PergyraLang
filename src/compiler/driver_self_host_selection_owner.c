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
        && flags->opt_profile == PGY_OPT_RELEASE
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
        && flags->diag_format == DIAG_FORMAT_TEXT
        && flags->runtime_mode == RUNTIME_DEFAULT
        && flags->machine_layer_physical_manifest == NULL;
}

bool
driver_self_host_llvm_artifact_request_supported(const DriverFlags *flags)
{
    return driver_plain_llvm_binary_target_requested(flags)
        && !flags->emit_debug_lines
        && flags->diag_format == DIAG_FORMAT_TEXT
        && flags->runtime_mode == RUNTIME_DEFAULT
        && flags->machine_layer_physical_manifest == NULL;
}
