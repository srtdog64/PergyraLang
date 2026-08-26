#ifndef PGY_DRIVER_SELF_HOST_SELECTION_OWNER_H
#define PGY_DRIVER_SELF_HOST_SELECTION_OWNER_H

#include <stdbool.h>

#include "driver_app.h"

bool driver_plain_c_binary_target_requested(const DriverFlags *flags);
bool driver_plain_llvm_binary_target_requested(const DriverFlags *flags);
bool driver_self_host_machine_manifest_request_supported(const DriverFlags *flags);
const char *driver_self_host_source_stdout_mode(const DriverFlags *flags);
const char *driver_self_host_unowned_ir_option(const DriverFlags *flags);
bool driver_self_host_mir_json_request_supported(const DriverFlags *flags);
bool driver_self_host_c_artifact_request_supported(const DriverFlags *flags);

#endif /* PGY_DRIVER_SELF_HOST_SELECTION_OWNER_H */
