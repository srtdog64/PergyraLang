#include "mir_stmt_population_internal.h"

#include "mir_abi_layout.h"

#include <stdlib.h>
#include <string.h>

#include "mir_call_fact.h"
#include "mir_cfg_contract_control.h"
#include "mir_type_helpers.h"
#include "../parser/ast_api.h"

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
    mir_set_inst_source_statement_index(&inst, source_statement_index);
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
                if (inst.abi_type_name != NULL)
                    inst.type_layout = mir_abi_lookup(inst.abi_type_name);
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
    mir_set_inst_source_statement_index(&inst, source_statement_index);
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
        inst.expr0 = ast_assignment_target(stmt);
        inst.expr1 = ast_assignment_value(stmt);
    }
    mir_attach_statement_call_fact(&inst, stmt);
    mir_set_inst_source_statement_index(&inst, source_statement_index);
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
    mir_set_inst_source_statement_index(&inst, source_statement_index);
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
