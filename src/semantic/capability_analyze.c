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

#include "runtime/pgy_runtime_capability.h" /* PGY_CAP_* bits */

typedef struct {
    uint32_t    bit;
    const char *name;
} CapBitName;

static const CapBitName k_cap_names[] = {
    {PGY_CAP_IO_READ,  "IO_READ"},
    {PGY_CAP_IO_WRITE, "IO_WRITE"},
    {PGY_CAP_NETWORK,  "NETWORK"},
    {PGY_CAP_CLOCK,    "CLOCK"},
    {PGY_CAP_RANDOM,   "RANDOM"},
    {PGY_CAP_ENV,      "ENV"},
    {PGY_CAP_RENDER,   "RENDER"},
    {PGY_CAP_AUDIO,    "AUDIO"},
    {PGY_CAP_INPUT,    "INPUT"},
};

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
    static const struct { const char *name; uint32_t cap; } k_builtin_caps[] = {
        {"Random",     PGY_CAP_RANDOM},
        {"SeedRandom", PGY_CAP_RANDOM},
        {"Now",        PGY_CAP_CLOCK},
        {"ReadFile",   PGY_CAP_IO_READ},
        {"ReadStdin",  PGY_CAP_IO_READ},
        {"Input",      PGY_CAP_IO_READ},
        {"DirWalk",    PGY_CAP_IO_READ},
        {"WriteFile",  PGY_CAP_IO_WRITE},
        {"Args",       PGY_CAP_ENV},
    };
    if (name == NULL)
        return PGY_CAP_NONE;
    for (size_t i = 0; i < sizeof(k_builtin_caps) / sizeof(k_builtin_caps[0]); i++) {
        if (strcmp(name, k_builtin_caps[i].name) == 0)
            return k_builtin_caps[i].cap;
    }
    return PGY_CAP_NONE;
}

/* The capability name for a single PGY_CAP_* bit, or NULL. */
const char *
capability_bit_name(uint32_t bit)
{
    for (size_t i = 0; i < sizeof(k_cap_names) / sizeof(k_cap_names[0]); i++) {
        if (k_cap_names[i].bit == bit)
            return k_cap_names[i].name;
    }
    return NULL;
}

void
capability_used_names_print_json(uint32_t used_mask, FILE *out)
{
    int first = 1;

    if (out == NULL)
        return;
    fputs("[", out);
    for (size_t i = 0; i < sizeof(k_cap_names) / sizeof(k_cap_names[0]); i++) {
        if ((used_mask & k_cap_names[i].bit) == 0)
            continue;
        if (!first)
            fputs(", ", out);
        fprintf(out, "\"%s\"", k_cap_names[i].name);
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
