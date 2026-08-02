#include "mir_stmt_population_internal.h"

#include "mir_abi_layout.h"
#include "mir_nominal_abi_layout.h"
#include "mir_resource_runtime_population.h"
#include "mir_source_local_type_shape.h"

#include <stdlib.h>
#include <string.h>

#include "mir_call_fact.h"
#include "mir_cfg_contract_control.h"
#include "mir_decl_headers.h"
#include "mir_type_helpers.h"
#include "../parser/ast_api.h"

static const char *
mir_assignment_target_root_name(const ASTNode *target)
{
    const ASTNode *cursor = target;

    while (cursor != NULL) {
        if (cursor->type == AST_IDENTIFIER)
            return ast_identifier_name(cursor);
        if (cursor->type == AST_MEMBER_ACCESS) {
            cursor = ast_member_object(cursor);
            continue;
        }
        if (cursor->type == AST_ARRAY_ACCESS) {
            cursor = ast_array_access_array(cursor);
            continue;
        }
        return NULL;
    }
    return NULL;
}

static const char *
mir_assignment_parameter_mode_name(ParamMode mode)
{
    switch (mode) {
    case PARAM_MODE_DEFAULT:
        return "default_param";
    case PARAM_MODE_MUT_REF:
        return "inout_param";
    case PARAM_MODE_OWN:
        return "own_param";
    case PARAM_MODE_REF:
        return "ref_param";
    default:
        return NULL;
    }
}

const char *
mir_assignment_target_root_binding_mode(const MIRRoutine *routine,
                                        const ASTNode *target)
{
    const char *root_name = mir_assignment_target_root_name(target);
    const char *parameter_mode = NULL;
    size_t parameter_matches = 0;
    size_t local_matches = 0;
    size_t owner_field_matches = 0;

    if (routine == NULL || root_name == NULL || root_name[0] == '\0')
        return NULL;

    for (size_t i = 0; i < routine->param_count; i++) {
        FuncParam *param = routine->params != NULL ? routine->params[i] : NULL;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, root_name) != 0) {
            continue;
        }
        parameter_matches++;
        parameter_mode = mir_assignment_parameter_mode_name(param->mode);
    }
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        const MIRSourceLocalType *local = routine->source_local_types != NULL
            ? &routine->source_local_types[i]
            : NULL;
        if (local != NULL && local->name != NULL
            && strcmp(local->name, root_name) == 0) {
            local_matches++;
        }
    }

    if (routine->owner_name != NULL && routine->owner_name[0] != '\0') {
        const MIRDeclHeader *owner;

        if (routine->program == NULL)
            return NULL;
        owner = mir_find_decl_header(routine->program, routine->owner_name);
        if (owner == NULL
            || owner->field_count != owner->field_metadata_count
            || (owner->field_metadata_count > 0
                && owner->field_metadata == NULL)) {
            return NULL;
        }
        for (size_t i = 0; i < mir_decl_header_field_count(owner); i++) {
            const MIRDeclField *field = mir_decl_header_field(owner, i);
            const char *field_name = mir_decl_field_name(field);
            if (field_name != NULL && strcmp(field_name, root_name) == 0)
                owner_field_matches++;
        }
    }

    if (parameter_matches + local_matches + owner_field_matches != 1)
        return NULL;
    if (parameter_matches == 1)
        return parameter_mode;
    if (local_matches == 1)
        return "local";
    return owner_field_matches == 1 ? "owner_field" : NULL;
}

bool
mir_routine_has_def_for_name(const MIRRoutine *routine, const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            const char *def_name = NULL;

            if (inst->kind != MIR_INST_DEF)
                continue;

            def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
            if (def_name != NULL && strcmp(def_name, base_name) == 0)
                return true;
        }
    }

    return false;
}

