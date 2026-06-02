#include "mir_non_cfg_stmt_population.h"

#include <string.h>

#include "mir_base_helpers.h"
#include "mir_call_fact.h"
#include "mir_stmt_population.h"
#include "mir_stmt_population_internal.h"
#include "mir_type_helpers.h"

static void
mir_record_non_cfg_body_fallback(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    routine->used_non_cfg_body_fallback = true;
    routine->non_cfg_body_fallback_count++;
}

static bool
mir_commit_non_cfg_instruction(MIRRoutine *routine, MIRBasicBlock *block, MIRInstruction *inst)
{
    if (routine == NULL || block == NULL || inst == NULL)
        return false;
    inst->id = routine->instruction_count;
    if (!append_instruction(block, *inst))
        return false;
    routine->instruction_count++;
    return true;
}

static bool
mir_append_non_cfg_source_statement(MIRRoutine *routine,
                                    MIRBasicBlock *block,
                                    ASTNode *stmt,
                                    size_t source_index)
{
    MIRInstruction inst = {
        .kind = MIR_INST_STMT,
        .name = "stmt",
        .ast = stmt,
        .source_statement_index = source_index,
        .has_source_statement_index = true,
    };

    mir_attach_statement_call_fact(&inst, stmt);
    if (!mir_commit_non_cfg_instruction(routine, block, &inst))
        return false;
    mir_record_non_cfg_body_fallback(routine);
    return true;
}

bool
mir_append_non_cfg_body_statements(MIRRoutine *routine, MIRBasicBlock *entry)
{
    ASTNode *func_decl;
    ASTNode *body;
    ASTNode **statements = NULL;
    size_t statement_count = 0;

    if (routine == NULL || entry == NULL || routine->ast == NULL)
        return true;
    if (routine->hir_routine != NULL && routine->hir_routine->has_cfg)
        return false;

    func_decl = routine->ast;
    if (func_decl->type != AST_FUNC_DECL
        || ast_func_body(func_decl) == NULL) {
        return true;
    }

    body = ast_func_body(func_decl);
    if (body->type != AST_BLOCK) {
        if (!mir_append_non_cfg_source_statement(routine, entry, body, 0))
            return false;
        return true;
    }

    if (mir_block_source_inventory_items(entry) != NULL
        && mir_block_source_inventory_count(entry) > 0) {
        statements = mir_block_source_inventory_items(entry);
        statement_count = mir_block_source_inventory_count(entry);
    } else {
        statements = ast_block_statements(body, &statement_count);
    }

    for (size_t i = 0; i < statement_count; i++) {
        ASTNode *stmt = statements[i];
        bool matched_def = false;
        if (stmt == NULL)
            continue;
        if (mir_stmt_is_control_flow(stmt, entry))
            continue;
        if (mir_assignment_requires_stmt_preservation(func_decl,
                                                      statements,
                                                      statement_count,
                                                      i,
                                                      stmt)) {
            MIRInstruction inst = mir_make_assignment_instruction(NULL,
                                                                  stmt,
                                                                  i);
            if (!mir_commit_non_cfg_instruction(routine, entry, &inst)) {
                return false;
            }
            mir_record_non_cfg_body_fallback(routine);
            continue;
        }
        if (mir_stmt_requires_source_local_preservation(stmt)) {
            if (!mir_append_non_cfg_source_statement(routine, entry, stmt, i)) {
                return false;
            }
            continue;
        }
        if (stmt->type == AST_LET_DESTRUCTURE) {
            MIRInstruction inst = mir_make_destructure_instruction(NULL,
                                                                   stmt,
                                                                   i);
            if (!mir_commit_non_cfg_instruction(routine, entry, &inst))
                return false;
            mir_record_non_cfg_body_fallback(routine);
            continue;
        }
        if (mir_stmt_is_def_source(stmt)) {
            const char *stmt_name = mir_stmt_def_name(stmt);
            for (size_t j = 0; j < entry->instruction_count; j++) {
                MIRInstruction *inst = &entry->instructions[j];
                const char *def_name;
                if (inst->kind != MIR_INST_DEF)
                    continue;
                def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
                if (stmt_name == NULL || def_name == NULL
                    || strcmp(stmt_name, def_name) != 0) {
                    continue;
                }
                if (inst->ast == NULL)
                    inst->ast = stmt;
                mir_attach_def_initializer_call_fact(inst, stmt);
                mir_set_inst_source_statement_index(inst, i);
                mir_mark_select_receive_statement_emit(entry, inst);
                matched_def = true;
                break;
            }
            if (matched_def)
                continue;
        }
        if (stmt->type == AST_ASSIGNMENT) {
            MIRInstruction inst = mir_make_assignment_instruction(NULL,
                                                                  stmt,
                                                                  i);
            if (!mir_commit_non_cfg_instruction(routine, entry, &inst))
                return false;
            mir_record_non_cfg_body_fallback(routine);
            continue;
        }
        if (!mir_append_non_cfg_source_statement(routine, entry, stmt, i)) {
            return false;
        }
    }

    return true;
}
