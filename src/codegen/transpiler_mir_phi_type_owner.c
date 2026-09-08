#include "transpiler_mir_phi_type_owner.h"

#include <stdlib.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_ssa_map.h"

typedef struct {
    const char *name;
    const char *type_name;
    const MIRInstruction *instruction;
    size_t output_row;
} MIRPhiTypeRow;

static int compare_phi_type_names(const void *left, const void *right)
{
    const MIRPhiTypeRow *a = left, *b = right;
    return strcmp(a->name, b->name);
}

static MIRPhiTypeRow *find_phi_type_row(MIRPhiTypeRow *rows, size_t count,
                                      const char *name)
{
    if (name == NULL)
        return NULL;
    MIRPhiTypeRow key = {.name = name};
    return bsearch(&key, rows, count, sizeof(*rows), compare_phi_type_names);
}

static bool phi_type_failure(TranspilerCtx *ctx, const char *name,
                             const char *detail)
{
    transpiler_set_mir_inventory_missing(ctx,
        "MIR SSA phi type fact invalid for '%s': %s",
        name != NULL ? name : "(missing)", detail);
    return false;
}

bool transpiler_mir_phi_types_from_incomings(
    TranspilerCtx *ctx, const MIRRoutine *routine,
    const char *const *names, size_t count, TranspilerMIRPhiType *out)
{
    if (routine == NULL || (count > 0 && (names == NULL || out == NULL)))
        return phi_type_failure(ctx, NULL, "missing routine/value inventory");
    if (count == 0)
        return true;
    MIRPhiTypeRow *rows = calloc(count, sizeof(*rows));
    if (rows == NULL)
        return phi_type_failure(ctx, NULL, "projection allocation failed");
    bool ok = false;
    size_t phi_count = 0;
    for (size_t i = 0; i < count; i++) {
        out[i] = (TranspilerMIRPhiType){0};
        if (names[i] == NULL) {
            phi_type_failure(ctx, NULL, "missing SSA value identity");
            goto cleanup;
        }
        rows[i].name = names[i];
        rows[i].output_row = i;
    }
    qsort(rows, count, sizeof(*rows), compare_phi_type_names);
    for (size_t i = 1; i < count; i++) {
        if (strcmp(rows[i - 1].name, rows[i].name) == 0) {
            phi_type_failure(ctx, rows[i].name, "duplicate SSA value identity");
            goto cleanup;
        }
    }
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        if (!block->is_reachable || block->is_cleanup)
            continue;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            MIRPhiTypeRow *row = find_phi_type_row(rows, count, inst->result_name);
            if (row == NULL)
                continue;
            if (row->instruction != NULL) {
                phi_type_failure(ctx, row->name, "multiple defining instructions");
                goto cleanup;
            }
            row->instruction = inst;
            if (inst->abi_type_name != NULL && inst->abi_type_name[0] != '\0' &&
                strcmp(inst->abi_type_name, "Unknown") != 0)
                row->type_name = inst->abi_type_name;
            if (inst->kind == MIR_INST_PHI) {
                out[row->output_row].is_phi = true;
                phi_count++;
            }
        }
    }
    for (size_t i = 0; i < count; i++) {
        char base[128];
        size_t version;
        if (rows[i].instruction != NULL ||
            !transpiler_parse_versioned_name(rows[i].name, base, sizeof(base), &version) ||
            version != 0)
            continue;
        for (size_t p = 0; p < transpiler_mir_routine_param_count(routine); p++) {
            FuncParam *param = transpiler_mir_routine_param(routine, p);
            if (param != NULL && param->name != NULL && strcmp(param->name, base) == 0) {
                const char *type = transpiler_mir_routine_param_type_name(routine, p);
                if (type != NULL && type[0] != '\0' && strcmp(type, "Unknown") != 0)
                    rows[i].type_name = type;
                break;
            }
        }
    }
    /* Monotone type equalities resolve loop cycles from concrete incoming
     * definitions. Every edge is rechecked after convergence; a seed is not
     * permission to overlook an unknown or differently typed incoming value. */
    for (size_t round = 0; round <= phi_count; round++) {
        bool changed = false;
        for (size_t i = 0; i < count; i++) {
            const MIRInstruction *phi = rows[i].instruction;
            if (phi == NULL || phi->kind != MIR_INST_PHI)
                continue;
            if (phi->phi_incoming_count == 0 || phi->phi_incomings == NULL) {
                phi_type_failure(ctx, rows[i].name, "missing incoming edges");
                goto cleanup;
            }
            for (size_t e = 0; e < phi->phi_incoming_count; e++) {
                const MIRPhiIncoming *edge = &phi->phi_incomings[e];
                MIRPhiTypeRow *input = find_phi_type_row(rows, count, edge->value_name);
                if (input == NULL || edge->predecessor_block >= routine->block_count) {
                    phi_type_failure(ctx, rows[i].name, "missing incoming value identity");
                    goto cleanup;
                }
                if (input->type_name == NULL)
                    continue;
                if (rows[i].type_name == NULL) {
                    rows[i].type_name = input->type_name;
                    changed = true;
                } else if (strcmp(rows[i].type_name, input->type_name) != 0) {
                    phi_type_failure(ctx, rows[i].name, "conflicting incoming types");
                    goto cleanup;
                }
            }
        }
        if (!changed)
            break;
    }
    for (size_t i = 0; i < count; i++) {
        const MIRInstruction *phi = rows[i].instruction;
        if (phi == NULL || phi->kind != MIR_INST_PHI)
            continue;
        if (rows[i].type_name == NULL) {
            phi_type_failure(ctx, rows[i].name, "unresolved incoming type cycle");
            goto cleanup;
        }
        for (size_t e = 0; e < phi->phi_incoming_count; e++) {
            MIRPhiTypeRow *input = find_phi_type_row(rows, count,
                phi->phi_incomings[e].value_name);
            if (input == NULL || input->type_name == NULL ||
                strcmp(rows[i].type_name, input->type_name) != 0) {
                phi_type_failure(ctx, rows[i].name, "missing or crossed incoming type fact");
                goto cleanup;
            }
        }
        out[rows[i].output_row].type_name = rows[i].type_name;
    }
    ok = true;
cleanup:
    free(rows);
    return ok;
}