bool
mir_assignment_requires_stmt_preservation(const MIRRoutine *routine,
                                          const ASTNode *stmt)
{
    char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
    const char *target_name;
    const char *type_name;

    if (routine == NULL || stmt == NULL || stmt->type != AST_ASSIGNMENT
        || ast_assignment_target(stmt) == NULL
        || ast_assignment_target(stmt)->type != AST_IDENTIFIER) {
        return false;
    }
    target_name = ast_identifier_name(ast_assignment_target(stmt));
    if (target_name == NULL)
        return false;
    type_name = mir_routine_source_local_type_name(routine, target_name);
    return mir_source_local_unwrap_slot_like_type(type_name, inner,
                                                   sizeof(inner));
}

void
mir_consume_matching_def_instruction(MIRInstruction *old_insts,
                                     size_t old_count,
                                     size_t *def_cursor,
                                     bool *copied_flags,
                                     const char *base_name)
{
    if (old_insts == NULL || def_cursor == NULL || copied_flags == NULL
        || base_name == NULL) {
        return;
    }

    while (*def_cursor < old_count) {
        MIRInstruction *inst = &old_insts[*def_cursor];
        const char *def_name;

        if (inst->kind != MIR_INST_DEF) {
            (*def_cursor)++;
            continue;
        }

        def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
        if (def_name != NULL && strcmp(def_name, base_name) == 0) {
            copied_flags[*def_cursor] = true;
            (*def_cursor)++;
        }
        return;
    }
}

bool
mir_stmt_is_for_loop_init_payload(const ASTNode *stmt,
                                  const MIRBasicBlock *mir_block)
{
    return stmt != NULL
        && stmt->type == AST_FOR_LOOP
        && mir_block != NULL
        && (mir_block->has_succ_true || mir_block->has_succ_false);
}

bool
mir_stmt_is_inline_cfg_wrapper(const ASTNode *stmt)
{
    return stmt != NULL
        && (stmt->type == AST_WITH_STMT
            || stmt->type == AST_UNSAFE_BLOCK
            || stmt->type == AST_TRANSACTION_BLOCK);
}

bool
mir_stmt_population_is_semantic_carrier(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->name == NULL)
        return false;
    return mir_instruction_is_intent_semantic_carrier(inst);
}

MIRInstruction
mir_make_source_stmt_instruction(MIRRoutine *routine,
                                 ASTNode *stmt,
                                 size_t source_statement_index)
{
    MIRInstruction inst;

    if (stmt != NULL && stmt->type == AST_ASSIGNMENT)
        return mir_make_assignment_instruction(routine, stmt,
            source_statement_index);
    if (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE)
        return mir_make_destructure_instruction(routine, stmt,
            source_statement_index);

    memset(&inst, 0, sizeof(inst));
    if (routine != NULL)
        inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_STMT;
    inst.name = "stmt";
    inst.ast = stmt;
    mir_instruction_capture_source_provenance(&inst, stmt);
    if (stmt != NULL && stmt->type == AST_BIND_STMT) {
        inst.name = "bind";
        inst.arg0 = ast_bind_statement_party_var(stmt);
        inst.slot_anchor = ast_bind_statement_slot_name(stmt);
        inst.arg1 = ast_bind_statement_role_name(stmt);
    }
    mir_attach_statement_call_fact(&inst, stmt);
    mir_set_inst_source_statement_fact(&inst, stmt, source_statement_index);
    return inst;
}

