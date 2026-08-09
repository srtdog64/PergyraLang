#ifndef PGY_DRIVER_SELF_HOST_SELECTION_OWNER_H
#define PGY_DRIVER_SELF_HOST_SELECTION_OWNER_H

#include <stdbool.h>

#include "driver_app.h"

bool driver_plain_c_binary_target_requested(const DriverFlags *flags);
bool driver_plain_llvm_binary_target_requested(const DriverFlags *flags);
bool driver_self_host_tokens_request_supported(const DriverFlags *flags);
bool driver_self_host_mir_json_request_supported(const DriverFlags *flags);
bool driver_self_host_c_artifact_request_supported(const DriverFlags *flags);

#endif /* PGY_DRIVER_SELF_HOST_SELECTION_OWNER_H */
