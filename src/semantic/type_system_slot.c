/*
 * Copyright (c) 2026 Pergyra Language Project
 * Slot/view type construction and accessors.
 */

#include <stdlib.h>
#include <string.h>
#include "type_system.h"

static const char *
type_slot_prefix(bool is_secure, SlotAccessMode access_mode)
{
    if (access_mode == SLOT_ACCESS_READ_VIEW)
        return "ReadView<";
    if (access_mode == SLOT_ACCESS_WRITE_VIEW)
        return "WriteView<";
    if (access_mode == SLOT_ACCESS_MOVE_TOKEN)
        return "MoveToken<";
    if (is_secure)
        return "SecureSlot<";
    return "Slot<";
}

Type *
type_create_slot(Type *inner_type, bool is_secure)
{
    return type_create_slot_access(inner_type, is_secure, SLOT_ACCESS_OWNED);
}

Type *
type_create_slot_access(Type *inner_type,
                        bool is_secure,
                        SlotAccessMode access_mode)
{
    const char *prefix;
    size_t prefix_len;
    size_t inner_len;
    size_t name_len;
    Type *t;

    if (inner_type == NULL || inner_type->name == NULL)
        return NULL;

    t = type_alloc();
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_SLOT;
    prefix = type_slot_prefix(is_secure, access_mode);
    prefix_len = strlen(prefix);
    inner_len = strlen(inner_type->name);
    if (inner_len > ((size_t)-1) - prefix_len - 2) {
        free(t);
        return NULL;
    }

    name_len = prefix_len + inner_len + 2;
    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }

    memcpy(t->name, prefix, prefix_len);
    memcpy(t->name + prefix_len, inner_type->name, inner_len);
    t->name[prefix_len + inner_len] = '>';
    t->name[prefix_len + inner_len + 1] = '\0';

    t->data.slot.inner_type = inner_type;
    t->data.slot.is_secure = is_secure;
    t->data.slot.security_level = 0;
    t->data.slot.access_mode = access_mode;
    return t;
}

Type *
type_create_read_view(Type *inner_type)
{
    return type_create_slot_access(inner_type, false, SLOT_ACCESS_READ_VIEW);
}

Type *
type_create_write_view(Type *inner_type)
{
    return type_create_slot_access(inner_type, false, SLOT_ACCESS_WRITE_VIEW);
}

Type *
type_slot_inner_type(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_SLOT)
        return NULL;
    return type->data.slot.inner_type;
}

bool
type_slot_is_secure(const Type *type)
{
    return type != NULL
        && type->kind == TYPE_KIND_SLOT
        && type->data.slot.is_secure;
}

SlotAccessMode
type_slot_access_mode(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_SLOT)
        return SLOT_ACCESS_OWNED;
    return type->data.slot.access_mode;
}
