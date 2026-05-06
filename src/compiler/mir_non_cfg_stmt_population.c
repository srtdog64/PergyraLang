#include "mir_non_cfg_stmt_population.h"

#include <string.h>

#include "mir_base_helpers.h"
#include "mir_call_fact.h"
#include "mir_stmt_population.h"
#include "mir_type_helpers.h"

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
    routine->used_non_cfg_body_fallback = true;

    func_decl = routine->ast;
    if (func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return true;
    }

    body = func_decl->data.func_decl.body;
    if (body->type != AST_BLOCK) {
        MIRInstruction inst = {
            .id = routine->instruction_count++,
            .kind = MIR_INST_STMT,
            .name = "stmt",
            .ast = body,
            .source_statement_index = 0,
            .has_source_statement_index = true,
        };
        mir_attach_statement_call_fact(&inst, body);
        routine->non_cfg_body_fallback_count++;
        return append_instruction(entry, inst);
    }

    if (mir_block_source_inventory_items(entry) != NULL
        && mir_block_source_inventory_count(entry) > 0) {
        statements = mir_block_source_inventory_items(entry);
        statement_count = mir_block_source_inventory_count(entry);
    } else {
        statements = body->data.block.statements;
        statement_count = body->data.block.count;
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
            MIRInstruction inst = {
                    .id = routine->instruction_count++,
                    .kind = MIR_INST_STMT,
                    .name = "stmt",
                    .ast = stmt,
                    .source_statement_index = i,
                    .has_source_statement_index = true,
            };
            mir_attach_statement_call_fact(&inst, stmt);
            if (!append_instruction(entry, inst)) {
                return false;
            }
            routine->non_cfg_body_fallback_count++;
            continue;
        }
        if (stmt->type == AST_LET_DECL
            && mir_let_decl_requires_stmt_preservation(stmt)) {
            MIRInstruction inst = {
                    .id = routine->instruction_count++,
                    .kind = MIR_INST_STMT,
                    .name = "stmt",
                    .ast = stmt,
                    .source_statement_index = i,
                    .has_source_statement_index = true,
            };
            mir_attach_statement_call_fact(&inst, stmt);
            if (!append_instruction(entry, inst)) {
                return false;
            }
            routine->non_cfg_body_fallback_count++;
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
                matched_def = true;
                break;
            }
            if (matched_def)
                continue;
        }
        MIRInstruction inst = {
                .id = routine->instruction_count++,
                .kind = MIR_INST_STMT,
                .name = "stmt",
                .ast = stmt,
                .source_statement_index = i,
                .has_source_statement_index = true,
        };
        mir_attach_statement_call_fact(&inst, stmt);
        if (!append_instruction(entry, inst)) {
            return false;
        }
        routine->non_cfg_body_fallback_count++;
    }

    return true;
}