MIRInstruction
mir_make_destructure_instruction(MIRRoutine *routine,
                                 ASTNode *stmt,
                                 size_t source_statement_index)
{
    MIRInstruction inst;

    memset(&inst, 0, sizeof(inst));
    if (routine != NULL)
        inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_DESTRUCTURE;
    inst.name = "destructure";
    inst.ast = stmt;
    mir_instruction_capture_source_provenance(&inst, stmt);
    if (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE) {
        size_t name_count = ast_let_destructure_name_count(stmt);
        inst.expr0 = ast_let_destructure_initializer(stmt);
        if (routine != NULL) {
            char *claim_type_name = mir_claim_abi_type_name_from_ast(inst.expr0);
            if (claim_type_name != NULL) {
                inst.abi_type_name =
                    pgy_arena_strdup(&routine->scratch, claim_type_name);
                if (inst.abi_type_name != NULL) {
                    inst.type_layout = mir_abi_lookup(inst.abi_type_name);
                    inst.abi_layout_id = mir_abi_layout_id(inst.type_layout);
                    (void)mir_materialize_resource_runtime_fact(routine, &inst);
                }
                free(claim_type_name);
            }
        }
        if (name_count > 0) {
            inst.destructure_binding_names =
                calloc(name_count, sizeof(const char *));
            if (inst.destructure_binding_names != NULL) {
                for (size_t i = 0; i < name_count; i++) {
                    inst.destructure_binding_names[i] =
                        ast_let_destructure_name(stmt, i);
                }
                inst.destructure_binding_count = name_count;
            }
            inst.destructure_element_type_name =
                mir_routine_source_local_type_name(
                    routine, ast_let_destructure_name(stmt, 0));
        }
    }
    mir_attach_statement_call_fact(&inst, stmt);
    mir_set_inst_source_statement_fact(&inst, stmt, source_statement_index);
    return inst;
}

MIRInstruction
mir_make_assignment_instruction(MIRRoutine *routine,
                                ASTNode *stmt,
                                size_t source_statement_index)
{
    MIRInstruction inst;

    memset(&inst, 0, sizeof(inst));
    if (routine != NULL)
        inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_ASSIGN;
    inst.name = "assign";
    inst.ast = stmt;
    mir_instruction_capture_source_provenance(&inst, stmt);
    if (stmt != NULL && stmt->type == AST_ASSIGNMENT) {
        const char *target_name;

        inst.expr0 = ast_assignment_target(stmt);
        inst.expr1 = ast_assignment_value(stmt);
        inst.arg1 = mir_assignment_target_root_binding_mode(routine,
                                                            inst.expr0);
        target_name = mir_assignment_target_root_name(inst.expr0);
        if (routine != NULL && target_name != NULL) {
            const char *type_name = mir_routine_source_local_type_name(
                routine, target_name);
            if (type_name != NULL) {
                inst.abi_type_name = pgy_arena_strdup(
                    &routine->scratch, type_name);
                inst.type_layout = mir_program_abi_layout_for_type_name(
                    routine->program, inst.abi_type_name);
                inst.abi_layout_id = mir_abi_layout_id(inst.type_layout);
            }
        }
    }
    mir_attach_statement_call_fact(&inst, stmt);
    mir_set_inst_source_statement_fact(&inst, stmt, source_statement_index);
    return inst;
}

MIRInstruction
mir_make_loop_init_instruction(MIRRoutine *routine,
                               ASTNode *stmt,
                               size_t source_statement_index)
{
    MIRInstruction inst;

    memset(&inst, 0, sizeof(inst));
    if (routine != NULL)
        inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_LOOP_INIT;
    inst.name = "loop-init";
    inst.ast = stmt;
    mir_instruction_capture_source_provenance(&inst, stmt);
    inst.arg0 = ast_for_variable(stmt);
    inst.branch_shape = ast_for_iterable(stmt) != NULL
        ? MIR_BRANCH_FOR_IN
        : MIR_BRANCH_FOR_RANGE;
    mir_set_inst_source_statement_fact(&inst, stmt, source_statement_index);
    if (ast_for_iterable(stmt) != NULL) {
        inst.expr0 = ast_for_iterable(stmt);
        inst.expr1 = ast_for_iterable(stmt);
    } else {
        inst.expr0 = ast_for_range_start(stmt);
        inst.expr1 = ast_for_range_end(stmt);
    }
    return inst;
}

ASTNode *
mir_block_source_inventory_at(const MIRBasicBlock *block, size_t index)
{
    if (block == NULL
        || block->source_statement_inventory.items == NULL
        || index >= block->source_statement_inventory.count) {
        return NULL;
    }
    return block->source_statement_inventory.items[index];
}
