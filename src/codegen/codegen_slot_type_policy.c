/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared codegen policy for slot/view type-name classification.
 */

#include <string.h>
#include <stdlib.h>

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

bool
pgy_codegen_call_name_is_read(const char *name)
{
    return name != NULL && strcmp(name, "Read") == 0;
}

bool
pgy_codegen_call_name_is_write(const char *name)
{
    return name != NULL && strcmp(name, "Write") == 0;
}

bool
pgy_codegen_call_name_is_release(const char *name)
{
    return name != NULL && strcmp(name, "Release") == 0;
}

bool
pgy_codegen_call_name_is_slot_operation(const char *name)
{
    return pgy_codegen_call_name_is_read(name)
        || pgy_codegen_call_name_is_write(name)
        || pgy_codegen_call_name_is_release(name);
}

bool
pgy_codegen_call_name_is_move(const char *name)
{
    return name != NULL && strcmp(name, "Move") == 0;
}

bool
pgy_codegen_call_name_is_slot_source(const char *name)
{
    return pgy_codegen_call_name_is_view_constructor(name)
        || pgy_codegen_call_name_is_move(name);
}

typedef enum PgyCodegenClaimSlotKind {
    PGY_CODEGEN_CLAIM_SLOT,
    PGY_CODEGEN_CLAIM_SECURE_SLOT,
    PGY_CODEGEN_CLAIM_DEVICE_SLOT,
} PgyCodegenClaimSlotKind;

typedef struct PgyCodegenClaimSlotSpec {
    const char *name;
    const char *abi_prefix;
    PgyCodegenClaimSlotKind kind;
} PgyCodegenClaimSlotSpec;

static int
pgy_codegen_claim_slot_spec_compare(const void *key, const void *entry)
{
    return strcmp((const char *)key,
                  ((const PgyCodegenClaimSlotSpec *)entry)->name);
}

static const PgyCodegenClaimSlotSpec *
pgy_codegen_claim_slot_spec(const char *name)
{
    static const PgyCodegenClaimSlotSpec specs[] = {
        {"ClaimDeviceSlot", "DeviceSlot", PGY_CODEGEN_CLAIM_DEVICE_SLOT},
        {"ClaimSecureSlot", "SecureSlot", PGY_CODEGEN_CLAIM_SECURE_SLOT},
        {"ClaimSlot", "Slot", PGY_CODEGEN_CLAIM_SLOT},
    };

    if (name == NULL)
        return NULL;
    return bsearch(name,
                   specs,
                   sizeof(specs) / sizeof(specs[0]),
                   sizeof(specs[0]),
                   pgy_codegen_claim_slot_spec_compare);
}

bool
pgy_codegen_call_name_is_claim_slot(const char *name)
{
    const PgyCodegenClaimSlotSpec *spec = pgy_codegen_claim_slot_spec(name);
    return spec != NULL && spec->kind == PGY_CODEGEN_CLAIM_SLOT;
}

bool
pgy_codegen_call_name_is_claim_secure_slot(const char *name)
{
    const PgyCodegenClaimSlotSpec *spec = pgy_codegen_claim_slot_spec(name);
    return spec != NULL && spec->kind == PGY_CODEGEN_CLAIM_SECURE_SLOT;
}

bool
pgy_codegen_call_name_is_claim_device_slot(const char *name)
{
    const PgyCodegenClaimSlotSpec *spec = pgy_codegen_claim_slot_spec(name);
    return spec != NULL && spec->kind == PGY_CODEGEN_CLAIM_DEVICE_SLOT;
}

bool
pgy_codegen_call_name_is_slot_claim(const char *name)
{
    return pgy_codegen_claim_slot_spec(name) != NULL;
}

const char *
pgy_codegen_claim_slot_abi_prefix(const char *name)
{
    const PgyCodegenClaimSlotSpec *spec = pgy_codegen_claim_slot_spec(name);
    return spec != NULL ? spec->abi_prefix : NULL;
}
