#ifndef PERGYRA_SLOT_MANAGER_H
#define PERGYRA_SLOT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 슬롯 테이블 엔트리
typedef struct {
    uint32_t slot_id;
    uint32_t type_tag;      // 타입 식별자 (해시값)
    bool occupied;
    void* data_block_ref;   // 실제 데이터 블록 포인터
    uint32_t ttl;           // Time-To-Live (밀리초 또는 스코프 카운터)
    uint32_t thread_affinity; // 할당된 스레드 ID
    uint64_t allocation_time; // 할당 시간 (성능 측정용)
} SlotEntry;

// 슬롯 매니저 구조체
typedef struct {
    SlotEntry* slot_table;
    size_t table_size;
    size_t max_slots;
    uint32_t next_slot_id;
    
    // 메모리 풀 참조
    void* memory_pool;
    
    // 동시성 제어
    void* mutex;            // pthread_mutex_t 또는 다른 뮤텍스
    
    // 통계
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t active_slots;
    uint64_t cache_hits;
    uint64_t cache_misses;
} SlotManager;

// 슬롯 핸들 (외부에서 사용하는 참조)
typedef struct {
    uint32_t slot_id;
    uint32_t type_tag;
    uint32_t generation;    // ABA 문제 방지용
} SlotHandle;

// 타입 정보
typedef enum {
    TYPE_INT = 0x1,
    TYPE_LONG = 0x2,
    TYPE_FLOAT = 0x3,
    TYPE_DOUBLE = 0x4,
    TYPE_STRING = 0x5,
    TYPE_BOOL = 0x6,
    TYPE_VECTOR = 0x7,
    TYPE_CUSTOM = 0x1000   // 사용자 정의 타입 시작
} TypeTag;

// 에러 코드
typedef enum {
    SLOT_SUCCESS = 0,
    SLOT_ERROR_OUT_OF_MEMORY,
    SLOT_ERROR_INVALID_HANDLE,
    SLOT_ERROR_TYPE_MISMATCH,
    SLOT_ERROR_SLOT_NOT_FOUND,
    SLOT_ERROR_PERMISSION_DENIED,
    SLOT_ERROR_TTL_EXPIRED,
    SLOT_ERROR_THREAD_VIOLATION
} SlotError;

// 슬롯 매니저 생성 및 해제
SlotManager* slot_manager_create(size_t max_slots, size_t memory_pool_size);
void slot_manager_destroy(SlotManager* manager);

// 슬롯 라이프사이클 (어셈블리 최적화된 핵심 함수들)
SlotError slot_claim(SlotManager* manager, TypeTag type, SlotHandle* handle);
SlotError slot_write(SlotManager* manager, const SlotHandle* handle, 
                     const void* data, size_t data_size);
SlotError slot_read(SlotManager* manager, const SlotHandle* handle, 
                    void* buffer, size_t buffer_size, size_t* bytes_read);
SlotError slot_release(SlotManager* manager, const SlotHandle* handle);

// 스코프 기반 관리
SlotError slot_claim_scoped(SlotManager* manager, TypeTag type, 
                           uint32_t scope_id, SlotHandle* handle);
SlotError slot_release_scope(SlotManager* manager, uint32_t scope_id);

// 타입 안전성 검사 (어셈블리 최적화)
bool slot_validate_type(SlotManager* manager, const SlotHandle* handle, TypeTag expected_type);
bool slot_is_valid(SlotManager* manager, const SlotHandle* handle);

// TTL 관리
SlotError slot_set_ttl(SlotManager* manager, const SlotHandle* handle, uint32_t ttl_ms);
SlotError slot_refresh_ttl(SlotManager* manager, const SlotHandle* handle);
void slot_cleanup_expired(SlotManager* manager);

// 동시성 지원
SlotError slot_lock(SlotManager* manager, const SlotHandle* handle);
SlotError slot_unlock(SlotManager* manager, const SlotHandle* handle);
SlotError slot_try_lock(SlotManager* manager, const SlotHandle* handle);

// 통계 및 디버깅
void slot_manager_print_stats(const SlotManager* manager);
size_t slot_manager_get_active_count(const SlotManager* manager);
double slot_manager_get_utilization(const SlotManager* manager);

// 타입 유틸리티
uint32_t type_tag_hash(const char* type_name);
const char* type_tag_to_string(TypeTag tag);
bool type_is_primitive(TypeTag tag);
size_t type_get_size(TypeTag tag);

// 어셈블리 최적화된 내부 함수들 (인라인)
static inline uint32_t slot_hash_function(uint32_t slot_id);
static inline bool slot_compare_and_swap(volatile uint32_t* ptr, uint32_t expected, uint32_t new_val);
static inline void slot_memory_barrier(void);

// 어셈블리 구현 함수들
extern SlotError slot_claim_fast(SlotManager* manager, TypeTag type, SlotHandle* handle);
extern SlotError slot_write_fast(SlotManager* manager, const SlotHandle* handle, 
                                 const void* data, size_t data_size);
extern SlotError slot_read_fast(SlotManager* manager, const SlotHandle* handle, 
                                void* buffer, size_t buffer_size);

#endif // PERGYRA_SLOT_MANAGER_H