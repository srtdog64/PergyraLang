/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party System Runtime Implementation
 */

#include "party_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

/* ============= Shared Local Helpers ============= */

void party_runtime_warn(const char* op, const char* reason);
static char* party_runtime_strdup(const char* text);
static uint64_t HashString(const char* str);
uint64_t GetTimeNanos(void);
static size_t party_context_find_role_index_by_name(const PartyContext* context,
                                                    const char* slotName);
static size_t party_context_find_role_index_by_slot(const PartyContext* context,
                                                    uint32_t slotId);
void* party_context_role_instance_by_slot(PartyContext* context, uint32_t slotId);

static bool
party_runtime_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

void
party_runtime_warn(const char* op, const char* reason)
{
    fprintf(stderr,
            "[pgy][party] %s failed: %s\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown");
}

static char*
party_runtime_strdup(const char* text)
{
    size_t length;
    char* copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }
    length++;
    copy = (char*)malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
}

/* ============= FiberMap Generation ============= */

FiberMap*
GenerateFiberMap(const char* partyType,
                 const PartyRoleBinding* roleBindings,
                 size_t bindingCount)
{
    if (partyType == NULL || roleBindings == NULL) {
        party_runtime_warn("generate_fiber_map", "partyType or roleBindings is null");
        return NULL;
    }

    FiberMap* map = (FiberMap*)calloc(1, sizeof(FiberMap));
    if (map == NULL) {
        party_runtime_warn("generate_fiber_map", "map allocation failed");
        return NULL;
    }

    map->partyTypeName = party_runtime_strdup(partyType);
    if (map->partyTypeName == NULL) {
        free(map);
        party_runtime_warn("generate_fiber_map", "party type allocation failed");
        return NULL;
    }

    if (!party_runtime_array_fits(bindingCount, sizeof(FiberMapEntry))) {
        free((void*)map->partyTypeName);
        free(map);
        party_runtime_warn("generate_fiber_map", "entry allocation size overflow");
        return NULL;
    }

    if (bindingCount > 0) {
        map->entries = (FiberMapEntry*)calloc(bindingCount, sizeof(FiberMapEntry));
    }
    if (bindingCount > 0 && map->entries == NULL) {
        free((void*)map->partyTypeName);
        free(map);
        party_runtime_warn("generate_fiber_map", "entry allocation failed");
        return NULL;
    }

    size_t entryCount = 0;
    for (size_t i = 0; i < bindingCount; i++) {
        const RoleParallelMetadata* metadata = roleBindings[i].metadata;
        if (metadata == NULL || metadata->function == NULL) {
            continue;
        }

        FiberMapEntry* entry = &map->entries[entryCount];
        entry->roleId = party_runtime_strdup(roleBindings[i].slotName != NULL
                                                 ? roleBindings[i].slotName
                                                 : "<anonymous>");
        if (entry->roleId == NULL) {
            FreeFiberMap(map);
            party_runtime_warn("generate_fiber_map", "role id allocation failed");
            return NULL;
        }

        entry->instanceSlotId = roleBindings[i].instanceSlotId;
        entry->parallelFn = metadata->function;
        entry->schedulerTag = metadata->scheduler;
        entry->priority = metadata->priority;
        entry->executionIntervalMs = metadata->intervalMs;
        entry->isContinuous = metadata->continuous;
        entryCount++;
    }

    map->entryCount = entryCount;
    map->cacheKey = HashString(partyType);
    for (size_t i = 0; i < entryCount; i++) {
        map->cacheKey ^= HashString(map->entries[i].roleId);
        map->cacheKey ^= (uint64_t)map->entries[i].schedulerTag << 32;
    }
    map->isStatic = true;

    return map;
}

void
FreeFiberMap(FiberMap* map)
{
    if (map == NULL) {
        return;
    }

    for (size_t i = 0; i < map->entryCount; i++) {
        free((void*)map->entries[i].roleId);
    }

    free((void*)map->partyTypeName);
    free(map->entries);
    free(map);
}

void
PartyContextAttachFiberMap(PartyContext* context, FiberMap* map)
{
    if (context == NULL) {
        party_runtime_warn("party_context.attach_fiber_map", "context is null");
        return;
    }

    pthread_mutex_lock(&context->contextLock);
    context->fiberMap = map;
    pthread_mutex_unlock(&context->contextLock);
}

