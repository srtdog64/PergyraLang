#include "mir_lower_population.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "mir_abi_layout.h"
#include "mir_base_helpers.h"
#include "mir_cleanup.h"
#include "mir_destructure_type_facts.h"
#include "mir_intent.h"
#include "mir_machine_layer.h"
#include "mir_non_cfg_stmt_population.h"
#include "mir_stmt_population.h"
#include "mir_type_helpers.h"
#include "../common/string_compat.h"

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
        && (strcmp(ast_identifier_name(callee), "Write") == 0
            || strcmp(ast_identifier_name(callee), "DeviceWrite") == 0)
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

typedef struct
{
    const char *view_name;
    const char *source_slot;
    const char *source_abi_type_name;
    const MIRTypeLayout *source_layout;
} MIRResourceBorrowLoweringFact;

static const char *
mir_resource_abi_type_name_from_fact(MIRRoutine *routine, const RIRFact *fact)
{
    ASTNode *type_node = NULL;
    char *type_name = NULL;
    const char *result = NULL;

    if (routine == NULL || fact == NULL)
        return NULL;
    if (fact->ast == NULL)
        return NULL;
    if (fact->ast->type == AST_WITH_STMT) {
        type_name = mir_claim_abi_type_name_from_ast(fact->ast);
    } else if (fact->ast->type == AST_LET_DECL) {
        type_node = ast_let_type(fact->ast);
        if (type_node != NULL)
            type_name = mir_render_type_name(type_node);
        if (type_name == NULL)
            type_name = mir_claim_abi_type_name_from_ast(
                ast_let_initializer(fact->ast));
    } else if (fact->ast->type == AST_LET_DESTRUCTURE
               && fact->name != NULL) {
        const uint32_t destructure_id = ast_node_stable_id(fact->ast);
        const size_t name_count = ast_let_destructure_name_count(fact->ast);
        for (size_t i = 0; i < name_count; i++) {
            const char *binding_name = ast_let_destructure_name(fact->ast, i);
            const MIRDestructureTypeFact *binding_fact;
            if (binding_name == NULL
                || strcmp(binding_name, fact->name) != 0)
                continue;
            binding_fact = mir_routine_destructure_type_fact(
                routine, destructure_id, i);
            if (binding_fact != NULL
                && binding_fact->binding_type_name != NULL)
                type_name = pergyra_strdup(binding_fact->binding_type_name);
            break;
        }
    }
    else if (fact->ast->type == AST_CLASS_DECL && fact->name != NULL) {
        /* Field-slot resource fact: the owning class node is carried as the
         * fact `ast`; recover the named field's type so a method's
         * Write/Read/Release on `self->field` renders the full
         * `SecureSlot<Int>` ABI name the runtime layout lookup expects. */
        size_t field_count = 0;
        ClassField **fields = ast_class_fields(fact->ast, &field_count);
        for (size_t fi = 0; fi < field_count; fi++) {
            ClassField *field = fields != NULL ? fields[fi] : NULL;
            if (field == NULL || field->name == NULL)
                continue;
            if (strcmp(field->name, fact->name) == 0) {
                if (field->type != NULL)
                    type_name = mir_render_type_name(field->type);
                break;
            }
        }
    }
    else if (fact->ast->type == AST_FUNC_DECL && fact->name != NULL) {
        /* Closure #85: function-parameter resource facts carry the
         * function AST as their `ast`, not a let/with. Walk the param
         * list to recover the parameter's type node so pin-on-param
         * (pin_secure_param_read_view_block) can lower its view-backed
         * Write/Read to a typed runtime layout. */
        for (size_t pi = 0; pi < ast_func_param_count(fact->ast); pi++) {
            FuncParam *param = ast_func_param(fact->ast, pi);
            if (param == NULL || param->name == NULL)
                continue;
            if (strcmp(param->name, fact->name) == 0) {
                type_node = param->type;
                break;
            }
        }
        if (type_node != NULL)
            type_name = mir_render_type_name(type_node);
    }
    if (type_name != NULL)
        result = pgy_arena_strdup(&routine->scratch, type_name);
    else if (fact->arg0 != NULL)
        result = pgy_arena_strdup(&routine->scratch, fact->arg0);
    free(type_name);
    return result;
}

