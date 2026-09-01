#include "self_host_public_diagnostic_stdout_process_owner.h"

#include "compiler_process.h"
#include "self_host_child_io_authority.h"
#include "self_host_public_diagnostic_wire_owner.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define PGY_PUBLIC_DIAGNOSTIC_STDOUT_LIMIT (128u * 1024u * 1024u)
#define PGY_PUBLIC_DIAGNOSTIC_STDOUT_TIMEOUT_MILLIS 300000u

static const char *
driver_public_diagnostic_stdout_name(
    DriverSelfHostPublicDiagnosticStdoutKind kind)
{
    if (kind == DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_AST)
        return "AST diagnostic";
    return kind == DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_TOKENS
        ? "token diagnostic" : "MIR diagnostic";
}

int
driver_run_self_host_public_diagnostic_stdout_process(
    const char *const argv[],
    DriverSelfHostPublicDiagnosticStdoutKind kind,
    bool emit_json_diagnostic)
{
    const char *name;
    const char *article;
    unsigned char *payload = NULL;
    size_t payload_length = 0;
    int rc;

    if (argv == NULL || (kind != DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_AST
        && kind != DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_MIR
        && kind != DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_TOKENS)) {
        fprintf(stderr, "pgy: invalid self-host public diagnostic stdout request\n");
        return 1;
    }
    name = driver_public_diagnostic_stdout_name(kind);
    article = kind == DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_AST ? "an" : "a";
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv_capture_stdout(
        argv, PGY_PUBLIC_DIAGNOSTIC_STDOUT_LIMIT,
        PGY_PUBLIC_DIAGNOSTIC_STDOUT_TIMEOUT_MILLIS,
        &payload, &payload_length);
    if (rc != 0) {
        int wire_relay = 0;

        if (rc > 0 && emit_json_diagnostic && payload_length != 0) {
            wire_relay = driver_self_host_public_diagnostic_wire_relay(
                payload, payload_length);
            if (wire_relay != 0) {
                if (wire_relay < 0) {
                    fprintf(stderr,
                            "pgy: failed while writing admitted self-host JSON diagnostic\n");
                    rc = 1;
                }
                free(payload);
                return rc < 0 ? 1 : rc;
            }
            fprintf(stderr, "pgy: self-host JSON diagnostic receipt is malformed\n");
        } else if (rc == PGY_EXEC_CAPTURE_TIMEOUT)
            fprintf(stderr, "pgy: self-host %s timed out\n", name);
        else if (rc == PGY_EXEC_CAPTURE_OUTPUT_LIMIT)
            fprintf(stderr, "pgy: self-host %s exceeded its stdout limit\n", name);
        else if (rc == PGY_EXEC_CAPTURE_CRASHED)
            fprintf(stderr, "pgy: self-host %s child crashed\n", name);
        else if (rc < 0)
            fprintf(stderr, "pgy: failed to capture self-host %s\n", name);
        else
            fprintf(stderr,
                    "pgy: self-host driver failed (exit %d) emitting %s\n",
                    rc, name);
        free(payload);
        return rc < 0 ? 1 : rc;
    }
    if (payload_length == 0) {
        fprintf(stderr,
                "pgy: self-host driver reported success without %s %s payload\n",
                article, name);
        free(payload);
        return 1;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "pgy: could not select binary %s stdout\n", name);
        free(payload);
        return 1;
    }
#endif
    if (fwrite(payload, 1, payload_length, stdout) != payload_length
        || fflush(stdout) != 0) {
        fprintf(stderr, "pgy: failed while writing %s stdout\n", name);
        rc = 1;
    }
    free(payload);
    return rc;
}
