#include "self_host_mir_diagnostic_stdout_owner.h"

#include "path_utils.h"
#include "self_host_driver.h"
#include "self_host_public_diagnostic_stdout_process_owner.h"

#include <stdio.h>
#include <stdlib.h>

static bool
driver_self_host_mir_diagnostic_request_supported(const DriverFlags *flags)
{
    return flags != NULL && flags->source_path != NULL
        && flags->output_path == NULL && flags->dump_mir
        && !flags->emit_c_only && !flags->emit_llvm_ir && !flags->do_run
        && !flags->dump_tokens && !flags->dump_ast && !flags->dump_dir
        && !flags->dump_capability_manifest
        && !flags->dump_rir && !flags->dump_rir_json
        && !flags->dump_machine_manifest_json
        && !flags->dump_air && !flags->dump_air_json
        && !flags->dump_mir_json && !flags->test_native_mir_json_oracle
        && !flags->dump_hir && !flags->check_only && !flags->verbose
        && !flags->repl && !flags->emit_debug_lines
        && (flags->diag_format == DIAG_FORMAT_TEXT
            || flags->diag_format == DIAG_FORMAT_JSON)
        && flags->runtime_mode == RUNTIME_DEFAULT
        && flags->machine_layer_physical_manifest == NULL;
}

int
driver_run_self_host_mir_diagnostic_request(const char *launcher_path,
                                            const DriverFlags *flags)
{
    const char *child_argv[4];
    char *binary;
    char *source_path;
    int rc;

    if (!driver_self_host_mir_diagnostic_request_supported(flags)) {
        fprintf(stderr, "pgy: --mir options are outside the installed self-host driver contract\n");
        return 1;
    }
    source_path = driver_self_host_source_identity_path_dup(flags->source_path);
    if (source_path == NULL) {
        fprintf(stderr, "pgy: could not canonicalize self-host MIR diagnostic source identity\n");
        return 1;
    }
    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr, "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary); free(source_path);
        return 1;
    }
    child_argv[0] = binary;
    child_argv[1] = flags->diag_format == DIAG_FORMAT_JSON
        ? "--emit-mir-json-diagnostic-verified"
        : "--emit-mir-diagnostic-verified";
    child_argv[2] = source_path;
    child_argv[3] = NULL;
    rc = driver_run_self_host_public_diagnostic_stdout_process(
        child_argv, DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_MIR,
        flags->diag_format == DIAG_FORMAT_JSON);
    free(binary); free(source_path);
    return rc;
}