static const MIRTypeLayout *
mir_resource_layout_from_fact(MIRRoutine *routine,
                              const RIRFact *fact,
                              const char **abi_type_name_out)
{
    const char *abi_type_name;

    if (abi_type_name_out != NULL)
        *abi_type_name_out = NULL;
    abi_type_name = mir_resource_abi_type_name_from_fact(routine, fact);
    if (abi_type_name_out != NULL)
        *abi_type_name_out = abi_type_name;
    return mir_abi_lookup(abi_type_name);
}

static const MIRTypeLayout *
mir_resource_layout_for_slot_fact(MIRRoutine *routine,
                                  const RIRScope *rir_scope,
                                  const char *slot_name,
                                  const char **abi_type_name_out)
{
    if (abi_type_name_out != NULL)
        *abi_type_name_out = NULL;
    if (rir_scope == NULL || slot_name == NULL || slot_name[0] == '\0')
        return NULL;
    for (size_t i = 0; i < rir_scope_fact_count(rir_scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(rir_scope, i);
        if (fact == NULL || fact->kind != RIR_FACT_RESOURCE
            || fact->arg0 == NULL) {
            continue;
        }
        if ((fact->name != NULL && strcmp(fact->name, slot_name) == 0)
            || (fact->slot_anchor != NULL
                && strcmp(fact->slot_anchor, slot_name) == 0)) {
            return mir_resource_layout_from_fact(routine, fact,
                abi_type_name_out);
        }
    }
    return NULL;
}

static const MIRTypeLayout *
mir_resource_layout_for_prior_claim(MIRRoutine *routine,
                                    const RIRScope *rir_scope,
                                    size_t op_index,
                                    const char *slot_name,
                                    const char **abi_type_name_out)
{
    char *type_name = NULL;
    const char *abi_type_name = NULL;

    if (abi_type_name_out != NULL)
        *abi_type_name_out = NULL;
    if (routine == NULL || rir_scope == NULL
        || slot_name == NULL || slot_name[0] == '\0')
        return NULL;
    for (size_t i = op_index; i > 0; i--) {
        const RIROp *claim = rir_scope_op_at(rir_scope, i - 1);
        if (claim == NULL || claim->kind != RIR_OP_CLAIM)
            continue;
        if ((claim->subject == NULL || strcmp(claim->subject, slot_name) != 0)
            && (claim->slot_anchor == NULL
                || strcmp(claim->slot_anchor, slot_name) != 0)) {
            continue;
        }
        type_name = mir_claim_abi_type_name_from_ast(claim->ast);
        if (type_name != NULL)
            abi_type_name = pgy_arena_strdup(&routine->scratch, type_name);
        else if (claim->arg0 != NULL)
            abi_type_name = pgy_arena_strdup(&routine->scratch, claim->arg0);
        free(type_name);
        if (abi_type_name_out != NULL)
            *abi_type_name_out = abi_type_name;
        return mir_abi_lookup(abi_type_name);
    }
    return NULL;
}

static const MIRTypeLayout *
mir_resource_layout_for_prior_destructure(const MIRRoutine *routine,
                                          const char *slot_name,
                                          const char **abi_type_name_out)
{
    if (abi_type_name_out != NULL)
        *abi_type_name_out = NULL;
    if (routine == NULL || slot_name == NULL || slot_name[0] == '\0')
        return NULL;
    for (size_t bi = routine->block_count; bi > 0; bi--) {
        const MIRBasicBlock *block = &routine->blocks[bi - 1];
        for (size_t ii = block->instruction_count; ii > 0; ii--) {
            const MIRInstruction *inst = &block->instructions[ii - 1];
            if (inst->kind != MIR_INST_DESTRUCTURE
                || inst->abi_type_name == NULL)
                continue;
            for (size_t di = 0;
                 di < mir_instruction_destructure_binding_count(inst); di++) {
                const char *binding =
                    mir_instruction_destructure_binding_name_at(inst, di);
                if (binding == NULL || strcmp(binding, slot_name) != 0)
                    continue;
                if (abi_type_name_out != NULL)
                    *abi_type_name_out = inst->abi_type_name;
                return inst->type_layout;
            }
        }
    }
    return NULL;
}

static const MIRTypeLayout *
mir_resource_layout_for_source_local(const MIRRoutine *routine,
                                     const char *slot_name,
                                     const char **abi_type_name_out)
{
    if (abi_type_name_out != NULL)
        *abi_type_name_out = NULL;
    if (routine == NULL || slot_name == NULL || slot_name[0] == '\0')
        return NULL;
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        const MIRSourceLocalType *local = &routine->source_local_types[i];
        if (local->name == NULL || local->type_name == NULL
            || strcmp(local->name, slot_name) != 0)
            continue;
        if (strncmp(local->type_name, "Slot<", 5) != 0
            && strncmp(local->type_name, "SecureSlot<", 11) != 0
            && strncmp(local->type_name, "DeviceSlot<", 11) != 0)
            continue;
        if (abi_type_name_out != NULL)
            *abi_type_name_out = local->type_name;
        return mir_abi_lookup(local->type_name);
    }
    return NULL;
}

