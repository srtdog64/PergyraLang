/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer builtin vocabulary.
 */

#include <stdlib.h>
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

static int
slot_builtin_fact_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const SlotBuiltinFact *fact = (const SlotBuiltinFact *)entry;

    return strcmp(name, fact->name);
}

static const SlotBuiltinFact *
slot_builtin_lookup(const char *name)
{
    const SlotBuiltinFact *match;

    if (name == NULL)
        return NULL;

    match = (const SlotBuiltinFact *)bsearch(
        &name, slot_builtin_facts,
        sizeof(slot_builtin_facts) / sizeof(slot_builtin_facts[0]),
        sizeof(slot_builtin_facts[0]), slot_builtin_fact_compare);
    return match;
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
