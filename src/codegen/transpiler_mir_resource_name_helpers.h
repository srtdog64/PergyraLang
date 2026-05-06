#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_NAME_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_NAME_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

const char *transpiler_extract_type_suffix_from_fn(const char *fn_name);

bool transpiler_format_slot_runtime_fn(const char *op_name,
                                       bool is_secure_slot,
                                       bool is_device_slot,
                                       const char *inner_name,
                                       char *out,
                                       size_t out_size);
#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_NAME_HELPERS_H */