static const MIRResourceBorrowLoweringFact *
mir_resource_borrow_fact_for_view(
    const MIRResourceBorrowLoweringFact *facts,
    size_t fact_count,
    const char *view_name)
{
    if (facts == NULL || view_name == NULL || view_name[0] == '\0')
        return NULL;
    for (size_t i = fact_count; i > 0; i--) {
        const MIRResourceBorrowLoweringFact *fact = &facts[i - 1];
        if (fact->view_name != NULL && strcmp(fact->view_name, view_name) == 0)
            return fact;
    }
    return NULL;
}

static void
mir_resource_record_borrow_fact(MIRResourceBorrowLoweringFact *facts,
                                size_t max_facts,
                                size_t *fact_count,
                                const char *view_name,
                                const char *source_slot,
                                const char *source_abi_type_name,
                                const MIRTypeLayout *source_layout)
{
    if (facts == NULL || fact_count == NULL || view_name == NULL
        || source_slot == NULL) {
        return;
    }
    for (size_t i = *fact_count; i > 0; i--) {
        MIRResourceBorrowLoweringFact *fact = &facts[i - 1];
        if (fact->view_name != NULL && strcmp(fact->view_name, view_name) == 0) {
            fact->source_slot = source_slot;
            fact->source_abi_type_name = source_abi_type_name;
            fact->source_layout = source_layout;
            return;
        }
    }
    if (*fact_count >= max_facts)
        return;
    facts[*fact_count].view_name = view_name;
    facts[*fact_count].source_slot = source_slot;
    facts[*fact_count].source_abi_type_name = source_abi_type_name;
    facts[*fact_count].source_layout = source_layout;
    (*fact_count)++;
}

static bool
mir_resource_runtime_operation_has_row(const char *operation)
{
    if (operation == NULL)
        return false;
    return strcmp(operation, "Claim") == 0
        || strcmp(operation, "Read") == 0
        || strcmp(operation, "Write") == 0
        || strcmp(operation, "Release") == 0
        || strcmp(operation, "PinRead") == 0
        || strcmp(operation, "PinWrite") == 0
        || strcmp(operation, "PinReadInit") == 0
        || strcmp(operation, "PinWriteInit") == 0
        || strcmp(operation, "Unpin") == 0
        || strcmp(operation, "UnpinCleanup") == 0
        || strcmp(operation, "SubmitRead") == 0;
}

