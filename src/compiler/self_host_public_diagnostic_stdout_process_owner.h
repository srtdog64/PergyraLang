#ifndef PGY_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_PROCESS_OWNER_H
#define PGY_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_PROCESS_OWNER_H

#include <stdbool.h>

typedef enum {
    DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_AST = 0,
    DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_MIR = 1
} DriverSelfHostPublicDiagnosticStdoutKind;

int driver_run_self_host_public_diagnostic_stdout_process(
    const char *const argv[],
    DriverSelfHostPublicDiagnosticStdoutKind kind,
    bool emit_json_diagnostic);

#endif /* PGY_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_PROCESS_OWNER_H */
