#ifndef PGY_DRIVER_DIAG_H
#define PGY_DRIVER_DIAG_H

#include <stdbool.h>
#include "air.h"
#include "driver_app.h"

void driver_emit_stage_fail(const DriverFlags *flags,
                            const char *stage,
                            const char *description,
                            const char *detail);
void driver_emit_air_drift_fail(const DriverFlags *flags, const AIRProgram *air);
const char *driver_diag_code_from_message(const char *message);
const char *driver_diag_cause_from_code(const char *code);
const char *driver_diag_fix_from_code(const char *code);
bool driver_diag_compatibility_manifest_validate_file(const char *path,
                                                       char **error_message);

#endif /* PGY_DRIVER_DIAG_H */
