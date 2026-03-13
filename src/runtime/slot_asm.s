;
; Copyright (c) 2025 Pergyra Language Project
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 1. Redistributions of source code must retain the above copyright
;    notice, this list of conditions and the following disclaimer.
; 2. Redistributions in binary form must reproduce the above copyright
;    notice, this list of conditions and the following disclaimer in the
;    documentation and/or other materials provided with the distribution.
; 3. Neither the name of the Pergyra Language Project nor the names of
;    its contributors may be used to endorse or promote products derived
;    from this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
; ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
; LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
; CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
; SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
; INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
; CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
; ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
; POSSIBILITY OF SUCH DAMAGE.
;

; Pergyra Slot Manager - Assembly optimized routines
; x86-64 System V ABI calling convention
; Arguments: RDI, RSI, RDX, RCX, R8, R9

section .text
global slot_claim_fast
global slot_write_fast  
global slot_read_fast
global slot_hash_function_asm
global slot_compare_and_swap_asm
global slot_memory_barrier_asm
global SlotMaskMirrorChecksumAsm
global SlotUnmaskChecksumAsm

;
; Fast slot hash function (FNV-1a variant)
; uint32_t slot_hash_function_asm(uint32_t slot_id)
; Input: RDI = slot_id
; Output: RAX = hash value
;
slot_hash_function_asm:
    push    rbp
    mov     rbp, rsp
    
    ; FNV-1a hash implementation
    mov     eax, 0x811c9dc5        ; FNV offset basis
    mov     ecx, edi               ; slot_id to ECX
    
    ; Process each byte of the 4-byte input
    mov     edx, ecx
    and     edx, 0xFF
    xor     eax, edx
    mov     edx, 0x01000193        ; FNV prime
    mul     edx
    
    mov     edx, ecx
    shr     edx, 8
    and     edx, 0xFF
    xor     eax, edx
    mov     edx, 0x01000193
    mul     edx
    
    mov     edx, ecx
    shr     edx, 16
    and     edx, 0xFF
    xor     eax, edx
    mov     edx, 0x01000193
    mul     edx
    
    mov     edx, ecx
    shr     edx, 24
    and     edx, 0xFF
    xor     eax, edx
    mov     edx, 0x01000193
    mul     edx
    
    pop     rbp
    ret

;
; Atomic Compare-And-Swap operation
; bool slot_compare_and_swap_asm(volatile uint32_t* ptr, uint32_t expected, uint32_t new_val)
; Input: RDI = ptr, RSI = expected, RDX = new_val
; Output: RAX = success (1=success, 0=failure)
;
slot_compare_and_swap_asm:
    push    rbp
    mov     rbp, rsp
    
    mov     eax, esi               ; expected value to EAX
    lock cmpxchg [rdi], edx        ; atomic compare and exchange
    sete    al                     ; set AL = 1 if successful, 0 if failed
    movzx   eax, al                ; zero-extend AL to EAX
    
    pop     rbp
    ret

;
; Memory barrier operation
; void slot_memory_barrier_asm(void)
;
slot_memory_barrier_asm:
    mfence                         ; full memory barrier
    ret

;
; Fast slot claim operation
; SlotError slot_claim_fast(SlotManager* manager, TypeTag type, SlotHandle* handle)
; Input: RDI = manager, RSI = type, RDX = handle
; Output: RAX = SlotError
;
slot_claim_fast:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    ; Register allocation
    mov     r12, rdi               ; manager
    mov     r13, rsi               ; type
    mov     r14, rdx               ; handle
    
    ; Validate manager pointer
    test    r12, r12
    jz      .error_invalid_manager
    
    ; Get slot table
    mov     r15, [r12]             ; manager->slot_table
    test    r15, r15
    jz      .error_invalid_table
    
    ; Atomically increment next_slot_id
    mov     rax, r12
    add     rax, 16                ; offset to next_slot_id
    mov     ecx, 1
    lock xadd [rax], ecx           ; atomically increment and return old value
    mov     ebx, ecx               ; new slot ID
    
    ; Find empty slot in table
    mov     rax, [r12 + 8]         ; table_size
    mov     rcx, 0                 ; index counter
    