FiberMap*
PartyContextGetFiberMap(PartyContext* context)
{
    if (context == NULL) {
        party_runtime_warn("party_context.get_fiber_map", "context is null");
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    FiberMap* map = context->fiberMap;
    pthread_mutex_unlock(&context->contextLock);
    return map;
}

/* ============= Context API Implementation ============= */

static size_t
party_context_find_role_index_by_name(const PartyContext* context, const char* slotName)
{
    if (context == NULL || slotName == NULL) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < context->roleCount; i++) {
        if (context->roles[i].slotName != NULL
            && strcmp(context->roles[i].slotName, slotName) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

static size_t
party_context_find_role_index_by_slot(const PartyContext* context, uint32_t slotId)
{
    if (context == NULL) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < context->roleCount; i++) {
        if (context->roles[i].slotId == slotId) {
            return i;
        }
    }

    return SIZE_MAX;
}

void*
party_context_role_instance_by_slot(PartyContext* context, uint32_t slotId)
{
    if (context == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    size_t index = party_context_find_role_index_by_slot(context, slotId);
    void* instance = index != SIZE_MAX ? context->roles[index].roleInstance : NULL;
    pthread_mutex_unlock(&context->contextLock);
    return instance;
}

void*
ContextGetRole(PartyContext* context, const char* slotName, const char* requiredAbility)
{
    if (context == NULL || slotName == NULL) {
        party_runtime_warn("context.get_role", "context or slotName is null");
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    size_t index = party_context_find_role_index_by_name(context, slotName);
    void* result = NULL;

    if (index != SIZE_MAX) {
        bool hasAbility = (requiredAbility == NULL);
        if (requiredAbility != NULL) {
            for (size_t j = 0; j < context->roles[index].abilityCount; j++) {
                if (context->roles[index].abilities[j] != NULL
                    && strcmp(context->roles[index].abilities[j], requiredAbility) == 0) {
                    hasAbility = true;
                    break;
                }
            }
        }

        if (hasAbility) {
            result = context->roles[index].roleInstance;
        }
    }

    pthread_mutex_unlock(&context->contextLock);

    if (result == NULL) {
        party_runtime_warn("context.get_role",
                           requiredAbility != NULL
                               ? "role missing required ability or instance"
                               : "role slot not found or instance unavailable");
    }

    return result;
}

RoleQueryResult
ContextFindRoles(PartyContext* context, const char* requiredAbility)
{
    RoleQueryResult result = {0};
    size_t capacity = 0;

    if (context == NULL || requiredAbility == NULL) {
        party_runtime_warn("context.find_roles", "context or requiredAbility is null");
        return result;
    }

    pthread_mutex_lock(&context->contextLock);

    for (size_t i = 0; i < context->roleCount; i++) {
        bool matched = false;

        if (context->roles[i].roleInstance == NULL)
            continue;

        for (size_t j = 0; j < context->roles[i].abilityCount; j++) {
            if (context->roles[i].abilities[j] != NULL
                && strcmp(context->roles[i].abilities[j], requiredAbility) == 0) {
                matched = true;
                break;
            }
        }

        if (!matched)
            continue;

        if (result.count == capacity) {
            size_t nextCapacity;
            void** nextInstances;
            const char** nextSlotNames;
            if (capacity == 0) {
                nextCapacity = 4U;
            } else {
                if (capacity > SIZE_MAX / 2U) {
                    party_runtime_warn("context.find_roles", "result capacity overflow");
                    break;
                }
                nextCapacity = capacity * 2U;
            }
            if (!party_runtime_array_fits(nextCapacity, sizeof(void*))
                || !party_runtime_array_fits(nextCapacity, sizeof(const char*))) {
                party_runtime_warn("context.find_roles", "result allocation size overflow");
                break;
            }
            nextInstances = (void**)malloc(nextCapacity * sizeof(void*));
            nextSlotNames = (const char**)malloc(nextCapacity * sizeof(const char*));
            if (nextInstances == NULL || nextSlotNames == NULL) {
                free(nextInstances);
                free((void*)nextSlotNames);
                free(result.instances);
                free((void*)result.slotNames);
                result.instances = NULL;
                result.slotNames = NULL;
                result.count = 0;
                party_runtime_warn("context.find_roles", "result allocation failed");
                break;
            }
            if (result.count > 0) {
                memcpy(nextInstances, result.instances, result.count * sizeof(void*));
                memcpy((void*)nextSlotNames,
                       result.slotNames,
                       result.count * sizeof(const char*));
            }
            free(result.instances);
            free((void*)result.slotNames);
            result.instances = nextInstances;
            result.slotNames = nextSlotNames;
            capacity = nextCapacity;
        }

        result.instances[result.count] = context->roles[i].roleInstance;
        result.slotNames[result.count] = context->roles[i].slotName;
        result.count++;
    }

    if (result.count == 0) {
        free(result.instances);
        free((void*)result.slotNames);
        result.instances = NULL;
        result.slotNames = NULL;
    } else if (result.count < capacity) {
        void** trimmedInstances =
            (void**)realloc(result.instances, result.count * sizeof(void*));
        const char** trimmedSlotNames =
            (const char**)realloc((void*)result.slotNames,
                                  result.count * sizeof(const char*));
        if (trimmedInstances != NULL)
            result.instances = trimmedInstances;
        if (trimmedSlotNames != NULL)
            result.slotNames = trimmedSlotNames;
    }

    pthread_mutex_unlock(&context->contextLock);
    return result;
}
void*
ContextGetShared(PartyContext* context, const char* fieldName)
{
    if (context == NULL || fieldName == NULL) {
        party_runtime_warn("context.get_shared", "context or fieldName is null");
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    void* result = NULL;
    for (size_t i = 0; i < context->sharedFieldCount; i++) {
        if (context->sharedFields[i].fieldName != NULL
            && strcmp(context->sharedFields[i].fieldName, fieldName) == 0) {
            result = context->sharedFields[i].value;
            break;
        }
    }
    pthread_mutex_unlock(&context->contextLock);

    if (result == NULL) {
        party_runtime_warn("context.get_shared", "shared field not found");
    }

    return result;
}

/* ============= Helper Functions ============= */

static uint64_t
HashString(const char* str)
{
    if (str == NULL) {
        return 0;
    }

    uint64_t hash = 5381;
    int c = 0;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + (uint64_t)c;
    }
    return hash;
}

uint64_t
GetTimeNanos(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}
