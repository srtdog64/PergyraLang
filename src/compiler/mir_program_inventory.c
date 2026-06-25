#include <string.h>

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

void
mir_decl_header_inventory_from_program(const MIRProgram *mir,
                                       MIRDeclHeaderInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->headers = NULL;
    inventory->count = 0;
    if (mir != NULL) {
        inventory->headers = mir->decl_headers;
        inventory->count = mir->decl_header_count;
    }
}

const MIRDeclHeader *
mir_decl_header_inventory_get(const MIRDeclHeaderInventory *inventory,
                              size_t index)
{
    if (inventory == NULL || inventory->headers == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->headers[index];
}

ASTNode *
mir_routine_source_decl(const MIRRoutine *routine)
{
    return routine != NULL ? routine->ast : NULL;
}

ASTNode *
mir_routine_source_decl_of_type(const MIRRoutine *routine,
                                MIRScopeKind expected_kind,
                                ASTNodeType expected_ast_type)
{
    ASTNode *source = mir_routine_source_decl(routine);

    if (routine == NULL || mir_routine_kind(routine) != expected_kind)
        return NULL;
    if (source == NULL || source->type != expected_ast_type)
        return NULL;
    return source;
}

MIRScopeKind
mir_routine_kind(const MIRRoutine *routine)
{
    return routine != NULL ? routine->kind : MIR_SCOPE_FUNCTION;
}

const char *
mir_routine_name(const MIRRoutine *routine)
{
    return routine != NULL ? routine->name : NULL;
}

const char *
mir_routine_owner_name(const MIRRoutine *routine)
{
    return routine != NULL ? routine->owner_name : NULL;
}

ASTNodeType
mir_routine_owner_ast_type(const MIRRoutine *routine)
{
    return routine != NULL ? routine->owner_ast_type : AST_PROGRAM;
}

bool
mir_routine_has_signature(const MIRRoutine *routine)
{
    return routine != NULL && routine->has_signature;
}

size_t
mir_routine_generic_param_count(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine)
        ? routine->generic_param_count
        : 0;
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

/* Row 607: lossless callable (EventHandler) signature for param `index`, or
   NULL when the param is not a callable. */
const MIRCallableSig *
mir_routine_param_callable_sig(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine)
        || routine->param_callable_sigs == NULL
        || index >= routine->param_count
        || !routine->param_callable_sigs[index].is_callable) {
        return NULL;
    }
    return &routine->param_callable_sigs[index];
}

/* Row 607: lossless callable signature for the return type, or NULL. */
const MIRCallableSig *
mir_routine_return_callable_sig(const MIRRoutine *routine)
{
    if (!mir_routine_has_signature(routine)
        || !routine->return_callable_sig.is_callable) {
        return NULL;
    }
    return &routine->return_callable_sig;
}

const char *
mir_routine_source_local_type_name(const MIRRoutine *routine,
                                   const char *local_name)
{
    const MIRSourceLocalType *fact =
        mir_routine_source_local_type_fact(routine, local_name);
    return fact != NULL ? fact->type_name : NULL;
}

const MIRSourceLocalType *
mir_routine_source_local_type_fact(const MIRRoutine *routine,
                                   const char *local_name)
{
    if (routine == NULL || local_name == NULL)
        return NULL;
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        const MIRSourceLocalType *entry = &routine->source_local_types[i];
        if (entry->name != NULL && entry->type_name != NULL
            && strcmp(entry->name, local_name) == 0) {
            return entry;
        }
    }
    return NULL;
}

size_t
mir_routine_source_local_type_count(const MIRRoutine *routine)
{
    return routine != NULL ? routine->source_local_type_count : 0;
}

const char *
mir_routine_source_local_name_at(const MIRRoutine *routine, size_t index)
{
    if (routine == NULL || routine->source_local_types == NULL
        || index >= routine->source_local_type_count) {
        return NULL;
    }
    return routine->source_local_types[index].name;
}

const char *
mir_routine_source_local_type_name_at(const MIRRoutine *routine, size_t index)
{
    if (routine == NULL || routine->source_local_types == NULL
        || index >= routine->source_local_type_count) {
        return NULL;
    }
    return routine->source_local_types[index].type_name;
}

const char *
mir_routine_within_zone(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->within_zone : NULL;
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
