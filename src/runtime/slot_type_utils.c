/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot type-tag and low-level utility helpers.
 */

#include "slot_manager.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    TypeTag tag;
} SlotBuiltinTypeSpec;

static const SlotBuiltinTypeSpec kSlotBuiltinTypes[] = {
    {"Bool", TYPE_BOOL},
    {"Double", TYPE_DOUBLE},
    {"Float", TYPE_FLOAT},
    {"Int", TYPE_INT},
    {"Long", TYPE_LONG},
    {"String", TYPE_STRING},
    {"Vector", TYPE_VECTOR},
};

static int
slot_builtin_type_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const SlotBuiltinTypeSpec *spec = (const SlotBuiltinTypeSpec *)entry;
    return strcmp(name, spec->name);
}

static const SlotBuiltinTypeSpec *
slot_builtin_type_find(const char *typeName)
{
    return (const SlotBuiltinTypeSpec *)bsearch(
        typeName,
        kSlotBuiltinTypes,
        sizeof(kSlotBuiltinTypes) / sizeof(kSlotBuiltinTypes[0]),
        sizeof(kSlotBuiltinTypes[0]),
        slot_builtin_type_compare);
}

uint32_t
TypeTagHash(const char *typeName)
{
    uint32_t hash = 5381u;
    const unsigned char *p;
    const SlotBuiltinTypeSpec *builtin;

    if (typeName == NULL)
        return 0;

    builtin = slot_builtin_type_find(typeName);
    if (builtin != NULL)
        return builtin->tag;

    for (p = (const unsigned char *)typeName; *p != '\0'; p++)
        hash = ((hash << 5) + hash) ^ *p;

    return hash | TYPE_CUSTOM;
}

const char *
TypeTagToString(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:
        return "Int";
    case TYPE_LONG:
        return "Long";
    case TYPE_FLOAT:
        return "Float";
    case TYPE_DOUBLE:
        return "Double";
    case TYPE_STRING:
        return "String";
    case TYPE_BOOL:
        return "Bool";
    case TYPE_VECTOR:
        return "Vector";
    default:
        return "Custom";
    }
}

bool
TypeIsPrimitive(TypeTag tag)
{
    return tag >= TYPE_INT && tag <= TYPE_BOOL;
}

size_t
TypeGetSize(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:
        return sizeof(int32_t);
    case TYPE_LONG:
        return sizeof(int64_t);
    case TYPE_FLOAT:
        return sizeof(float);
    case TYPE_DOUBLE:
        return sizeof(double);
    case TYPE_BOOL:
        return sizeof(bool);
    case TYPE_STRING:
        return 256;
    case TYPE_VECTOR:
        return 1024;
    default:
        return 64;
    }
}

uint32_t
SlotHashFunction(uint32_t slotId)
{
    uint32_t hash = 0x811c9dc5u;
    int i;

    for (i = 0; i < 4; i++) {
        hash ^= (slotId >> (i * 8)) & 0xffu;
        hash *= 0x01000193u;
    }

    return hash;
}

bool
SlotCompareAndSwap(volatile uint32_t *ptr, uint32_t expected, uint32_t newVal)
{
    return __sync_bool_compare_and_swap(ptr, expected, newVal);
}

void
SlotMemoryBarrier(void)
{
    __sync_synchronize();
}