bool
mir_materialize_resource_runtime_row(MIRRoutine *routine,
                                     const char *abi_type_name,
                                     const char *operation,
                                     MIRResourceRuntimeRow *out)
{
    const MIRResourceRuntimeRow *row;

    if (routine == NULL || abi_type_name == NULL || operation == NULL
        || out == NULL)
        return true;
    if (!mir_resource_runtime_operation_has_row(operation)) {
        return true;
    }

    row = mir_abi_resource_runtime_row_for_type_name(
        abi_type_name, operation);
    if (row == NULL)
        return true;

    *out = *row;
    out->domain = pgy_arena_strdup(
        &routine->scratch, row->domain);
    out->abi_type_name = pgy_arena_strdup(
        &routine->scratch, row->abi_type_name);
    out->resource_op_name = pgy_arena_strdup(
        &routine->scratch, row->resource_op_name);
    out->runtime_fn = pgy_arena_strdup(
        &routine->scratch, row->runtime_fn);
    out->target_kind = pgy_arena_strdup(
        &routine->scratch, row->target_kind);
    out->materialization = pgy_arena_strdup(
        &routine->scratch, row->materialization);
    out->call_shape = pgy_arena_strdup(
        &routine->scratch, row->call_shape);
    out->runtime_call_abi_id = mir_abi_resource_runtime_row_id(out);
    if (out->domain == NULL
        || out->abi_type_name == NULL
        || out->resource_op_name == NULL
        || out->runtime_fn == NULL
        || out->target_kind == NULL
        || out->materialization == NULL
        || out->call_shape == NULL
        || out->runtime_call_abi_id == 0) {
        return false;
    }
    return true;
}

bool
mir_materialize_resource_runtime_fact(MIRRoutine *routine,
                                      MIRInstruction *inst)
{
    const char *operation;

    if (routine == NULL || inst == NULL || inst->abi_type_name == NULL)
        return true;
    operation = (inst->kind == MIR_INST_DEF
                 || inst->kind == MIR_INST_DESTRUCTURE)
        ? "Claim"
        : mir_machine_layer_runtime_operation(inst);
    if (operation == NULL)
        operation = inst->name;
    if (!mir_materialize_resource_runtime_row(
            routine, inst->abi_type_name, operation,
            &inst->resource_runtime_fact))
        return false;
    if (inst->resource_runtime_fact.runtime_call_abi_id != 0)
        inst->resource_runtime_fact_present = true;
    if (inst->resource_runtime_fact_present
        && inst->resource_runtime_fact.resource_op_name != NULL
        && strcmp(inst->resource_runtime_fact.resource_op_name, "Claim") == 0) {
        const char *aux_operations[] = {"Read", "Write"};
        for (size_t i = 0; i < sizeof(aux_operations) / sizeof(aux_operations[0]); i++) {
            if (inst->resource_runtime_aux_fact_count
                >= sizeof(inst->resource_runtime_aux_facts)
                    / sizeof(inst->resource_runtime_aux_facts[0]))
                break;
            MIRResourceRuntimeRow *aux =
                &inst->resource_runtime_aux_facts[inst->resource_runtime_aux_fact_count];
            if (!mir_materialize_resource_runtime_row(
                    routine, inst->abi_type_name, aux_operations[i], aux))
                return false;
            if (aux->runtime_call_abi_id != 0)
                inst->resource_runtime_aux_fact_count++;
        }
    } else if (inst->resource_runtime_fact_present
               && inst->resource_runtime_fact.resource_op_name != NULL
               && (strcmp(inst->resource_runtime_fact.resource_op_name,
                          "PinRead") == 0
                   || strcmp(inst->resource_runtime_fact.resource_op_name,
                             "PinWrite") == 0)) {
        const char *aux_operations[] = {
            strcmp(inst->resource_runtime_fact.resource_op_name, "PinRead") == 0
                ? "PinReadInit" : "PinWriteInit",
            "Unpin",
            "UnpinCleanup"
        };
        for (size_t i = 0; i < sizeof(aux_operations) / sizeof(aux_operations[0]); i++) {
            if (inst->resource_runtime_aux_fact_count
                >= sizeof(inst->resource_runtime_aux_facts)
                    / sizeof(inst->resource_runtime_aux_facts[0]))
                break;
            MIRResourceRuntimeRow *aux =
                &inst->resource_runtime_aux_facts[
                    inst->resource_runtime_aux_fact_count];
            if (!mir_materialize_resource_runtime_row(
                    routine, inst->abi_type_name, aux_operations[i], aux))
                return false;
            if (aux->runtime_call_abi_id != 0)
                inst->resource_runtime_aux_fact_count++;
        }
    } else if (inst->resource_runtime_fact_present
               && inst->resource_runtime_fact.resource_op_name != NULL
               && (strcmp(inst->resource_runtime_fact.resource_op_name,
                          "Read") == 0
                   || strcmp(inst->resource_runtime_fact.resource_op_name,
                             "Write") == 0)) {
        const char *aux_operations[] = {
            "PinReadInit", "PinWriteInit", "Unpin", "UnpinCleanup"
        };
        for (size_t i = 0; i < sizeof(aux_operations) / sizeof(aux_operations[0]); i++) {
            if (inst->resource_runtime_aux_fact_count
                >= sizeof(inst->resource_runtime_aux_facts)
                    / sizeof(inst->resource_runtime_aux_facts[0]))
                break;
            MIRResourceRuntimeRow *aux =
                &inst->resource_runtime_aux_facts[
                    inst->resource_runtime_aux_fact_count];
            if (!mir_materialize_resource_runtime_row(
                    routine, inst->abi_type_name, aux_operations[i], aux))
                return false;
            if (aux->runtime_call_abi_id != 0)
                inst->resource_runtime_aux_fact_count++;
        }
    }
    return true;
}

