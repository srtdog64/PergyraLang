; Pergyra 슬롯 매니저 - 어셈블리 최적화 함수들
; x86-64 System V ABI 호출 규약 사용
; RDI, RSI, RDX, RCX, R8, R9 순서로 인자 전달

section .text
global slot_claim_fast
global slot_write_fast  
global slot_read_fast
global slot_hash_function_asm
global slot_compare_and_swap_asm
global slot_memory_barrier_asm

; 슬롯 해시 함수 (FNV-1a 해시 변형)
; uint32_t slot_hash_function_asm(uint32_t slot_id)
; 입력: RDI = slot_id
; 출력: RAX = 해시값
slot_hash_function_asm:
    push rbp
    mov rbp, rsp
    
    ; FNV-1a 해시 구현
    mov eax, 0x811c9dc5     ; FNV offset basis
    mov ecx, edi            ; slot_id를 ECX로 복사
    
    ; 4바이트를 각각 처리
    mov edx, ecx
    and edx, 0xFF
    xor eax, edx
    mov edx, 0x01000193     ; FNV prime
    mul edx
    
    mov edx, ecx
    shr edx, 8
    and edx, 0xFF
    xor eax, edx
    mov edx, 0x01000193
    mul edx
    
    mov edx, ecx
    shr edx, 16
    and edx, 0xFF
    xor eax, edx
    mov edx, 0x01000193
    mul edx
    
    mov edx, ecx
    shr edx, 24
    and edx, 0xFF
    xor eax, edx
    mov edx, 0x01000193
    mul edx
    
    pop rbp
    ret

; 원자적 Compare-And-Swap
; bool slot_compare_and_swap_asm(volatile uint32_t* ptr, uint32_t expected, uint32_t new_val)
; 입력: RDI = ptr, RSI = expected, RDX = new_val
; 출력: RAX = 성공여부 (1=성공, 0=실패)
slot_compare_and_swap_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, esi            ; expected 값을 EAX에
    lock cmpxchg [rdi], edx ; 원자적 비교 교환
    sete al                 ; 성공시 AL = 1, 실패시 AL = 0
    movzx eax, al           ; AL을 EAX로 확장
    
    pop rbp
    ret

; 메모리 배리어
; void slot_memory_barrier_asm(void)
slot_memory_barrier_asm:
    mfence                  ; 전체 메모리 배리어
    ret

; 고속 슬롯 클레임
; SlotError slot_claim_fast(SlotManager* manager, TypeTag type, SlotHandle* handle)
; 입력: RDI = manager, RSI = type, RDX = handle
; 출력: RAX = SlotError
slot_claim_fast:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; 레지스터 할당
    mov r12, rdi            ; manager
    mov r13, rsi            ; type
    mov r14, rdx            ; handle
    
    ; 매니저 유효성 검사
    test r12, r12
    jz .error_invalid_manager
    
    ; 슬롯 테이블 가져오기
    mov r15, [r12]          ; manager->slot_table
    test r15, r15
    jz .error_invalid_table
    
    ; 다음 슬롯 ID 원자적 증가
    mov rax, r12
    add rax, 16             ; offset to next_slot_id
    mov ecx, 1
    lock xadd [rax], ecx    ; 원자적 증가하고 이전 값 반환
    mov ebx, ecx            ; 새 슬롯 ID
    
    ; 슬롯 테이블에서 빈 슬롯 찾기
    mov rax, [r12 + 8]      ; table_size
    mov rcx, 0              ; 인덱스 초기화
    
.find_slot_loop:
    cmp rcx, rax
    jge .error_no_slots     ; 테이블 끝에 도달
    
    ; 슬롯 엔트리 계산 (sizeof(SlotEntry) = 32바이트)
    mov rdx, rcx
    shl rdx, 5              ; rcx * 32
    add rdx, r15            ; slot_table + offset
    
    ; occupied 필드 확인 (offset 8)
    mov r8, rdx
    add r8, 8
    mov r9d, 0
    mov r10d, 1
    lock cmpxchg [r8], r10d ; occupied를 0에서 1로 원자적 변경
    jz .slot_found          ; 성공하면 슬롯 발견
    
    inc rcx
    jmp .find_slot_loop
    
.slot_found:
    ; 슬롯 엔트리 초기화
    mov [rdx], ebx          ; slot_id
    mov [rdx + 4], r13d     ; type_tag
    ; occupied는 이미 1로 설정됨
    mov qword [rdx + 16], 0 ; data_block_ref = NULL (나중에 할당)
    mov dword [rdx + 24], 0 ; ttl = 0 (무제한)
    
    ; 스레드 affinity 설정 (현재는 0으로)
    mov dword [rdx + 28], 0
    
    ; 할당 시간 기록 (RDTSC 사용)
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov [rdx + 32], rax     ; allocation_time (실제로는 오프셋 계산 필요)
    
    ; 핸들 설정
    mov [r14], ebx          ; handle->slot_id
    mov [r14 + 4], r13d     ; handle->type_tag
    mov dword [r14 + 8], 1  ; handle->generation = 1
    
    ; 통계 업데이트 (원자적)
    mov rax, r12
    add rax, 40             ; offset to total_allocations
    lock inc qword [rax]
    
    mov rax, r12
    add rax, 56             ; offset to active_slots
    lock inc qword [rax]
    
    mov eax, 0              ; SLOT_SUCCESS
    jmp .cleanup
    
.error_invalid_manager:
    mov eax, 1              ; SLOT_ERROR_OUT_OF_MEMORY (임시)
    jmp .cleanup
    
.error_invalid_table:
    mov eax, 1
    jmp .cleanup
    
.error_no_slots:
    mov eax, 1
    jmp .cleanup
    
