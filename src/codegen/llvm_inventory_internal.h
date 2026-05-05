/* LLVM backend MIR/DIR inventory helper layer.
 * Included by llvm_internal.h after LLVMGenCtx is fully defined. */

#ifndef PGY_LLVM_INVENTORY_INTERNAL_H
#define PGY_LLVM_INVENTORY_INTERNAL_H

#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"

void llvm_active_nominal_inventory(const LLVMGenCtx *ctx,
                                   ASTNode ***nodes_out,
                                   size_t *count_out);

typedef struct
{
    ASTNode **abilities;
    ASTNode **relations;
    ASTNode **effects;
    ASTNode **zones;
    ASTNode **worlds;
    ASTNode **parties;
    ASTNode **rosters;
    ASTNode **roles;
    ASTNode **events;
    size_t ability_count;
    size_t relation_count;
    size_t effect_count;
    size_t zone_count;
    size_t world_count;
    size_t party_count;
    size_t roster_count;
    size_t role_count;
    size_t event_count;
} LLVMDomainInventory;

typedef struct
{
    const MIRRoutine *routines;
    size_t            count;
} LLVMMIRRoutineInventory;

void llvm_active_routine_inventory(const LLVMGenCtx *ctx,
                                   LLVMMIRRoutineInventory *inventory);
void llvm_mir_routine_inventory_from_program(const MIRProgram *mir,
                                             LLVMMIRRoutineInventory *inventory);
const MIRRoutine *llvm_routine_inventory_get(
    const LLVMMIRRoutineInventory *inventory,
    size_t index);
void llvm_active_domain_inventory(const LLVMGenCtx *ctx,
                                  LLVMDomainInventory *inventory);
void llvm_active_executables(const LLVMGenCtx *ctx,
                             ASTNode ***nodes_out,
                             size_t *count_out);
void llvm_active_externs(const LLVMGenCtx *ctx,
                         ASTNode ***nodes_out,
                         size_t *count_out);
ASTNode *llvm_active_synthetic_executable_func(const LLVMGenCtx *ctx);
bool llvm_active_has_main_function(const LLVMGenCtx *ctx);
bool llvm_active_has_top_level_exec(const LLVMGenCtx *ctx);

#endif /* PGY_LLVM_INVENTORY_INTERNAL_H */
