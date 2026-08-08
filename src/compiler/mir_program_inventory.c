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

MIRRoutineSourceLookup
mir_routine_inventory_find_unique_by_source_syntax_id(
    const MIRRoutineInventory *inventory,
    uint32_t source_syntax_id)
{
    MIRRoutineSourceLookup result;

    result.status = MIR_ROUTINE_SOURCE_LOOKUP_INVALID;
    result.routine = NULL;
    if (inventory == NULL || source_syntax_id == 0)
        return result;
    result.status = MIR_ROUTINE_SOURCE_LOOKUP_MISSING;
    for (size_t i = 0; i < inventory->count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(inventory, i);
        if (mir_routine_source_syntax_id(routine) != source_syntax_id)
            continue;
        if (result.routine != NULL) {
            result.status = MIR_ROUTINE_SOURCE_LOOKUP_DUPLICATE;
            result.routine = NULL;
            return result;
        }
        result.status = MIR_ROUTINE_SOURCE_LOOKUP_UNIQUE;
        result.routine = routine;
    }
    return result;
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

uint32_t
mir_routine_source_syntax_id(const MIRRoutine *routine)
{
    return routine != NULL ? routine->source_syntax_id : 0;
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

MIRReceiverCarriage
mir_routine_receiver_carriage(const MIRRoutine *routine)
{
    return routine != NULL
        ? routine->receiver_carriage
        : MIR_RECEIVER_CARRIAGE_NONE;
}

const char *
mir_receiver_carriage_name(MIRReceiverCarriage carriage)
{
    switch (carriage) {
    case MIR_RECEIVER_CARRIAGE_NONE:
        return "none";
    case MIR_RECEIVER_CARRIAGE_VALUE:
        return "value";
    case MIR_RECEIVER_CARRIAGE_MUTABLE_IDENTITY:
        return "mutable-identity";
    }
    return "unknown";
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

const char *
mir_routine_generic_param_name(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine)
        || routine->generic_param_names == NULL
        || index >= routine->generic_param_count) {
        return NULL;
    }
    return routine->generic_param_names[index];
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

MIRParamCarriage
mir_routine_param_carriage(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine)
        || routine->param_abi_facts == NULL
        || index >= routine->param_count) {
        return MIR_PARAM_CARRIAGE_VALUE;
    }
    return routine->param_abi_facts[index].carriage;
}

MIRParamResourceKind
mir_routine_param_resource_kind(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine)
        || routine->param_abi_facts == NULL
        || index >= routine->param_count) {
        return MIR_PARAM_RESOURCE_NONE;
    }
    return routine->param_abi_facts[index].resource_kind;
}

bool
mir_routine_param_passes_indirect(const MIRRoutine *routine, size_t index)
{
    return mir_routine_has_signature(routine)
        && routine->param_abi_facts != NULL
        && index < routine->param_count
        && routine->param_abi_facts[index].pass_indirect;
}

const MIRTypeLayout *
mir_routine_param_abi_layout(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine)
        || routine->param_abi_facts == NULL
        || index >= routine->param_count) {
        return NULL;
    }
    return routine->param_abi_facts[index].type_layout;
}

uint32_t
mir_routine_param_abi_layout_id(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine)
        || routine->param_abi_facts == NULL
        || index >= routine->param_count) {
        return 0;
    }
    return routine->param_abi_facts[index].abi_layout_id;
}

const char *
mir_param_resource_kind_name(MIRParamResourceKind kind)
{
    switch (kind) {
    case MIR_PARAM_RESOURCE_SLOT:
        return "slot";
    case MIR_PARAM_RESOURCE_SECURE_SLOT:
        return "secure-slot";
    case MIR_PARAM_RESOURCE_DEVICE_SLOT:
        return "device-slot";
    case MIR_PARAM_RESOURCE_NONE:
    default:
        return "none";
    }
}

const char *
mir_param_carriage_name(MIRParamCarriage carriage)
{
    switch (carriage) {
    case MIR_PARAM_CARRIAGE_READONLY_REF:
        return "readonly-ref";
    case MIR_PARAM_CARRIAGE_VALUE_RESULT:
        return "value-result";
    case MIR_PARAM_CARRIAGE_OWNER_HANDLE:
        return "owner-handle";
    case MIR_PARAM_CARRIAGE_VALUE:
    default:
        return "value";
    }
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

bool
mir_routine_has_admitted_intent_execution_plan(const MIRRoutine *routine)
{
    return routine != NULL && routine->intent_execution_plan_admitted;
}

uint32_t
mir_routine_intent_execution_plan_digest(const MIRRoutine *routine)
{
    return mir_routine_has_admitted_intent_execution_plan(routine)
        ? routine->intent_execution_plan_digest : 0;
}

size_t
mir_routine_intent_step_transition_count(const MIRRoutine *routine)
{
    return mir_routine_has_admitted_intent_execution_plan(routine)
        ? routine->intent_step_transition_count : 0;
}

const MIRIntentStepTransitionFact *
mir_routine_intent_step_transition_at(const MIRRoutine *routine,
                                      size_t index)
{
    return mir_routine_has_admitted_intent_execution_plan(routine)
        && routine->intent_step_transitions != NULL
        && index < routine->intent_step_transition_count
            ? &routine->intent_step_transitions[index] : NULL;
}

size_t
mir_routine_intent_terminal_transition_count(const MIRRoutine *routine)
{
    return mir_routine_has_admitted_intent_execution_plan(routine)
        ? routine->intent_terminal_transition_count : 0;
}

const MIRIntentTerminalTransitionFact *
mir_routine_intent_terminal_transition_at(const MIRRoutine *routine,
                                          size_t index)
{
    return mir_routine_has_admitted_intent_execution_plan(routine)
        && routine->intent_terminal_transitions != NULL
        && index < routine->intent_terminal_transition_count
            ? &routine->intent_terminal_transitions[index] : NULL;
}

const char *
mir_intent_terminal_role_name(MIRIntentTerminalRole role)
{
    if (role == MIR_INTENT_TERMINAL_SUCCESS)
        return "success";
    if (role == MIR_INTENT_TERMINAL_FAILURE)
        return "failure";
    return "unknown";
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
