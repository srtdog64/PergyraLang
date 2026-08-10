/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Capability manifest presentation. The inference/propagation/check live in the
 * type checker (see capability_analyze.h); this is only the JSON rendering of a
 * capability mask for `pgy --capability-manifest`.
 */
#include "capability_analyze.h"

#include <string.h>

#include "../common/string_compat.h"
#include "callable_contract_vocabulary.h"
#include "runtime/pgy_runtime_capability.h" /* PGY_CAP_* bits */

typedef enum PgyBuiltinCapabilityPolicy {
    PGY_BUILTIN_CAPABILITY_FIXED = 1,
    PGY_BUILTIN_CAPABILITY_FILE_MODE = 2
} PgyBuiltinCapabilityPolicy;

typedef struct PgyBuiltinCapabilitySpec {
    size_t stable_id;
    const char *name;
    PgyCallableContractWordId primary_id;
    PgyCallableContractWordId secondary_id;
    PgyBuiltinCapabilityPolicy policy;
} PgyBuiltinCapabilitySpec;

#define PGY_BUILTIN_CAPABILITY(identity, stable_id_value, source_name,        \
                               primary_identity, secondary_identity,          \
                               policy_value)                                  \
    { stable_id_value, source_name,                                            \
      PGY_CALLABLE_CONTRACT_WORD_##primary_identity,                           \
      PGY_CALLABLE_CONTRACT_WORD_##secondary_identity, policy_value },
static const PgyBuiltinCapabilitySpec k_builtin_caps[] = {
#include "builtin_capability_registry.def"
};
#undef PGY_BUILTIN_CAPABILITY

static uint32_t
capability_mask_for_identity(PgyCallableContractWordId id)
{
    const PgyCallableContractWordSpec *spec =
        pgy_callable_contract_vocabulary_find_id(id);
    return spec != NULL &&
           spec->axis == PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY
        ? spec->mask
        : PGY_CAP_NONE;
}

/* Emit the used-capability names as a bare JSON array: ["IO_READ", "RANDOM"].
   These names are the program's external effect families; AIR reuses this so the
   effect inventory and the capability mask are owned in one place. */
/* The gated ambient builtins and the capability each requires -- the same
   name->cap keying the type checker records and the runtime gate enforces. AIR
   uses this to bind per-operation effect sites to their capability. Returns
   PGY_CAP_NONE for a non-gated name. */
uint32_t
capability_for_builtin(const char *name)
{
    if (name == NULL)
        return PGY_CAP_NONE;
    for (size_t i = 0; i < sizeof(k_builtin_caps) / sizeof(k_builtin_caps[0]); i++) {
        if (strcmp(name, k_builtin_caps[i].name) == 0) {
            if (k_builtin_caps[i].policy != PGY_BUILTIN_CAPABILITY_FIXED)
                return PGY_CAP_NONE;
            return capability_mask_for_identity(k_builtin_caps[i].primary_id);
        }
    }
    return PGY_CAP_NONE;
}

bool
capability_builtin_registry_ready(void)
{
    const size_t count = sizeof(k_builtin_caps) / sizeof(k_builtin_caps[0]);
    size_t file_mode_count = 0;

    if (count != 17 || !pgy_callable_contract_vocabulary_ready())
        return false;
    for (size_t i = 0; i < count; i++) {
        const PgyBuiltinCapabilitySpec *row = &k_builtin_caps[i];
        uint32_t primary = capability_mask_for_identity(row->primary_id);
        uint32_t secondary = capability_mask_for_identity(row->secondary_id);
        if (row->stable_id != i || row->name == NULL || row->name[0] == '\0' ||
            primary == PGY_CAP_NONE || secondary == PGY_CAP_NONE ||
            (i > 0 && strcmp(k_builtin_caps[i - 1].name, row->name) >= 0))
            return false;
        if (row->policy == PGY_BUILTIN_CAPABILITY_FIXED) {
            if (primary != secondary || capability_for_builtin(row->name) != primary)
                return false;
        } else if (row->policy == PGY_BUILTIN_CAPABILITY_FILE_MODE) {
            file_mode_count++;
            if (strcmp(row->name, "FileOpen") != 0 || primary == secondary ||
                capability_for_builtin(row->name) != PGY_CAP_NONE)
                return false;
        } else {
            return false;
        }
    }
    return file_mode_count == 1;
}

/* The capability name for a single PGY_CAP_* bit, or NULL. */
const char *
capability_bit_name(uint32_t bit)
{
    for (size_t i = 0; i < pgy_callable_contract_vocabulary_axis_count(
             PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY); i++) {
        const PgyCallableContractWordSpec *spec =
            pgy_callable_contract_vocabulary_at_rank(
                PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, i);
        if (spec != NULL && spec->mask == bit)
            return spec->external_name;
    }
    return NULL;
}

void
capability_mask_to_diagnostic_string(uint32_t mask,
                                     char *buf,
                                     size_t buf_size)
{
    size_t off = 0;

    if (buf == NULL || buf_size == 0)
        return;
    buf[0] = '\0';
    if (mask == 0u) {
        snprintf(buf, buf_size, "none");
        return;
    }
    for (size_t i = 0; i < pgy_callable_contract_vocabulary_axis_count(
             PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY); i++) {
        const PgyCallableContractWordSpec *spec =
            pgy_callable_contract_vocabulary_at_rank(
                PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, i);
        if (spec == NULL || (mask & spec->mask) == 0)
            continue;
        off = pergyra_str_appendf(buf, buf_size, "%s%s",
                                  off > 0 ? ", " : "",
                                  spec->spelling);
    }
}

void
capability_used_names_print_json(uint32_t used_mask, FILE *out)
{
    int first = 1;

    if (out == NULL)
        return;
    fputs("[", out);
    for (size_t i = 0; i < pgy_callable_contract_vocabulary_axis_count(
             PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY); i++) {
        const PgyCallableContractWordSpec *spec =
            pgy_callable_contract_vocabulary_at_rank(
                PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, i);
        if (spec == NULL || (used_mask & spec->mask) == 0)
            continue;
        if (!first)
            fputs(", ", out);
        fprintf(out, "\"%s\"", spec->external_name);
        first = 0;
    }
    fputs("]", out);
}

void
capability_manifest_print(uint32_t used_mask, FILE *out)
{
    if (out == NULL)
        return;

    fputs("{\n  \"pgy.capability.manifest.v1\": {\n", out);
    fputs("    \"used\": ", out);
    capability_used_names_print_json(used_mask, out);
    fprintf(out, ",\n    \"used_mask\": \"0x%x\"\n  }\n}\n", used_mask);
}
