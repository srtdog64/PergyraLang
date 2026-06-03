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

bool
mir_routine_has_signature(const MIRRoutine *routine)
{
    return routine != NULL && routine->has_signature;
}

size_t
mir_routine_param_count(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->param_count : 0;
}

FuncParam *
mir_routine_param(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine) || routine->params == NULL
        || index >= routine->param_count) {
        return NULL;
    }
    return routine->params[index];
}

const char *
mir_routine_param_type_name(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine) || routine->param_type_names == NULL
        || index >= routine->param_count) {
        return NULL;
    }
    return routine->param_type_names[index];
}

ASTNode *
mir_routine_return_type(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->return_type : NULL;
}

const char *
mir_routine_return_type_name(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->return_type_name : NULL;
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
