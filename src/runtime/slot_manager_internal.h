#ifndef PERGYRA_SLOT_MANAGER_INTERNAL_H
#define PERGYRA_SLOT_MANAGER_INTERNAL_H

#include "slot_manager.h"

#include <pthread.h>

pthread_mutex_t *manager_mutex(SlotManager *manager);
void slot_manager_record_security_violation(SlotManager *manager,
                                            const char *event,
                                            uint32_t slotId,
                                            const char *details);
uint64_t slot_now_us(void);
uint32_t current_thread_id(void);
uint32_t slot_checksum_bytes(const void *ptr, size_t size);
SlotEntry *find_slot_entry_locked(SlotManager *manager,
                                  const SlotHandle *handle);
void slot_free_plain_buffer(SlotEntry *entry);
void slot_free_buffers(SlotEntry *entry);
bool slot_reserve_storage(SlotEntry *entry, size_t size);
bool slot_store_plain_payload(SlotEntry *entry, const void *data, size_t size);
bool slot_is_expired_locked(const SlotEntry *entry);
SlotError slot_release_entry_locked(SlotManager *manager, SlotEntry *entry,
                                    bool allowSecure);

#endif /* PERGYRA_SLOT_MANAGER_INTERNAL_H */
