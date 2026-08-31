#include "self_host_artifact_process_owner.h"

#include "compiler_process.h"
#include "self_host_public_diagnostic_wire_owner.h"

#include <stdio.h>
#include <stdlib.h>

#define PGY_ARTIFACT_DIAGNOSTIC_LIMIT (128u * 1024u * 1024u)
#define PGY_ARTIFACT_DIAGNOSTIC_TIMEOUT_MILLIS 300000u

int
driver_run_self_host_artifact_process(
    const char *const argv[], bool verbose, bool emit_json_diagnostic)
{
    unsigned char *payload = NULL;
    size_t payload_length = 0;
    int rc;
    int wire_relay;

    if (!emit_json_diagnostic)
        return pgy_exec_argv(argv, verbose);

    rc = pgy_exec_argv_capture_stdout(
        argv, PGY_ARTIFACT_DIAGNOSTIC_LIMIT,
        PGY_ARTIFACT_DIAGNOSTIC_TIMEOUT_MILLIS, &payload, &payload_length);
    if (rc == 0) {
        if (payload_length != 0) {
            fprintf(stderr,
                    "pgy: self-host artifact producer succeeded with an unexpected diagnostic payload\n");
            rc = 1;
        }
        free(payload);
        return rc;
    }

    if (rc > 0 && payload_length != 0) {
        wire_relay = driver_self_host_public_diagnostic_wire_relay(
            payload, payload_length);
        free(payload);
        if (wire_relay > 0)
            return rc;
        if (wire_relay < 0) {
            fprintf(stderr,
                    "pgy: failed while writing admitted self-host JSON diagnostic\n");
            return 1;
        }
        fprintf(stderr,
                "pgy: self-host JSON diagnostic receipt is malformed\n");
        return 1;
    }

    free(payload);
    if (rc == PGY_EXEC_CAPTURE_TIMEOUT)
        fprintf(stderr, "pgy: self-host artifact producer timed out\n");
    else if (rc == PGY_EXEC_CAPTURE_OUTPUT_LIMIT)
        fprintf(stderr,
                "pgy: self-host artifact diagnostic exceeded its stdout limit\n");
    else if (rc == PGY_EXEC_CAPTURE_CRASHED)
        fprintf(stderr, "pgy: self-host artifact producer crashed\n");
    else if (rc < 0)
        fprintf(stderr, "pgy: failed to capture self-host artifact producer\n");
    else
        fprintf(stderr,
                "pgy: self-host driver failed (exit %d) without a JSON diagnostic receipt\n",
                rc);
    return rc < 0 ? 1 : rc;
}