.find_slot_loop:
    cmp     rcx, rax
    jge     .error_no_slots        ; reached end of table
    
    ; Calculate slot entry (sizeof(SlotEntry) = 32 bytes)
    mov     rdx, rcx
    shl     rdx, 5                 ; rcx * 32
    add     rdx, r15               ; slot_table + offset
    
    ; Check occupied field atomically (offset 8)
    mov     r8, rdx
    add     r8, 8
    mov     r9d, 0
    mov     r10d, 1
    lock cmpxchg [r8], r10d        ; try to set occupied from 0 to 1
    jz      .slot_found            ; if successful, slot is found
    
    inc     rcx
    jmp     .find_slot_loop
    
.slot_found:
    ; Initialize slot entry
    mov     [rdx], ebx             ; slot_id
    mov     [rdx + 4], r13d        ; type_tag
    ; occupied is already set to 1
    mov     qword [rdx + 16], 0    ; data_block_ref = NULL
    mov     dword [rdx + 24], 0    ; ttl = 0 (unlimited)
    
    ; Set thread affinity (currently 0)
    mov     dword [rdx + 28], 0
    
    ; Record allocation time using RDTSC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     [rdx + 32], rax        ; allocation_time
    
    ; Set handle values
    mov     [r14], ebx             ; handle->slot_id
    mov     [r14 + 4], r13d        ; handle->type_tag
    mov     dword [r14 + 8], 1     ; handle->generation = 1
    
    ; Update statistics atomically
    mov     rax, r12
    add     rax, 40                ; offset to total_allocations
    lock inc qword [rax]
    
    mov     rax, r12
    add     rax, 56                ; offset to active_slots
    lock inc qword [rax]
    
    mov     eax, 0                 ; SLOT_SUCCESS
    jmp     .cleanup
    
.error_invalid_manager:
    mov     eax, 1                 ; SLOT_ERROR_OUT_OF_MEMORY (temporary)
    jmp     .cleanup
    
.error_invalid_table:
    mov     eax, 1
    jmp     .cleanup
    
.error_no_slots:
    mov     eax, 1
    jmp     .cleanup
    
.cleanup:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret

;
; Fast slot write operation
; SlotError slot_write_fast(SlotManager* manager, const SlotHandle* handle, 
;                          const void* data, size_t data_size)
; Input: RDI = manager, RSI = handle, RDX = data, RCX = data_size
; Output: RAX = SlotError
;
slot_write_fast:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    ; Register allocation
    mov     r12, rdi               ; manager
    mov     r13, rsi               ; handle
    mov     r14, rdx               ; data
    mov     r15, rcx               ; data_size
    
    ; Validate inputs
    test    r12, r12
    jz      .write_error
    test    r13, r13
    jz      .write_error
    
    ; Get slot ID from handle
    mov     ebx, [r13]             ; handle->slot_id
    
    ; Find slot entry in table
    mov     rax, [r12]             ; slot_table
    mov     rcx, [r12 + 8]         ; table_size
    mov     rdx, 0                 ; index
    
.find_write_slot_loop:
    cmp     rdx, rcx
    jge     .write_error
    
    ; Calculate slot entry offset
    mov     r8, rdx
    shl     r8, 5                  ; rdx * 32
    add     r8, rax                ; slot_table + offset
    
    ; Compare slot ID
    mov     r9d, [r8]              ; entry->slot_id
    cmp     r9d, ebx
    je      .write_slot_found
    
    inc     rdx
    jmp     .find_write_slot_loop
    
.write_slot_found:
    ; Validate type
    mov     r9d, [r8 + 4]          ; entry->type_tag
    mov     r10d, [r13 + 4]        ; handle->type_tag
    cmp     r9d, r10d
    jne     .write_error
    
    ; Check if slot is occupied
    mov     r9b, [r8 + 8]          ; entry->occupied
    test    r9b, r9b
    jz      .write_error
    
    ; Get data block reference
    mov     r9, [r8 + 16]          ; entry->data_block_ref
    test    r9, r9
    jz      .write_allocate_block
    
