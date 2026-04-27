/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot type-tag and low-level utility helpers.
 */

#include "slot_manager.h"

#include <stdint.h>
#include <string.h>

uint32_t
TypeTagHash(const char *typeName)
{
    uint32_t hash = 5381u;
    const unsigned char *p;

    if (typeName == NULL)
        return 0;

    if (strcmp(typeName, "Int") == 0)
        return TYPE_INT;
    if (strcmp(typeName, "Long") == 0)
        return TYPE_LONG;
    if (strcmp(typeName, "Float") == 0)
        return TYPE_FLOAT;
    if (strcmp(typeName, "Double") == 0)
        return TYPE_DOUBLE;
    if (strcmp(typeName, "String") == 0)
        return TYPE_STRING;
    if (strcmp(typeName, "Bool") == 0)
        return TYPE_BOOL;
    if (strcmp(typeName, "Vector") == 0)
        return TYPE_VECTOR;

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
