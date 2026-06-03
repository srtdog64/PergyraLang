#include "mir.h"

bool
mir_program_has_main_function(const MIRProgram *mir)
{
    return mir != NULL && mir->has_main_function;
}

const char *
mir_program_main_function_name(const MIRProgram *mir)
{
    return mir != NULL ? mir->main_function_name : NULL;
}

bool
mir_program_has_top_level_exec(const MIRProgram *mir)
{
    return mir != NULL && mir->has_top_level_exec;
}

void
mir_routine_inventory_from_program(const MIRProgram *mir,
                                   MIRRoutineInventory *inventory)
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

const MIRRoutine *
mir_routine_inventory_get(const MIRRoutineInventory *inventory, size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}

ASTNode *
mir_routine_source_ast(const MIRRoutine *routine)
{
    return routine != NULL ? routine->ast : NULL;
}

void
mir_mutable_routine_inventory_from_program(
        MIRProgram *mir,
        MIRMutableRoutineInventory *inventory)
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

MIRRoutine *
mir_mutable_routine_inventory_get(
        const MIRMutableRoutineInventory *inventory,
        size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}
