/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared channel runtime payload ABI inventory for C and LLVM backends.
 */

#include "codegen_channel_runtime_abi.h"

#include <string.h>

static const PgyChannelRuntimePayloadAbi k_payload_abi[] = {
    { PGY_CHANNEL_RUNTIME_PAYLOAD_INT, "Int", "Int" },
    { PGY_CHANNEL_RUNTIME_PAYLOAD_STRING, "String", "String" },
};

size_t
pgy_channel_runtime_payload_abi_count(void)
{
    return sizeof(k_payload_abi) / sizeof(k_payload_abi[0]);
}

const PgyChannelRuntimePayloadAbi *
pgy_channel_runtime_payload_abi_at(size_t index)
{
    if (index >= pgy_channel_runtime_payload_abi_count())
        return NULL;
    return &k_payload_abi[index];
}

const PgyChannelRuntimePayloadAbi *
pgy_channel_runtime_payload_abi_lookup(const char *type_name)
{
    size_t count = pgy_channel_runtime_payload_abi_count();

    if (type_name == NULL)
        return NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(type_name, k_payload_abi[i].type_name) == 0)
            return &k_payload_abi[i];
    }
    return NULL;
}

bool
pgy_channel_runtime_payload_has_abi(const char *type_name)
{
    return pgy_channel_runtime_payload_abi_lookup(type_name) != NULL;
}

const char *
pgy_channel_runtime_payload_supported_list(void)
{
    return "Channel<Int> and Channel<String>";
}
