/* Native projection of the existing lexical LocalRef wire contract. */
#include "mir_json_local_ref.h"
#include "mir.h"
#include "mir_json_dump_internal.h"
#include "../parser/ast_api.h"
#include <inttypes.h>
#include <string.h>

bool
mir_json_routine_local_refs_required(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t row = 0; row < routine->source_local_type_count; row++) {
        const char *name = routine->source_local_types[row].name;
        if (name == NULL)
            continue;
        for (size_t prior = 0; prior < row; prior++) {
            const char *other = routine->source_local_types[prior].name;
            if (other != NULL && strcmp(name, other) == 0)
                return true;
        }
        for (size_t p = 0; p < mir_routine_param_count(routine); p++) {
            FuncParam *param = mir_routine_param(routine, p);
            if (param != NULL && param->name != NULL
                && strcmp(name, param->name) == 0)
                return true;
        }
    }
    if (routine->iteration_type_fact_count > 1)
        return true;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (inst->kind == MIR_INST_STMT && inst->arg0 != NULL
                && (strcmp(inst->arg0, "ArrayPush") == 0
                    || strcmp(inst->arg0, "ArraySet") == 0
                    || strcmp(inst->arg0, "ArrayPop") == 0))
                return true;
        }
    }
    return false;
}

static void
mir_json_emit_binding_local_ref(FILE *out, const MIRRoutine *routine,
                                uint32_t binding_id)
{
    if (binding_id != 0) {
        for (size_t p = 0; p < mir_routine_param_count(routine); p++) {
            FuncParam *param = mir_routine_param(routine, p);
            if (param != NULL && ast_func_param_stable_id(param) == binding_id) {
                fprintf(out, "\"parameter:%" PRIu32 ":%zu\"", routine->source_syntax_id, p);
                return;
            }
        }
        if (mir_routine_iteration_type_fact(routine, binding_id) != NULL) {
            fprintf(out, "\"iteration:%" PRIu32 ":0\"", binding_id);
            return;
        }
        for (size_t row = 0; row < routine->source_local_type_count; row++) {
            if (routine->source_local_types[row].binding_syntax_id == binding_id) {
                fprintf(out, "\"declaration:%" PRIu32 ":0\"", binding_id);
                return;
            }
        }
    }
    /* Missing identity remains absent; the consumer must refuse a required
     * definition rather than borrowing another declaration with this name. */
    fputs("null", out);
}

void
mir_json_emit_instruction_local_ref(FILE *out, const MIRRoutine *routine,
                                    const MIRInstruction *inst)
{
    uint32_t binding_id = inst->result_name != NULL ? inst->binding_syntax_id : 0;
    if (inst->kind == MIR_INST_STMT && inst->arg0 != NULL
        && (strcmp(inst->arg0, "ArrayPush") == 0
            || strcmp(inst->arg0, "ArraySet") == 0
            || strcmp(inst->arg0, "ArrayPop") == 0)
        && inst->expr0 != NULL && inst->expr0->type == AST_CALL) {
        binding_id = ast_identifier_binding_syntax_id(ast_call_argument(inst->expr0, 0));
    }
    fputs(",\"local_ref\":", out);
    mir_json_emit_binding_local_ref(out, routine, binding_id);
    /* Scalar SSA reads already carry their exact use/version rows. */
    fputs(",\"expr0_local_refs\":[]", out);
}
