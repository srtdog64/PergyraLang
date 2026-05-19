#ifndef PGY_RUNTIME_LIB_INTENT_ACTIVE_INDEX_EXPORTS_H
#define PGY_RUNTIME_LIB_INTENT_ACTIVE_INDEX_EXPORTS_H

#if (PGY_INTENT_ACTIVE_INDEX_MAX & (PGY_INTENT_ACTIVE_INDEX_MAX - 1)) != 0
#error "PGY_INTENT_ACTIVE_INDEX_MAX must stay a power of two"
#endif

static uint32_t
pgy_intent_handle_hash_export(int32_t handle)
{
    uint32_t value = (uint32_t)handle;
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    return value;
}

static int32_t
pgy_intent_active_index_find_slot_export(int32_t handle)
{
    uint32_t base;

    if (handle <= 0)
        return -1;
    base = pgy_intent_handle_hash_export(handle) & (PGY_INTENT_ACTIVE_INDEX_MAX - 1);
    for (int32_t probe = 0; probe < PGY_INTENT_ACTIVE_INDEX_MAX; probe++) {
        int32_t slot = (int32_t)((base + (uint32_t)probe)
            & (PGY_INTENT_ACTIVE_INDEX_MAX - 1));
        int32_t indexed_handle = pgy_intent_active_index_handles[slot];
        if (indexed_handle == 0)
            return -1;
        if (indexed_handle == handle)
            return slot;
    }
    return -1;
}

static void
pgy_intent_active_index_set_export(int32_t handle, int32_t active_slot)
{
    uint32_t base;
    int32_t first_tombstone = -1;

    if (handle <= 0 || active_slot < 0 || active_slot >= PGY_INTENT_ACTIVE_MAX)
        return;
    base = pgy_intent_handle_hash_export(handle) & (PGY_INTENT_ACTIVE_INDEX_MAX - 1);
    for (int32_t probe = 0; probe < PGY_INTENT_ACTIVE_INDEX_MAX; probe++) {
        int32_t slot = (int32_t)((base + (uint32_t)probe)
            & (PGY_INTENT_ACTIVE_INDEX_MAX - 1));
        int32_t indexed_handle = pgy_intent_active_index_handles[slot];
        if (indexed_handle == PGY_INTENT_ACTIVE_INDEX_TOMBSTONE) {
            if (first_tombstone < 0)
                first_tombstone = slot;
            continue;
        }
        if (indexed_handle == 0) {
            if (first_tombstone >= 0)
                slot = first_tombstone;
            pgy_intent_active_index_handles[slot] = handle;
            pgy_intent_active_index_slots[slot] = active_slot;
            return;
        }
        if (indexed_handle == handle) {
            pgy_intent_active_index_handles[slot] = handle;
            pgy_intent_active_index_slots[slot] = active_slot;
            return;
        }
    }
    if (first_tombstone >= 0) {
        pgy_intent_active_index_handles[first_tombstone] = handle;
        pgy_intent_active_index_slots[first_tombstone] = active_slot;
    }
}

static void
pgy_intent_active_index_clear_export(int32_t handle)
{
    int32_t slot = pgy_intent_active_index_find_slot_export(handle);
    if (slot < 0)
        return;
    pgy_intent_active_index_handles[slot] =
        PGY_INTENT_ACTIVE_INDEX_TOMBSTONE;
    pgy_intent_active_index_slots[slot] = 0;
}

static int32_t
pgy_intent_find_active_registry_slot_export(int32_t handle)
{
    int32_t slot = pgy_intent_active_index_find_slot_export(handle);

    if (slot >= 0) {
        int32_t active_slot = pgy_intent_active_index_slots[slot];
        if (active_slot >= 0 && active_slot < PGY_INTENT_ACTIVE_MAX) {
            PgyIntentActiveEntry *entry =
                &pgy_intent_active_registry[active_slot];
            if (entry->active && entry->handle == handle)
                return active_slot;
        }
    }

    for (int32_t i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active
            && pgy_intent_active_registry[i].handle == handle) {
            pgy_intent_active_index_set_export(handle, i);
            return i;
        }
    }
    return -1;
}

static PgyIntentActiveEntry *
pgy_intent_find_active_entry_export(int32_t handle)
{
    int32_t active_slot = pgy_intent_find_active_registry_slot_export(handle);

    if (active_slot < 0)
        return NULL;
    return &pgy_intent_active_registry[active_slot];
}

#endif /* PGY_RUNTIME_LIB_INTENT_ACTIVE_INDEX_EXPORTS_H */