.write_data:
    ; Optimized memory copy with XOR Obfuscation
    mov     rsi, r14               ; source
    mov     rdi, r9                ; destination
    mov     rcx, r15               ; size

    ; Generate Obfuscation Key (XOR Mask)
    ; key = slot_magic_number ^ slot_id
    mov     r11d, dword [rel slot_magic_number]
    xor     r11d, ebx              ; EBX contains slot_id
    ; duplicate this 32-bit key to 64-bit in R11
    mov     r10d, r11d
    shl     r11, 32
    or      r11, r10               ; R11 now contains obfuscation key (64-bit)
    
    ; Copy 8 bytes at a time for optimization
    mov     rax, rcx
    shr     rcx, 3                 ; divide by 8
    jz      .write_data_remainder

.write_qword_loop:
    mov     r10, [rsi]
    xor     r10, r11               ; Apply XOR Mask
    mov     [rdi], r10
    add     rsi, 8
    add     rdi, 8
    dec     rcx
    jnz     .write_qword_loop

.write_data_remainder:
    ; Handle remaining bytes
    mov     rcx, rax               ; restore original size
    and     rcx, 7                 ; remainder
    jz      .write_data_done

.write_byte_loop:
    mov     r10b, [rsi]
    xor     r10b, r11b             ; Apply XOR Mask (lowest byte of key)
    mov     [rdi], r10b
    add     rsi, 1
    add     rdi, 1
    ; Rotate key by 8 bits to keep masking somewhat consistent for uneven lengths
    ror     r11, 8
    dec     rcx
    jnz     .write_byte_loop

.write_data_done:
    
    mov     eax, 0                 ; SLOT_SUCCESS
    jmp     .write_cleanup
    
.write_allocate_block:
    ; Memory block allocation (simplified - should use memory pool)
    mov     eax, 1                 ; SLOT_ERROR_OUT_OF_MEMORY
    jmp     .write_cleanup
    
.write_error:
    mov     eax, 2                 ; SLOT_ERROR_INVALID_HANDLE
    
.write_cleanup:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret

;
; Fast slot read operation
; SlotError slot_read_fast(SlotManager* manager, const SlotHandle* handle, 
;                         void* buffer, size_t buffer_size)
; Input: RDI = manager, RSI = handle, RDX = buffer, RCX = buffer_size
; Output: RAX = SlotError
;
slot_read_fast:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    ; Register allocation
    mov     r12, rdi               ; manager
    mov     r13, rsi               ; handle
    mov     r14, rdx               ; buffer
    mov     r15, rcx               ; buffer_size
    
    ; Validate inputs
    test    r12, r12
    jz      .read_error
    test    r13, r13
    jz      .read_error
    test    r14, r14
    jz      .read_error
    
    ; Find slot (same logic as write)
    mov     ebx, [r13]             ; handle->slot_id
    mov     rax, [r12]             ; slot_table
    mov     rcx, [r12 + 8]         ; table_size
    mov     rdx, 0
    
.find_read_slot_loop:
    cmp     rdx, rcx
    jge     .read_error
    
    mov     r8, rdx
    shl     r8, 5
    add     r8, rax
    
    mov     r9d, [r8]
    cmp     r9d, ebx
    je      .read_slot_found
    
    inc     rdx
    jmp     .find_read_slot_loop
    
.read_slot_found:
    ; Validate type and state
    mov     r9d, [r8 + 4]
    mov     r10d, [r13 + 4]
    cmp     r9d, r10d
    jne     .read_error
    
    mov     r9b, [r8 + 8]
    test    r9b, r9b
    jz      .read_error
    
    ; Read from data block
    mov     r9, [r8 + 16]          ; data_block_ref
    test    r9, r9
    jz      .read_error
    
    ; Optimized memory copy with XOR De-obfuscation
    mov     rsi, r9                ; source
    mov     rdi, r14               ; destination
    mov     rcx, r15               ; size

    ; Generate Obfuscation Key (XOR Mask)
    ; key = slot_magic_number ^ slot_id
    mov     r11d, dword [rel slot_magic_number]
    xor     r11d, ebx              ; EBX contains slot_id
    ; duplicate this 32-bit key to 64-bit in R11
    mov     r10d, r11d
    shl     r11, 32
    or      r11, r10               ; R11 now contains obfuscation key (64-bit)
    
    mov     rax, rcx
    shr     rcx, 3                 ; divide by 8
    jz      .read_data_remainder

