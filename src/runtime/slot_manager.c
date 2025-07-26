#include "slot_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

// 외부 어셈블리 함수 선언
extern uint32_t slot_hash_function_asm(uint32_t slot_id);
extern bool slot_compare_and_swap_asm(volatile uint32_t* ptr, uint32_t expected, uint32_t new_val);
extern void slot_memory_barrier_asm(void);

// 메모리 풀 (간단한 구현)
typedef struct {
    void* pool_start;
    size_t pool_size;
    size_t block_size;
    size_t total_blocks;
    bool* free_blocks;
    pthread_mutex_t mutex;
} MemoryPool;

// 슬롯 매니저 생성
SlotManager* slot_manager_create(size_t max_slots, size_t memory_pool_size) {
    SlotManager* manager = malloc(sizeof(SlotManager));
    if (!manager) return NULL;
    
    // 슬롯 테이블 할당
    manager->slot_table = calloc(max_slots, sizeof(SlotEntry));
    if (!manager->slot_table) {
        free(manager);
        return NULL;
    }
    
    manager->table_size = max_slots;
    manager->max_slots = max_slots;
    manager->next_slot_id = 1; // 0은 무효한 ID로 예약
    
    // 메모리 풀 생성
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) {
        free(manager->slot_table);
        free(manager);
        return NULL;
    }
    
    pool->pool_size = memory_pool_size;
    pool->block_size = 64; // 64바이트 블록
    pool->total_blocks = memory_pool_size / pool->block_size;
    pool->pool_start = aligned_alloc(64, memory_pool_size); // 64바이트 정렬
    pool->free_blocks = calloc(pool->total_blocks, sizeof(bool));
    
    if (!pool->pool_start || !pool->free_blocks) {
        if (pool->pool_start) free(pool->pool_start);
        if (pool->free_blocks) free(pool->free_blocks);
        free(pool);
        free(manager->slot_table);
        free(manager);
        return NULL;
    }
    
    pthread_mutex_init(&pool->mutex, NULL);
    manager->memory_pool = pool;
    
    // 뮤텍스 초기화
    manager->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init((pthread_mutex_t*)manager->mutex, NULL);
    
    // 통계 초기화
    manager->total_allocations = 0;
    manager->total_deallocations = 0;
    manager->active_slots = 0;
    manager->cache_hits = 0;
    manager->cache_misses = 0;
    
    return manager;
}

// 슬롯 매니저 해제
void slot_manager_destroy(SlotManager* manager) {
    if (!manager) return;
    
    // 메모리 풀 해제
    if (manager->memory_pool) {
        MemoryPool* pool = (MemoryPool*)manager->memory_pool;
        pthread_mutex_destroy(&pool->mutex);
        if (pool->pool_start) free(pool->pool_start);
        if (pool->free_blocks) free(pool->free_blocks);
        free(pool);
    }
    
    // 뮤텍스 해제
    if (manager->mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)manager->mutex);
        free(manager->mutex);
    }
    
    // 슬롯 테이블 해제
    if (manager->slot_table) {
        free(manager->slot_table);
    }
    
    free(manager);
}