bool
mir_link_resource_runtime_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            MIRInstruction *consumer = &block->instructions[ii];
            MIRInstruction *source = NULL;
            bool ambiguous = false;

            if (consumer->kind == MIR_INST_RESOURCE_OP)
                continue;
            for (size_t ri = 0; ri < block->instruction_count; ri++) {
                MIRInstruction *resource = &block->instructions[ri];
                if (resource->kind != MIR_INST_RESOURCE_OP
                    || !resource->resource_runtime_fact_present
                    || !mir_instruction_consumes_resource_source(
                        resource, consumer)) {
                    continue;
                }
                if (source != NULL) {
                    ambiguous = true;
                    break;
                }
                source = resource;
            }
            if (ambiguous || source == NULL)
                continue;
            if (consumer->resource_runtime_fact_present
                && (consumer->resource_runtime_fact.runtime_fn == NULL
                    || strcmp(consumer->resource_runtime_fact.runtime_fn,
                              source->resource_runtime_fact.runtime_fn) != 0
                    || consumer->resource_runtime_fact.runtime_call_abi_id == 0
                    || consumer->resource_runtime_fact.runtime_call_abi_id
                        != source->resource_runtime_fact.runtime_call_abi_id)) {
                return false;
            }
            consumer->resource_runtime_fact = source->resource_runtime_fact;
            consumer->resource_runtime_fact_present = true;
        }
    }
    return true;
}