.read_qword_loop:
    mov     r10, [rsi]
    xor     r10, r11               ; Apply XOR De-Mask
    mov     [rdi], r10
    add     rsi, 8
    add     rdi, 8
    dec     rcx
    jnz     .read_qword_loop

.read_data_remainder:
    mov     rcx, rax
    and     rcx, 7
    jz      .read_data_done

.read_byte_loop:
    mov     r10b, [rsi]
    xor     r10b, r11b             ; Apply XOR De-Mask (lowest byte of key)
    mov     [rdi], r10b
    add     rsi, 1
    add     rdi, 1
    ; Rotate key by 8 bits to keep masking somewhat consistent for uneven lengths
    ror     r11, 8
    dec     rcx
    jnz     .read_byte_loop

.read_data_done:
    
    ; Update cache hit statistics
    mov     rax, r12
    add     rax, 64                ; offset to cache_hits
    lock inc qword [rax]
    
    mov     eax, 0                 ; SLOT_SUCCESS
    jmp     .read_cleanup
    
.read_error:
    ; Update cache miss statistics
    mov     rax, r12
    add     rax, 72                ; offset to cache_misses
    lock inc qword [rax]
    
    mov     eax, 2                 ; SLOT_ERROR_INVALID_HANDLE
    
.read_cleanup:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret

section .data
; Constants
slot_magic_number: dd 0xDEADBEEF
error_messages:
    db "Invalid manager", 0
    db "Invalid handle", 0
    db "Type mismatch", 0
    db "Out of memory", 0

section .text

; uint32_t SlotMaskMirrorChecksumAsm(uint8_t* primary, uint8_t* shadow,
;                                    const uint8_t* src, size_t size,
;                                    uint32_t mask_key)
; RDI=primary, RSI=shadow, RDX=src, RCX=size, R8D=mask_key
SlotMaskMirrorChecksumAsm:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16
    push    rbx
    push    r12

    mov     dword [rbp - 16], r8d
    xor     r9d, r9d              ; checksum
    xor     r10, r10              ; index

.mask_loop:
    cmp     r10, rcx
    jae     .mask_done

    movzx   ebx, byte [rdx + r10]
    mov     r11, r10
    and     r11, 3
    mov     r12b, [rbp - 16 + r11]

    rol     r9d, 5
    xor     r9d, ebx
    add     r9d, ebx

    xor     bl, r12b
    mov     [rdi + r10], bl
    mov     [rsi + r10], bl

    inc     r10
    jmp     .mask_loop

.mask_done:
    mov     eax, r9d
    pop     r12
    pop     rbx
    leave
    ret

; uint32_t SlotUnmaskChecksumAsm(uint8_t* dst, const uint8_t* src,
;                                size_t size, uint32_t mask_key)
; RDI=dst, RSI=src, RDX=size, ECX=mask_key
SlotUnmaskChecksumAsm:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 16
    push    rbx

    mov     dword [rbp - 16], ecx
    xor     r8d, r8d              ; checksum
    xor     r9, r9                ; index

.unmask_loop:
    cmp     r9, rdx
    jae     .unmask_done

    movzx   ebx, byte [rsi + r9]
    mov     r10, r9
    and     r10, 3
    mov     r11b, [rbp - 16 + r10]
    xor     bl, r11b
    mov     [rdi + r9], bl

    rol     r8d, 5
    xor     r8d, ebx
    add     r8d, ebx

    inc     r9
    jmp     .unmask_loop

.unmask_done:
    mov     eax, r8d
    pop     rbx
    leave
    ret
