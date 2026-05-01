/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared codegen policy for slot/view type-name classification.
 */

#include <string.h>

#include "codegen_slot_type_policy.h"

static bool
type_name_has_generic_prefix(const char *type_name, const char *prefix)
{
    return type_name != NULL
        && prefix != NULL
        && strncmp(type_name, prefix, strlen(prefix)) == 0;
}

static bool
type_name_is_exact_or_generic(const char *type_name, const char *name,
                              const char *generic_prefix)
{
    return type_name != NULL
        && ((name != NULL && strcmp(type_name, name) == 0)
            || type_name_has_generic_prefix(type_name, generic_prefix));
}

bool
pgy_codegen_type_name_is_slot(const char *type_name)
{
    return type_name_has_generic_prefix(type_name, "Slot<");
}

bool
pgy_codegen_type_name_is_secure_slot(const char *type_name)
{
    return type_name_has_generic_prefix(type_name, "SecureSlot<");
}

bool
pgy_codegen_type_name_is_device_slot(const char *type_name)
{
    return type_name_has_generic_prefix(type_name, "DeviceSlot<");
}

bool
pgy_codegen_type_name_is_read_view(const char *type_name)
{
    return type_name_is_exact_or_generic(type_name, "ReadView", "ReadView<");
}

bool
pgy_codegen_type_name_is_write_view(const char *type_name)
{
    return type_name_is_exact_or_generic(type_name, "WriteView", "WriteView<");
}

bool
pgy_codegen_type_name_is_view(const char *type_name)
{
    return pgy_codegen_type_name_is_read_view(type_name)
        || pgy_codegen_type_name_is_write_view(type_name);
}

bool
pgy_codegen_type_name_is_slot_or_view(const char *type_name)
{
    return pgy_codegen_type_name_is_slot(type_name)
        || pgy_codegen_type_name_is_secure_slot(type_name)
        || pgy_codegen_type_name_is_view(type_name);
}

bool
pgy_codegen_type_name_is_slot_family(const char *type_name)
{
    return pgy_codegen_type_name_is_slot_or_view(type_name)
        || pgy_codegen_type_name_is_device_slot(type_name);
}

bool
pgy_codegen_call_name_is_view_read(const char *name)
{
    return name != NULL && strcmp(name, "ViewRead") == 0;
}

bool
pgy_codegen_call_name_is_view_write(const char *name)
{
    return name != NULL && strcmp(name, "ViewWrite") == 0;
}

bool
pgy_codegen_call_name_is_view_constructor(const char *name)
{
    return pgy_codegen_call_name_is_view_read(name)
        || pgy_codegen_call_name_is_view_write(name);
}