static bool
mir_add_resource_instruction(MIRRoutine *routine,
                             MIRBasicBlock *block,
                             const RIROp *op,
                             const char *resource_owner_slot_anchor,
                             const char *resource_owner_abi_type_name,
                             const MIRTypeLayout *resource_owner_layout)
{
    MIRInstruction inst;
    char *claim_type_name = NULL;
    const char *abi_type_name = NULL;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_RESOURCE_OP;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.resource_owner_slot_anchor = resource_owner_slot_anchor;
    inst.resource_owner_requires_metadata = resource_owner_slot_anchor != NULL;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    inst.has_source_statement_stable_id =
        op->has_source_statement_syntax_id;
    inst.source_statement_stable_id = op->source_statement_syntax_id;
    if (!mir_attach_machine_layer_fact(&inst, op))
        return false;
    mir_instruction_capture_source_provenance(&inst, op->ast);
    if (op->kind == RIR_OP_WRITE)
        inst.expr0 = mir_resource_write_value_expr_from_call(op->ast);
    if (op->kind == RIR_OP_CLAIM)
        claim_type_name = mir_claim_abi_type_name_from_ast(op->ast);
    if (op->kind == RIR_OP_AWAIT_LOCAL) {
        abi_type_name = "Future";
    } else if (op->kind == RIR_OP_AWAIT_REMOTE
               && mir_machine_layer_runtime_operation(&inst) != NULL) {
        abi_type_name = resource_owner_abi_type_name;
    } else if (op->kind == RIR_OP_AWAIT_REMOTE) {
        abi_type_name = "RemoteFuture";
    } else {
        abi_type_name = claim_type_name != NULL
            ? claim_type_name
            : (resource_owner_abi_type_name != NULL
                ? resource_owner_abi_type_name
                : (op->arg0 != NULL ? op->arg0 : op->subject));
    }
    if (abi_type_name != NULL) {
        inst.abi_type_name = pgy_arena_strdup(&routine->scratch, abi_type_name);
        if (inst.abi_type_name == NULL) {
            free(claim_type_name);
            return false;
        }
    }
    inst.type_layout = resource_owner_layout != NULL
        ? resource_owner_layout
        : mir_abi_lookup(inst.abi_type_name);
    inst.abi_layout_id = mir_abi_layout_id(inst.type_layout);
    if (!mir_materialize_resource_runtime_fact(routine, &inst)) {
        free(claim_type_name);
        return false;
    }
    free(claim_type_name);
    return mir_commit_instruction(routine, block, &inst);
}

static MIRBasicBlock *
mir_with_release_target_block(MIRRoutine *routine, const RIROp *op)
{
    const HIRRoutine *hir_routine;

    if (routine == NULL || op == NULL || op->kind != RIR_OP_RELEASE
        || op->ast == NULL || op->ast->type != AST_WITH_STMT) {
        return NULL;
    }
    hir_routine = routine->hir_routine;
    if (hir_routine == NULL || !hir_routine->has_cfg)
        return NULL;
    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *mir_block = &routine->blocks[i];
        const HIRBasicBlock *hir_block;
        if (mir_block->is_cleanup
            || mir_block->source_hir_block_id >= hir_routine->cfg.block_count) {
            continue;
        }
        hir_block = &hir_routine->cfg.blocks[mir_block->source_hir_block_id];
        for (size_t e = 0; e < hir_block->resource_scope_exit_count; e++) {
            if (hir_block->resource_scope_exits[e] == op->ast)
                return mir_block;
        }
    }
    return NULL;
}

