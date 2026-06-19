#ifndef PERGYRA_LLVM_DOMAIN_PROJECTION_COUNT_HELPERS_H
#define PERGYRA_LLVM_DOMAIN_PROJECTION_COUNT_HELPERS_H

#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct LLVMHostedDomainSlotView LLVMHostedDomainSlotView;
typedef struct LLVMHostedZoneRefreshView LLVMHostedZoneRefreshView;

bool llvm_domain_slot_view_is_projection_slot(
    const LLVMHostedDomainSlotView *slot_view,
    size_t index,
    ASTNode **refreshes,
    size_t refresh_count);
size_t llvm_count_domain_projection_slots_in_view(
    const LLVMHostedDomainSlotView *slot_view,
    ASTNode **refreshes,
    size_t refresh_count);
bool llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view(
    const LLVMHostedDomainSlotView *slot_view,
    size_t index,
    const LLVMHostedZoneRefreshView *refresh_view);
size_t llvm_count_domain_projection_slots_in_zone_refresh_view(
    const LLVMHostedDomainSlotView *slot_view,
    const LLVMHostedZoneRefreshView *refresh_view);

#endif /* PERGYRA_LLVM_DOMAIN_PROJECTION_COUNT_HELPERS_H */
