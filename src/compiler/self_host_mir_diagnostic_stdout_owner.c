#include "self_host_mir_diagnostic_stdout_owner.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_child_io_authority.h"
#include "self_host_driver.h"
#include "self_host_public_diagnostic_wire_owner.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define PGY_MIR_DIAGNOSTIC_STDOUT_LIMIT (128u * 1024u * 1024u)
#define PGY_MIR_DIAGNOSTIC_TIMEOUT_MILLIS 300000u

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
        && flags->opt_profile == PGY_OPT_RELEASE
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
    unsigned char *payload = NULL;
    size_t payload_length = 0;
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
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv_capture_stdout(
        child_argv, PGY_MIR_DIAGNOSTIC_STDOUT_LIMIT,
        PGY_MIR_DIAGNOSTIC_TIMEOUT_MILLIS,
        &payload, &payload_length);
    if (rc != 0) {
        int wire_relay = 0;

        if (rc > 0 && flags->diag_format == DIAG_FORMAT_JSON
            && payload_length != 0) {
            wire_relay = driver_self_host_public_diagnostic_wire_relay(
                payload, payload_length);
            if (wire_relay != 0) {
                if (wire_relay < 0) {
                    fprintf(stderr, "pgy: failed while writing admitted self-host JSON diagnostic\n");
                    rc = 1;
                }
                free(payload); free(binary); free(source_path);
                return rc < 0 ? 1 : rc;
            }
            fprintf(stderr, "pgy: self-host JSON diagnostic receipt is malformed\n");
        } else if (rc == PGY_EXEC_CAPTURE_TIMEOUT)
            fprintf(stderr, "pgy: self-host MIR diagnostic timed out\n");
        else if (rc == PGY_EXEC_CAPTURE_OUTPUT_LIMIT)
            fprintf(stderr, "pgy: self-host MIR diagnostic exceeded its stdout limit\n");
        else if (rc == PGY_EXEC_CAPTURE_CRASHED)
            fprintf(stderr, "pgy: self-host MIR diagnostic child crashed\n");
        else if (rc < 0)
            fprintf(stderr, "pgy: failed to capture self-host MIR diagnostic\n");
        else
            fprintf(stderr,
                    "pgy: self-host driver failed (exit %d) emitting MIR diagnostic\n",
                    rc);
        free(payload); free(binary); free(source_path);
        return rc < 0 ? 1 : rc;
    }
    if (payload_length == 0) {
        fprintf(stderr, "pgy: self-host driver reported success without a MIR diagnostic payload\n");
        free(payload); free(binary); free(source_path);
        return 1;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "pgy: could not select binary MIR diagnostic stdout\n");
        free(payload); free(binary); free(source_path);
        return 1;
    }
#endif
    if (fwrite(payload, 1, payload_length, stdout) != payload_length
        || fflush(stdout) != 0) {
        fprintf(stderr, "pgy: failed while writing MIR diagnostic stdout\n");
        rc = 1;
    }
    free(payload); free(binary); free(source_path);
    return rc;
}