// 메모리 블록 할당 (내부 함수)
static void* allocate_memory_block(SlotManager* manager, size_t size) {
    if (!manager || !manager->memory_pool) return NULL;
    
    MemoryPool* pool = (MemoryPool*)manager->memory_pool;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 필요한 블록 수 계산
    size_t blocks_needed = (size + pool->block_size - 1) / pool->block_size;
    
    // 연속된 빈 블록 찾기
    for (size_t i = 0; i <= pool->total_blocks - blocks_needed; i++) {
        bool found = true;
        for (size_t j = 0; j < blocks_needed; j++) {
            if (pool->free_blocks[i + j]) {
                found = false;
                break;
            }
        }
        
        if (found) {
            // 블록들을 사용중으로 마킹
            for (size_t j = 0; j < blocks_needed; j++) {
                pool->free_blocks[i + j] = true;
            }
            
            void* result = (char*)pool->pool_start + (i * pool->block_size);
            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return NULL; // 할당 실패
}

// 메모리 블록 해제 (내부 함수)
static void deallocate_memory_block(SlotManager* manager, void* ptr, size_t size) {
    if (!manager || !manager->memory_pool || !ptr) return;
    
    MemoryPool* pool = (MemoryPool*)manager->memory_pool;
    
    pthread_mutex_lock(&pool->mutex);
    
    // 블록 인덱스 계산
    size_t block_index = ((char*)ptr - (char*)pool->pool_start) / pool->block_size;
    size_t blocks_to_free = (size + pool->block_size - 1) / pool->block_size;
    
    // 블록들을 빈 상태로 마킹
    for (size_t i = 0; i < blocks_to_free; i++) {
        if (block_index + i < pool->total_blocks) {
            pool->free_blocks[block_index + i] = false;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

// 슬롯 클레임 (C 버전, 일반적인 경우)
SlotError slot_claim(SlotManager* manager, TypeTag type, SlotHandle* handle) {
    if (!manager || !handle) return SLOT_ERROR_INVALID_HANDLE;
    
    // 어셈블리 최적화 버전 사용 가능한 경우
    if (type <= TYPE_CUSTOM) {
        return slot_claim_fast(manager, type, handle);
    }
    
    pthread_mutex_lock((pthread_mutex_t*)manager->mutex);
    
    // 빈 슬롯 찾기
    for (size_t i = 0; i < manager->table_size; i++) {
        SlotEntry* entry = &manager->slot_table[i];
        
        if (!entry->occupied) {
            // 슬롯 초기화
            entry->slot_id = manager->next_slot_id++;
            entry->type_tag = type;
            entry->occupied = true;
            entry->data_block_ref = NULL; // 나중에 할당
            entry->ttl = 0; // 무제한
            entry->thread_affinity = 0; // 현재 스레드
            entry->allocation_time = time(NULL);
            
            // 핸들 설정
            handle->slot_id = entry->slot_id;
            handle->type_tag = type;
            handle->generation = 1;
            
            // 통계 업데이트
            manager->total_allocations++;
            manager->active_slots++;
            
            pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
            return SLOT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
    return SLOT_ERROR_OUT_OF_MEMORY;
}

// 슬롯 쓰기
SlotError slot_write(SlotManager* manager, const SlotHandle* handle, 
                     const void* data, size_t data_size) {
    if (!manager || !handle || !data) return SLOT_ERROR_INVALID_HANDLE;
    
    // 간단한 타입에 대해서는 어셈블리 버전 사용
    if (data_size <= 256 && handle->type_tag <= TYPE_CUSTOM) {
        return slot_write_fast(manager, handle, data, data_size);
    }
    
    pthread_mutex_lock((pthread_mutex_t*)manager->mutex);
    
    // 슬롯 찾기
    SlotEntry* entry = NULL;
    for (size_t i = 0; i < manager->table_size; i++) {
        if (manager->slot_table[i].slot_id == handle->slot_id &&
            manager->slot_table[i].occupied) {
            entry = &manager->slot_table[i];
            break;
        }
    }
    
    if (!entry) {
        pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    // 타입 검증
    if (entry->type_tag != handle->type_tag) {
        pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
        return SLOT_ERROR_TYPE_MISMATCH;
    }
    
    // 메모리 블록 할당 (필요한 경우)
    if (!entry->data_block_ref) {
        entry->data_block_ref = allocate_memory_block(manager, data_size);
        if (!entry->data_block_ref) {
            pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
            return SLOT_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // 데이터 복사
    memcpy(entry->data_block_ref, data, data_size);
    
    pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
    return SLOT_SUCCESS;
}

// 슬롯 읽기
SlotError slot_read(SlotManager* manager, const SlotHandle* handle, 
                    void* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!manager || !handle || !buffer) return SLOT_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock((pthread_mutex_t*)manager->mutex);
    
    // 슬롯 찾기
    SlotEntry* entry = NULL;
    for (size_t i = 0; i < manager->table_size; i++) {
        if (manager->slot_table[i].slot_id == handle->slot_id &&
            manager->slot_table[i].occupied) {
            entry = &manager->slot_table[i];
            break;
        }
    }
    
    if (!entry) {
        manager->cache_misses++;
        pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    // 타입 검증
    if (entry->type_tag != handle->type_tag) {
        pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
        return SLOT_ERROR_TYPE_MISMATCH;
    }
    
    // 데이터 블록 확인
    if (!entry->data_block_ref) {
        pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    // 데이터 복사 (크기는 타입에 따라 결정)
    size_t copy_size = type_get_size(entry->type_tag);
    if (copy_size > buffer_size) copy_size = buffer_size;
    
    memcpy(buffer, entry->data_block_ref, copy_size);
    if (bytes_read) *bytes_read = copy_size;
    
    manager->cache_hits++;
    pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
    return SLOT_SUCCESS;
}

// 슬롯 해제
SlotError slot_release(SlotManager* manager, const SlotHandle* handle) {
    if (!manager || !handle) return SLOT_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock((pthread_mutex_t*)manager->mutex);
    
    // 슬롯 찾기
    for (size_t i = 0; i < manager->table_size; i++) {
        SlotEntry* entry = &manager->slot_table[i];
        
        if (entry->slot_id == handle->slot_id && entry->occupied) {
            // 메모리 블록 해제
            if (entry->data_block_ref) {
                size_t block_size = type_get_size(entry->type_tag);
                deallocate_memory_block(manager, entry->data_block_ref, block_size);
            }
            
            // 슬롯 초기화
            memset(entry, 0, sizeof(SlotEntry));
            
            // 통계 업데이트
            manager->total_deallocations++;
            manager->active_slots--;
            
            pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
            return SLOT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
    return SLOT_ERROR_SLOT_NOT_FOUND;
}

// 타입 검증
bool slot_validate_type(SlotManager* manager, const SlotHandle* handle, TypeTag expected_type) {
    if (!manager || !handle) return false;
    return handle->type_tag == expected_type;
}

// 핸들 유효성 검사
bool slot_is_valid(SlotManager* manager, const SlotHandle* handle) {
    if (!manager || !handle) return false;
    
    pthread_mutex_lock((pthread_mutex_t*)manager->mutex);
    
    for (size_t i = 0; i < manager->table_size; i++) {
        SlotEntry* entry = &manager->slot_table[i];
        if (entry->slot_id == handle->slot_id && 
            entry->occupied &&
            entry->type_tag == handle->type_tag) {
            pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)manager->mutex);
    return false;
}

// 통계 출력
void slot_manager_print_stats(const SlotManager* manager) {
    if (!manager) return;
    
    printf("=== Pergyra 슬롯 매니저 통계 ===\n");
    printf("총 할당: %lu\n", manager->total_allocations);
    printf("총 해제: %lu\n", manager->total_deallocations);
    printf("활성 슬롯: %lu\n", manager->active_slots);
    printf("캐시 히트: %lu\n", manager->cache_hits);
    printf("캐시 미스: %lu\n", manager->cache_misses);
    printf("테이블 크기: %zu\n", manager->table_size);
    printf("활용률: %.2f%%\n", slot_manager_get_utilization(manager) * 100.0);
}

// 활성 슬롯 수 반환
size_t slot_manager_get_active_count(const SlotManager* manager) {
    return manager ? manager->active_slots : 0;
}

// 슬롯 테이블 활용률 반환
double slot_manager_get_utilization(const SlotManager* manager) {
    if (!manager || manager->table_size == 0) return 0.0;
    return (double)manager->active_slots / (double)manager->table_size;
}

// 타입 크기 반환
size_t type_get_size(TypeTag tag) {
    switch (tag) {
        case TYPE_INT: return sizeof(int32_t);
        case TYPE_LONG: return sizeof(int64_t);
        case TYPE_FLOAT: return sizeof(float);
        case TYPE_DOUBLE: return sizeof(double);
        case TYPE_BOOL: return sizeof(bool);
        case TYPE_STRING: return 256; // 최대 문자열 크기
        case TYPE_VECTOR: return 1024; // 벡터 크기
        default: return 64; // 기본 크기
    }
}

// 타입 문자열 변환
const char* type_tag_to_string(TypeTag tag) {
    switch (tag) {
        case TYPE_INT: return "Int";
        case TYPE_LONG: return "Long";
        case TYPE_FLOAT: return "Float";
        case TYPE_DOUBLE: return "Double";
        case TYPE_BOOL: return "Bool";
        case TYPE_STRING: return "String";
        case TYPE_VECTOR: return "Vector";
        default: return "Custom";
    }
}

// 원시 타입 확인
bool type_is_primitive(TypeTag tag) {
    return tag >= TYPE_INT && tag <= TYPE_BOOL;
}

// 타입 해시 생성
uint32_t type_tag_hash(const char* type_name) {
    if (!type_name) return 0;
    
    // 간단한 해시 함수 (djb2)
    uint32_t hash = 5381;
    for (const char* c = type_name; *c; c++) {
        hash = ((hash << 5) + hash) + *c;
    }
    return hash;
}

// 인라인 함수 구현
static inline uint32_t slot_hash_function(uint32_t slot_id) {
    return slot_hash_function_asm(slot_id);
}

static inline bool slot_compare_and_swap(volatile uint32_t* ptr, uint32_t expected, uint32_t new_val) {
    return slot_compare_and_swap_asm(ptr, expected, new_val);
}

static inline void slot_memory_barrier(void) {
    slot_memory_barrier_asm();
}