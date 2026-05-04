typedef struct
{
    const MIRRoutine *routines;
    size_t            count;
} TranspilerMIRRoutineInventory;

static inline void
transpiler_active_routine_inventory(const TranspilerCtx *ctx,
                                    TranspilerMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (ctx != NULL && ctx->mir != NULL) {
        inventory->routines = ctx->mir->routines;
        inventory->count = ctx->mir->routine_count;
    }
}

static inline void
transpiler_mir_routine_inventory_from_program(
    const MIRProgram *mir,
    TranspilerMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (mir != NULL) {
        inventory->routines = mir->routines;
        inventory->count = mir->routine_count;
    }
}

static inline const MIRRoutine *
transpiler_routine_inventory_get(
    const TranspilerMIRRoutineInventory *inventory,
    size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}

static inline size_t
transpiler_active_routine_count(const TranspilerCtx *ctx)
{
    TranspilerMIRRoutineInventory inventory;
    transpiler_active_routine_inventory(ctx, &inventory);
    return inventory.count;
}

static inline void
transpiler_active_inventory(const TranspilerCtx *ctx,
                            ASTNodeType decl_type,
                            ASTNode ***nodes_out,
                            size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_inventory(ctx->mir, decl_type, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline void
transpiler_active_externs(const TranspilerCtx *ctx,
                          ASTNode ***nodes_out,
                          size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_externs(ctx->mir, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline void
transpiler_active_executables(const TranspilerCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    /* MIR-only: top-level exec is represented by __pgy_top_level_exec. */
    (void)ctx;

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline ASTNode *
transpiler_active_synthetic_executable_func(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    return NULL;
}

static inline bool
transpiler_active_has_main_function(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_main_function;
    return false;
}

static inline bool
transpiler_active_has_top_level_exec(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_top_level_exec;
    return false;
}
