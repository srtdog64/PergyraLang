/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer builtin vocabulary.
 */

#include <string.h>

#include "slot_analyzer_internal.h"

typedef struct
{
    const char *name;
    unsigned    access_mask;
    bool        local_non_escape;
} SlotBuiltinFact;

static const SlotBuiltinFact slot_builtin_facts[] = {
    {"Move", SLOT_ACCESS_WRITE, false},
    {"Read", SLOT_ACCESS_READ, true},
    {"ReadView", 0, true},   /* legacy/parser-adjacent spelling */
    {"Release", SLOT_ACCESS_RELEASE, true},
    {"ViewRead", SLOT_ACCESS_READ, true},
    {"ViewWrite", SLOT_ACCESS_WRITE, true},
    {"Write", SLOT_ACCESS_WRITE, true},
    {"WriteView", 0, true},  /* legacy/parser-adjacent spelling */
};

static const SlotBuiltinFact *
slot_builtin_lookup(const char *name)
{
    if (name == NULL)
        return NULL;

    for (size_t i = 0;
         i < sizeof(slot_builtin_facts) / sizeof(slot_builtin_facts[0]);
         i++) {
        if (strcmp(slot_builtin_facts[i].name, name) == 0)
            return &slot_builtin_facts[i];
    }
    return NULL;
}

unsigned
slot_builtin_access_mask(const char *name)
{
    const SlotBuiltinFact *fact = slot_builtin_lookup(name);
    return fact != NULL ? fact->access_mask : 0;
}

bool
slot_builtin_call_is_local_non_escape(const char *name)
{
    const SlotBuiltinFact *fact = slot_builtin_lookup(name);
    return fact != NULL && fact->local_non_escape;
}