bool
mir_populate_instructions(MIRRoutine *routine, const DIRProgram *dir)
{
    const RIRScope *rir_scope;
    MIRBasicBlock *entry;
    MIRBasicBlock *rollback;
    MIRBasicBlock *invalidation;
    MIRResourceBorrowLoweringFact *borrow_facts = NULL;
    size_t borrow_fact_count = 0;
    size_t op_count = 0;
    bool appended_intent_steps = false;

    if (routine == NULL || routine->block_count == 0)
        return true;

    rir_scope = routine->rir_scope;
    entry = &routine->blocks[routine->entry_block];
    rollback = routine->has_rollback_block ? &routine->blocks[routine->rollback_block] : NULL;
    invalidation = routine->has_invalidation_block ? &routine->blocks[routine->invalidation_block] : NULL;

    if (routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL) {
        if (!mir_append_intent_step_instructions(routine, entry, dir))
            return false;
        appended_intent_steps = true;
    }

    if (rir_scope == NULL)
        return true;

    op_count = rir_scope_op_count(rir_scope);
    if (op_count > 0) {
        borrow_facts = calloc(op_count, sizeof(*borrow_facts));
        if (borrow_facts == NULL)
            return false;
    }

    for (size_t i = 0; i < op_count; i++) {
        const RIROp *op = rir_scope_op_at(rir_scope, i);
        const char *resource_owner_slot_anchor = NULL;
        const char *resource_owner_abi_type_name = NULL;
        const MIRTypeLayout *resource_owner_layout = NULL;
        const MIRResourceBorrowLoweringFact *borrow_fact = NULL;
        if (op == NULL)
            continue;
        if (op->kind == RIR_OP_BORROW_READ
            || op->kind == RIR_OP_BORROW_WRITE) {
            resource_owner_slot_anchor = op->subject;
            resource_owner_layout =
                mir_resource_layout_for_slot_fact(routine, rir_scope,
                    op->subject, &resource_owner_abi_type_name);
        } else if (op->kind == RIR_OP_CLAIM) {
            resource_owner_layout =
                mir_resource_layout_for_slot_fact(routine, rir_scope,
                    op->subject, &resource_owner_abi_type_name);
        } else if (op->kind == RIR_OP_READ
                   || op->kind == RIR_OP_WRITE
                   || op->kind == RIR_OP_RELEASE
                   || op->kind == RIR_OP_MOVE
                   || op->machine_contact_kind
                        == RIR_MACHINE_CONTACT_SUBMIT_READ) {
            borrow_fact = mir_resource_borrow_fact_for_view(
                borrow_facts, borrow_fact_count, op->subject);
            if (borrow_fact != NULL) {
                resource_owner_slot_anchor = borrow_fact->source_slot;
                resource_owner_abi_type_name = borrow_fact->source_abi_type_name;
                resource_owner_layout = borrow_fact->source_layout;
            } else {
                resource_owner_layout =
                    mir_resource_layout_for_slot_fact(routine, rir_scope,
                        op->subject, &resource_owner_abi_type_name);
                if (resource_owner_abi_type_name == NULL) {
                    resource_owner_layout =
                        mir_resource_layout_for_prior_claim(routine, rir_scope,
                            i, op->subject, &resource_owner_abi_type_name);
                }
                if (resource_owner_abi_type_name == NULL) {
                    resource_owner_layout =
                        mir_resource_layout_for_prior_destructure(
                            routine, op->subject,
                            &resource_owner_abi_type_name);
                }
                if (resource_owner_abi_type_name == NULL) {
                    resource_owner_layout =
                        mir_resource_layout_for_source_local(
                            routine, op->subject,
                            &resource_owner_abi_type_name);
                }
            }
        }
        switch (op->kind) {
        case RIR_OP_ABORT_INTENT:
        case RIR_OP_COMPENSATE_INTENT_STEP:
            if (rollback != NULL) {
                if (!mir_add_cleanup_instruction(routine, rollback, op)) {
                    free(borrow_facts);
                    return false;
                }
                break;
            }
            /* fallthrough */
        default: {
            MIRBasicBlock *target_block = entry;
            if (op->kind == RIR_OP_RELEASE && op->ast != NULL
                && op->ast->type == AST_WITH_STMT) {
                target_block = mir_with_release_target_block(routine, op);
                if (target_block == NULL) {
                    free(borrow_facts);
                    return false;
                }
            }
            if (!mir_add_resource_instruction(routine, target_block, op,
                    resource_owner_slot_anchor, resource_owner_abi_type_name,
                    resource_owner_layout)) {
                free(borrow_facts);
                return false;
            }
            if (op->kind == RIR_OP_BORROW_READ
                || op->kind == RIR_OP_BORROW_WRITE) {
                mir_resource_record_borrow_fact(
                    borrow_facts,
                    op_count,
                    &borrow_fact_count,
                    op->arg0,
                    op->subject,
                    resource_owner_abi_type_name,
                    resource_owner_layout);
            }
            break;
        }
        }
    }
    free(borrow_facts);
    borrow_facts = NULL;

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
            mir_instruction_capture_source_provenance(&inst, fact->ast);
            if (!mir_commit_instruction(routine, invalidation, &inst))
                return false;
            routine->cleanup_instruction_count++;
        }
        if (!mir_append_intent_invalidation_markers(routine, invalidation))
            return false;
    }

    if (!appended_intent_steps && routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL) {
        if (!mir_append_intent_step_instructions(routine, entry, dir))
            return false;
    } else if (routine->hir_routine != NULL
               && !routine->hir_routine->has_cfg) {
        if (!mir_append_non_cfg_body_statements(routine, entry))
            return false;
    }
    return true;
}
