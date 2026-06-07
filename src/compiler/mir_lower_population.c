#include "mir_lower_population.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "mir_abi_layout.h"
#include "mir_base_helpers.h"
#include "mir_cleanup.h"
#include "mir_intent.h"
#include "mir_non_cfg_stmt_population.h"
#include "mir_stmt_population.h"
#include "mir_type_helpers.h"

static ASTNode *
mir_resource_write_value_expr_from_call(ASTNode *call)
{
    ASTNode *callee;

    if (call == NULL || call->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(call);
    if (callee == NULL)
        return NULL;
    if (callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL
        && strcmp(ast_identifier_name(callee), "Write") == 0
        && ast_call_arg_count(call) >= 2) {
        return ast_call_argument(call, 1);
    }
    if (callee->type == AST_MEMBER_ACCESS
        && ast_member_name(callee) != NULL
        && strcmp(ast_member_name(callee), "Write") == 0
        && ast_call_arg_count(call) >= 1) {
        return ast_call_argument(call, 0);
    }
    return NULL;
}

static bool
mir_add_resource_instruction(MIRRoutine *routine,
                             MIRBasicBlock *block,
                             const RIROp *op)
{
    MIRInstruction inst;
    char *claim_type_name = NULL;
    const char *abi_type_name = NULL;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_RESOURCE_OP;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    if (op->kind == RIR_OP_WRITE)
        inst.expr0 = mir_resource_write_value_expr_from_call(op->ast);
    if (op->kind == RIR_OP_CLAIM)
        claim_type_name = mir_claim_abi_type_name_from_ast(op->ast);
    if (op->kind == RIR_OP_AWAIT_LOCAL) {
        abi_type_name = "Future";
    } else if (op->kind == RIR_OP_AWAIT_REMOTE) {
        abi_type_name = "RemoteFuture";
    } else {
        abi_type_name = claim_type_name != NULL
            ? claim_type_name
            : (op->arg0 != NULL ? op->arg0 : op->subject);
    }
    inst.type_layout = mir_abi_lookup(abi_type_name);
    free(claim_type_name);
    return mir_commit_instruction(routine, block, &inst);
}

bool
mir_populate_instructions(MIRRoutine *routine)
{
    const RIRScope *rir_scope;
    MIRBasicBlock *entry;
    MIRBasicBlock *rollback;
    MIRBasicBlock *invalidation;
    bool appended_intent_steps = false;

    if (routine == NULL || routine->block_count == 0)
        return true;

    rir_scope = routine->rir_scope;
    entry = &routine->blocks[routine->entry_block];
    rollback = routine->has_rollback_block ? &routine->blocks[routine->rollback_block] : NULL;
    invalidation = routine->has_invalidation_block ? &routine->blocks[routine->invalidation_block] : NULL;

    if (routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL) {
        if (!mir_append_intent_step_instructions(routine, entry))
            return false;
        appended_intent_steps = true;
    }

    if (rir_scope == NULL)
        return true;

    for (size_t i = 0; i < rir_scope_op_count(rir_scope); i++) {
        const RIROp *op = rir_scope_op_at(rir_scope, i);
        if (op == NULL)
            continue;
        switch (op->kind) {
        case RIR_OP_ABORT_INTENT:
        case RIR_OP_COMPENSATE_INTENT_STEP:
            if (rollback != NULL) {
                if (!mir_add_cleanup_instruction(routine, rollback, op))
                    return false;
                break;
            }
            /* fallthrough */
        default:
            if (!mir_add_resource_instruction(routine, entry, op))
                return false;
            break;
        }
    }

    if (invalidation != NULL) {
        for (size_t i = 0; i < rir_scope_fact_count(rir_scope); i++) {
            const RIRFact *fact = rir_scope_fact_at(rir_scope, i);
            MIRInstruction inst;
            if (fact == NULL)
                continue;
            if (fact->kind != RIR_FACT_PROJECTION
                && fact->resource_kind != RIR_RESOURCE_EFFECT_INSTANCE
                && fact->resource_kind != RIR_RESOURCE_RELATION_INSTANCE
                && fact->resource_kind != RIR_RESOURCE_ZONE_HANDLE) {
                continue;
            }
            memset(&inst, 0, sizeof(inst));
            inst.kind = MIR_INST_CLEANUP_EDGE;
            inst.name = "DetachInvalidation";
            inst.slot_anchor = fact->slot_anchor != NULL
                ? fact->slot_anchor
                : fact->name;
            inst.arg0 = fact->name;
            inst.arg1 = rir_resource_kind_name(fact->resource_kind);
            inst.ast = fact->ast;
            if (!mir_commit_instruction(routine, invalidation, &inst))
                return false;
            routine->cleanup_instruction_count++;
        }
        if (!mir_append_intent_invalidation_markers(routine, invalidation))
            return false;
    }

    if (!appended_intent_steps && routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL) {
        if (!mir_append_intent_step_instructions(routine, entry))
            return false;
    } else if (routine->hir_routine != NULL
               && !routine->hir_routine->has_cfg) {
        if (!mir_append_non_cfg_body_statements(routine, entry))
            return false;
    }

    return true;
}
