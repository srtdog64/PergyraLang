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

ASTNode *transpiler_mir_routine_source_ast(const MIRRoutine *routine);

ASTNode *transpiler_mir_routine_source_ast_of_type(
    const MIRRoutine *routine,
    MIRScopeKind expected_kind,
    ASTNodeType expected_ast_type);

size_t transpiler_active_routine_count(const TranspilerCtx *ctx);

void
transpiler_active_inventory(const TranspilerCtx *ctx,
                            ASTNodeType decl_type,
                            ASTNode ***nodes_out,
                            size_t *count_out);

const MIRDeclHeader *transpiler_active_decl_header(
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

ASTNode *transpiler_active_synthetic_executable_func(
    const TranspilerCtx *ctx);

bool transpiler_active_has_mir(const TranspilerCtx *ctx);

const MIRProgram *transpiler_active_mir_identity(const TranspilerCtx *ctx);

bool transpiler_active_has_main_function(const TranspilerCtx *ctx);

const char *transpiler_active_main_function_name(const TranspilerCtx *ctx);

bool transpiler_active_has_top_level_exec(const TranspilerCtx *ctx);

bool transpiler_active_uses_intent_observability(const TranspilerCtx *ctx);

bool transpiler_active_uses_thread_pool(const TranspilerCtx *ctx);

bool transpiler_active_can_emit_intent_cleanup_from_mir(
    const TranspilerCtx *ctx,
    const ASTNode *intent_decl);

#endif /* PERGYRA_TRANSPILER_INVENTORY_VIEW_H */
