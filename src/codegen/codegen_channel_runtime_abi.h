#ifndef PGY_CODEGEN_CHANNEL_RUNTIME_ABI_H
#define PGY_CODEGEN_CHANNEL_RUNTIME_ABI_H

#include <stdbool.h>
#include <stddef.h>

typedef enum PgyChannelRuntimePayloadKind {
    PGY_CHANNEL_RUNTIME_PAYLOAD_INVALID = 0,
    PGY_CHANNEL_RUNTIME_PAYLOAD_INT,
    PGY_CHANNEL_RUNTIME_PAYLOAD_STRING,
} PgyChannelRuntimePayloadKind;

typedef struct PgyChannelRuntimePayloadAbi {
    PgyChannelRuntimePayloadKind kind;
    const char *type_name;
    const char *suffix;
} PgyChannelRuntimePayloadAbi;

size_t pgy_channel_runtime_payload_abi_count(void);
const PgyChannelRuntimePayloadAbi *
pgy_channel_runtime_payload_abi_at(size_t index);
const PgyChannelRuntimePayloadAbi *
pgy_channel_runtime_payload_abi_lookup(const char *type_name);
bool pgy_channel_runtime_payload_has_abi(const char *type_name);
const char *pgy_channel_runtime_payload_supported_list(void);
bool pgy_channel_runtime_name(char *out,
                              size_t out_size,
                              const char *op,
                              const char *suffix);
bool pgy_lane_channel_runtime_name(char *out,
                                   size_t out_size,
                                   const char *op,
                                   const char *suffix);

#endif /* PGY_CODEGEN_CHANNEL_RUNTIME_ABI_H */
