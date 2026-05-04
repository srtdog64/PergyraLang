/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal helpers shared by party runtime owners.
 */

#ifndef PERGYRA_PARTY_RUNTIME_INTERNAL_H
#define PERGYRA_PARTY_RUNTIME_INTERNAL_H

#include "party_runtime.h"

void party_runtime_warn(const char* op, const char* reason);
uint64_t GetTimeNanos(void);
void UpdateFiberStats(const char* roleId, const FiberResult* result);
void* party_context_role_instance_by_slot(PartyContext* context, uint32_t slotId);

#endif /* PERGYRA_PARTY_RUNTIME_INTERNAL_H */
