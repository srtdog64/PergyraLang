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

typedef struct
{
    const MIRDeclHeader *headers;
    size_t               count;
} LLVMMIRDeclHeaderInventory;

void llvm_active_routine_inventory(const LLVMGenCtx *ctx,
                                   LLVMMIRRoutineInventory *inventory);
void llvm_mir_routine_inventory_from_program(const MIRProgram *mir,
                                             LLVMMIRRoutineInventory *inventory);
const MIRRoutine *llvm_routine_inventory_get(
    const LLVMMIRRoutineInventory *inventory,
    size_t index);
const MIRRoutine *llvm_active_function_routine_by_name(
    const LLVMGenCtx *ctx,
    const char *name);
void llvm_active_decl_header_inventory(
    const LLVMGenCtx *ctx,
    LLVMMIRDeclHeaderInventory *inventory);
const MIRDeclHeader *llvm_decl_header_inventory_get(
    const LLVMMIRDeclHeaderInventory *inventory,
    size_t index);
MIRScopeKind llvm_mir_routine_kind(const MIRRoutine *routine);
const char *llvm_mir_routine_name(const MIRRoutine *routine);
const char *llvm_mir_routine_owner_name(const MIRRoutine *routine);
ASTNodeType llvm_mir_routine_owner_ast_type(const MIRRoutine *routine);
bool llvm_mir_routine_has_signature(const MIRRoutine *routine);
size_t llvm_mir_routine_generic_param_count(const MIRRoutine *routine);
size_t llvm_mir_routine_param_count(const MIRRoutine *routine);
FuncParam *llvm_mir_routine_param(const MIRRoutine *routine, size_t index);
const char *llvm_mir_routine_param_type_name(const MIRRoutine *routine,
                                             size_t index);
ASTNode *llvm_mir_routine_return_type(const MIRRoutine *routine);
const char *llvm_mir_routine_return_type_name(const MIRRoutine *routine);
const char *llvm_mir_routine_within_zone(const MIRRoutine *routine);
void llvm_active_domain_inventory(const LLVMGenCtx *ctx,
                                  LLVMDomainInventory *inventory);
void llvm_active_executables(const LLVMGenCtx *ctx,
                             ASTNode ***nodes_out,
                             size_t *count_out);
void llvm_active_externs(const LLVMGenCtx *ctx,
                         ASTNode ***nodes_out,
                         size_t *count_out);
bool llvm_active_has_mir(const LLVMGenCtx *ctx);
const char *llvm_active_source_path(const LLVMGenCtx *ctx);
bool llvm_active_has_main_function(const LLVMGenCtx *ctx);
const char *llvm_active_main_function_name(const LLVMGenCtx *ctx);
bool llvm_active_has_top_level_exec(const LLVMGenCtx *ctx);
bool llvm_active_uses_thread_pool(const LLVMGenCtx *ctx);

#endif /* PGY_LLVM_INVENTORY_INTERNAL_H */
