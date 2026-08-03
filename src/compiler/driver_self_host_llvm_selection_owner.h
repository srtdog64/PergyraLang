#ifndef PGY_DRIVER_SELF_HOST_LLVM_SELECTION_OWNER_H
#define PGY_DRIVER_SELF_HOST_LLVM_SELECTION_OWNER_H

#include <stdbool.h>

#include "driver_app.h"

bool driver_self_host_llvm_artifact_request_supported(
    const DriverFlags *flags);
bool driver_self_host_llvm_ir_file_request_supported(
    const DriverFlags *flags);
bool driver_self_host_llvm_ir_stdout_request_supported(
    const DriverFlags *flags);

#endif /* PGY_DRIVER_SELF_HOST_LLVM_SELECTION_OWNER_H */