.cleanup:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; 고속 슬롯 쓰기
; SlotError slot_write_fast(SlotManager* manager, const SlotHandle* handle, const void* data, size_t data_size)
; 입력: RDI = manager, RSI = handle, RDX = data, RCX = data_size
; 출력: RAX = SlotError
slot_write_fast:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; 레지스터 할당
    mov r12, rdi            ; manager
    mov r13, rsi            ; handle
    mov r14, rdx            ; data
    mov r15, rcx            ; data_size
    
    ; 매니저 유효성 검사
    test r12, r12
    jz .write_error
    
    ; 핸들 유효성 검사
    test r13, r13
    jz .write_error
    
    ; 슬롯 ID 가져오기
    mov ebx, [r13]          ; handle->slot_id
    
    ; 슬롯 테이블에서 엔트리 찾기
    mov rax, [r12]          ; slot_table
    mov rcx, [r12 + 8]      ; table_size
    mov rdx, 0              ; 인덱스
    
.find_write_slot_loop:
    cmp rdx, rcx
    jge .write_error
    
    ; 슬롯 엔트리 계산
    mov r8, rdx
    shl r8, 5               ; rdx * 32
    add r8, rax             ; slot_table + offset
    
    ; 슬롯 ID 비교
    mov r9d, [r8]           ; entry->slot_id
    cmp r9d, ebx
    je .write_slot_found
    
    inc rdx
    jmp .find_write_slot_loop
    
.write_slot_found:
    ; 타입 검증
    mov r9d, [r8 + 4]       ; entry->type_tag
    mov r10d, [r13 + 4]     ; handle->type_tag
    cmp r9d, r10d
    jne .write_error
    
    ; occupied 확인
    mov r9b, [r8 + 8]       ; entry->occupied
    test r9b, r9b
    jz .write_error
    
    ; 데이터 블록 참조 가져오기
    mov r9, [r8 + 16]       ; entry->data_block_ref
    test r9, r9
    jz .write_allocate_block
    
.write_data:
    ; 실제 데이터 복사 (memcpy 최적화 버전)
    mov rsi, r14            ; 소스
    mov rdi, r9             ; 대상
    mov rcx, r15            ; 크기
    
    ; 8바이트씩 복사 (최적화)
    shr rcx, 3              ; 8로 나누기
    rep movsq               ; 8바이트씩 복사
    
    ; 나머지 바이트 처리
    mov rcx, r15
    and rcx, 7              ; 나머지
    rep movsb               ; 바이트 단위 복사
    
    mov eax, 0              ; SLOT_SUCCESS
    jmp .write_cleanup
    
.write_allocate_block:
    ; 메모리 블록 할당 (간단한 구현 - 실제로는 메모리 풀 사용)
    ; 여기서는 일단 에러 반환
    mov eax, 1              ; SLOT_ERROR_OUT_OF_MEMORY
    jmp .write_cleanup
    
.write_error:
    mov eax, 2              ; SLOT_ERROR_INVALID_HANDLE
    
.write_cleanup:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; 고속 슬롯 읽기
; SlotError slot_read_fast(SlotManager* manager, const SlotHandle* handle, void* buffer, size_t buffer_size)
; 입력: RDI = manager, RSI = handle, RDX = buffer, RCX = buffer_size
; 출력: RAX = SlotError
slot_read_fast:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; 레지스터 할당
    mov r12, rdi            ; manager
    mov r13, rsi            ; handle
    mov r14, rdx            ; buffer
    mov r15, rcx            ; buffer_size
    
    ; 유효성 검사
    test r12, r12
    jz .read_error
    test r13, r13
    jz .read_error
    test r14, r14
    jz .read_error
    
    ; 슬롯 찾기 (write와 동일한 로직)
    mov ebx, [r13]          ; handle->slot_id
    mov rax, [r12]          ; slot_table
    mov rcx, [r12 + 8]      ; table_size
    mov rdx, 0
    
.find_read_slot_loop:
    cmp rdx, rcx
    jge .read_error
    
    mov r8, rdx
    shl r8, 5
    add r8, rax
    
    mov r9d, [r8]
    cmp r9d, ebx
    je .read_slot_found
    
    inc rdx
    jmp .find_read_slot_loop
    
.read_slot_found:
    ; 타입 및 상태 검증
    mov r9d, [r8 + 4]
    mov r10d, [r13 + 4]
    cmp r9d, r10d
    jne .read_error
    
    mov r9b, [r8 + 8]
    test r9b, r9b
    jz .read_error
    
    ; 데이터 블록에서 읽기
    mov r9, [r8 + 16]       ; data_block_ref
    test r9, r9
    jz .read_error
    
    ; 메모리 복사 (최적화)
    mov rsi, r9             ; 소스
    mov rdi, r14            ; 대상
    mov rcx, r15            ; 크기
    
    shr rcx, 3
    rep movsq
    
    mov rcx, r15
    and rcx, 7
    rep movsb
    
    ; 캐시 히트 통계 업데이트
    mov rax, r12
    add rax, 64             ; offset to cache_hits
    lock inc qword [rax]
    
    mov eax, 0              ; SLOT_SUCCESS
    jmp .read_cleanup
    
.read_error:
    ; 캐시 미스 통계 업데이트
    mov rax, r12
    add rax, 72             ; offset to cache_misses
    lock inc qword [rax]
    
    mov eax, 2              ; SLOT_ERROR_INVALID_HANDLE
    
.read_cleanup:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .data
; 상수 데이터
slot_magic_number: dd 0xDEADBEEF
error_messages:
    db "Invalid manager", 0
    db "Invalid handle", 0
    db "Type mismatch", 0
    db "Out of memory", 0