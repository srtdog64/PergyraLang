#ifndef PERGYRA_TRANSPILER_INVENTORY_VIEW_H
#define PERGYRA_TRANSPILER_INVENTORY_VIEW_H

typedef struct
{
    const MIRRoutine *routines;
    size_t            count;
} TranspilerMIRRoutineInventory;

void
transpiler_active_routine_inventory(const TranspilerCtx *ctx,
                                    TranspilerMIRRoutineInventory *inventory);

void
transpiler_mir_routine_inventory_from_program(
    const MIRProgram *mir,
    TranspilerMIRRoutineInventory *inventory);

const MIRRoutine *
transpiler_routine_inventory_get(
    const TranspilerMIRRoutineInventory *inventory,
    size_t index);

MIRScopeKind transpiler_mir_routine_kind(const MIRRoutine *routine);

const char *transpiler_mir_routine_name(const MIRRoutine *routine);

const char *transpiler_mir_routine_owner_name(const MIRRoutine *routine);

ASTNodeType transpiler_mir_routine_owner_ast_type(const MIRRoutine *routine);

bool transpiler_mir_routine_has_signature(const MIRRoutine *routine);

size_t transpiler_mir_routine_generic_param_count(const MIRRoutine *routine);

size_t transpiler_mir_routine_param_count(const MIRRoutine *routine);

FuncParam *transpiler_mir_routine_param(
    const MIRRoutine *routine,
    size_t index);

const char *transpiler_mir_routine_param_type_name(
    const MIRRoutine *routine,
    size_t index);

ASTNode *transpiler_mir_routine_return_type(const MIRRoutine *routine);

const char *transpiler_mir_routine_return_type_name(
    const MIRRoutine *routine);
const MIRCallableSig *transpiler_mir_routine_param_callable_sig(
    const MIRRoutine *routine, size_t index);
const MIRCallableSig *transpiler_mir_routine_return_callable_sig(
    const MIRRoutine *routine);
const char *transpiler_mir_routine_source_local_type_name(
    const MIRRoutine *routine,
    const char *local_name);
size_t transpiler_mir_routine_source_local_type_count(
    const MIRRoutine *routine);
const char *transpiler_mir_routine_source_local_name_at(
    const MIRRoutine *routine,
    size_t index);
const char *transpiler_mir_routine_source_local_type_name_at(
    const MIRRoutine *routine,
    size_t index);
const char *transpiler_mir_routine_within_zone(
    const MIRRoutine *routine);

size_t transpiler_active_routine_count(const TranspilerCtx *ctx);

void
transpiler_active_decl_header_inventory(
    const TranspilerCtx *ctx,
    MIRDeclHeaderInventory *inventory);

void
transpiler_active_inventory(const TranspilerCtx *ctx,
                            ASTNodeType decl_type,
                            ASTNode ***nodes_out,
                            size_t *count_out);

const MIRDeclHeader *transpiler_active_decl_header_of_type(
    const TranspilerCtx *ctx,
    ASTNodeType decl_type,
    const char *name);
const MIRDeclHeader *transpiler_active_host_decl_header(
    const TranspilerCtx *ctx,
    const char *name);

void
transpiler_active_externs(const TranspilerCtx *ctx,
                          ASTNode ***nodes_out,
                          size_t *count_out);

void
transpiler_active_executables(const TranspilerCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out);

bool transpiler_active_has_mir(const TranspilerCtx *ctx);
const char *transpiler_active_source_path(const TranspilerCtx *ctx);

const MIRProgram *transpiler_active_mir_identity(const TranspilerCtx *ctx);

bool transpiler_active_has_main_function(const TranspilerCtx *ctx);

const char *transpiler_active_main_function_name(const TranspilerCtx *ctx);

bool transpiler_active_has_top_level_exec(const TranspilerCtx *ctx);

bool transpiler_active_uses_thread_pool(const TranspilerCtx *ctx);

bool transpiler_active_can_emit_intent_cleanup_from_mir(
    const TranspilerCtx *ctx,
    const ASTNode *intent_decl);

#endif /* PERGYRA_TRANSPILER_INVENTORY_VIEW_H */
