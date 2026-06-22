#include "transpiler_mir_ssa_local_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

void
transpiler_mir_ssa_local_trim_type_annotation_suffix(char *type_name)
{
    char *colon;

    if (type_name == NULL)
        return;
    colon = strchr(type_name, ':');
    if (colon == NULL)
        return;
    while (colon > type_name
           && (colon[-1] == ' ' || colon[-1] == '\t')) {
        colon--;
    }
    *colon = '\0';
}

static ASTNode *
transpiler_mir_receive_expr_from_def(const MIRInstruction *inst)
{
    if (inst == NULL)
        return NULL;
    return inst->expr0 != NULL && inst->expr0->type == AST_CHANNEL_RECV
        ? inst->expr0
        : NULL;
}

const char *
transpiler_mir_ssa_local_find_receive_payload_type_name(
    TranspilerCtx *ctx,
    ASTNode *func_decl,
    const MIRRoutine *routine,
    const char *base_name)
{
    char payload_buf[128];

    if (ctx == NULL || routine == NULL || base_name == NULL)
        return NULL;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            char def_base[128];
            size_t def_version = 0;
            ASTNode *recv_expr;
            ASTNode *channel;
            const char *channel_type;

            if (inst->kind != MIR_INST_DEF
                || inst->result_name == NULL
                || !mir_instruction_uses_channel_receive_statement_emit(inst)) {
                continue;
            }
            if (!transpiler_parse_versioned_name(inst->result_name,
                    def_base, sizeof(def_base), &def_version)
                || strcmp(def_base, base_name) != 0) {
                continue;
            }
            (void)def_version;
            recv_expr = transpiler_mir_receive_expr_from_def(inst);
            if (recv_expr == NULL)
                continue;
            channel = ast_channel_recv_channel(recv_expr);
            if (channel_inner_type_name_copy(ctx, channel, payload_buf,
                    sizeof(payload_buf))
                && payload_buf[0] != '\0'
                && strcmp(payload_buf, "Unknown") != 0) {
                transpiler_mir_ssa_local_trim_type_annotation_suffix(
                    payload_buf);
                return transpiler_mir_arena_copy_type_name(ctx, payload_buf);
            }
            if (channel == NULL || channel->type != AST_IDENTIFIER)
                continue;
            channel_type = transpiler_find_local_type_name(ctx, func_decl,
                ast_identifier_name(channel));
            if (!slot_inner_type_name_copy(channel_type, payload_buf,
                    sizeof(payload_buf))
                || payload_buf[0] == '\0'
                || strcmp(payload_buf, "Unknown") == 0) {
                continue;
            }
            transpiler_mir_ssa_local_trim_type_annotation_suffix(payload_buf);
            return transpiler_mir_arena_copy_type_name(ctx, payload_buf);
        }
    }
    return NULL;
}

static char *
transpiler_mir_format_owned_type_name(const char *prefix, const char *inner)
{
    char buf[128];
    int written;

    if (prefix == NULL || inner == NULL || inner[0] == '\0')
        return NULL;
    written = snprintf(buf, sizeof(buf), "%s<%s>", prefix, inner);
    if (written < 0 || (size_t)written >= sizeof(buf))
        return NULL;
    return pergyra_strdup(buf);
}

static char *
transpiler_mir_tuple_element_type_name(const char *tuple_type, size_t index)
{
    size_t pi = 1;
    size_t plen;
    size_t cur = 0;

    if (tuple_type == NULL || tuple_type[0] != '(')
        return NULL;
    plen = strlen(tuple_type);
    while (pi < plen && tuple_type[pi] != ')') {
        char rendered[128];
        size_t eo = 0;
        int depth = 0;

        while (pi < plen
               && (tuple_type[pi] == ' ' || tuple_type[pi] == '\t')) {
            pi++;
        }
        while (pi < plen && eo + 1 < sizeof(rendered)) {
            char c = tuple_type[pi];
            if (depth == 0 && (c == ',' || c == ')'))
                break;
            if (c == '<' || c == '(')
                depth++;
            if (c == '>' || c == ')')
                depth--;
            rendered[eo++] = c;
            pi++;
        }
        rendered[eo] = '\0';
        while (eo > 0
               && (rendered[eo - 1] == ' '
                   || rendered[eo - 1] == '\t')) {
            rendered[--eo] = '\0';
        }
        if (cur == index)
            return rendered[0] != '\0' ? pergyra_strdup(rendered) : NULL;
        cur++;
        if (pi < plen && tuple_type[pi] == ',')
            pi++;
    }
    return NULL;
}

static char *
transpiler_mir_destructure_binding_type_name(TranspilerCtx *ctx,
                                             const ASTNode *func_decl,
                                             const MIRInstruction *inst,
                                             const char *base_name)
{
    ASTNode *init;
    size_t binding_index = 0;
    const char *init_type;

    if (!mir_instruction_destructure_binding_index(inst, base_name,
            &binding_index)) {
        return NULL;
    }
    init = inst->expr0;
    if (init != NULL && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(init)) != NULL) {
        const char *callee = ast_identifier_name(ast_call_callee(init));
        if (pgy_codegen_call_name_is_claim_secure_slot(callee)) {
            const char *inner =
                transpiler_let_slot_inner_from_call_type_arg(ctx, init);
            return transpiler_mir_format_owned_type_name(
                binding_index == 0 ? "SecureSlot" : "Token", inner);
        }
        if (pgy_codegen_call_name_is_claim_slot(callee)
            && binding_index == 0) {
            const char *inner =
                transpiler_let_slot_inner_from_call_type_arg(ctx, init);
            return transpiler_mir_format_owned_type_name("Slot", inner);
        }
    }
    init_type = transpiler_infer_local_type_name_from_expr(ctx, func_decl,
        init);
    if (transpiler_type_name_is_array_or_slice(init_type)) {
        char inner_buf[128];
        if (slot_inner_type_name_copy(init_type, inner_buf,
                sizeof(inner_buf))
            && inner_buf[0] != '\0') {
            return pergyra_strdup(inner_buf);
        }
    }
    return transpiler_mir_tuple_element_type_name(init_type, binding_index);
}

static size_t
transpiler_mir_ssa_local_source_def_count(const MIRRoutine *routine,
                                          const char *base_name)
{
    size_t count = 0;

    if (routine == NULL || base_name == NULL)
        return 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t i = 0; i < block->source_local_def_count; i++) {
            const char *name = block->source_local_defs[i];
            if (name != NULL && strcmp(name, base_name) == 0)
                count++;
        }
    }
    return count;
}

char *
transpiler_mir_ssa_local_find_versioned_type_name(
    TranspilerCtx *ctx,
    const ASTNode *func_decl,
    const MIRRoutine *routine,
    const char *versioned_name)
{
    char base_name[128];
    size_t version = 0;

    if (routine == NULL || versioned_name == NULL)
        return NULL;
    if (!transpiler_parse_versioned_name(versioned_name, base_name,
            sizeof(base_name), &version)) {
        return NULL;
    }
    (void)version;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_DEF
                && inst->result_name != NULL
                && strcmp(inst->result_name, versioned_name) == 0) {
                const char *source_type =
                    transpiler_mir_routine_source_local_type_name(
                        routine, base_name);
                if (source_type != NULL
                    && source_type[0] != '\0'
                    && strcmp(source_type, "Unknown") != 0
                    && transpiler_mir_ssa_local_source_def_count(
                        routine, base_name) == 1) {
                    return pergyra_strdup(source_type);
                }
                if (inst->expr1 != NULL
                    && inst->expr1->type == AST_TYPE) {
                    return transpiler_render_effective_local_type_name(
                        ctx, inst->expr1);
                }
                if (inst->expr0 != NULL) {
                    const char *inferred =
                        transpiler_infer_local_type_name_from_expr(
                            ctx, func_decl, inst->expr0);
                    if (inferred != NULL
                        && inferred[0] != '\0'
                        && strcmp(inferred, "Unknown") != 0) {
                        return pergyra_strdup(inferred);
                    }
                }
                if (inst->arg1 != NULL
                    && inst->arg1[0] != '\0'
                    && is_nominal_host_type_name(ctx, inst->arg1)) {
                    return pergyra_strdup(inst->arg1);
                }
            }
        }
    }
    {
        const char *source_type =
            transpiler_mir_routine_source_local_type_name(routine, base_name);
        if (source_type != NULL
            && source_type[0] != '\0'
            && strcmp(source_type, "Unknown") != 0) {
            return pergyra_strdup(source_type);
        }
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            char *type_name;

            if (inst->kind != MIR_INST_DESTRUCTURE)
                continue;
            type_name = transpiler_mir_destructure_binding_type_name(
                ctx, func_decl, inst, base_name);
            if (type_name != NULL)
                return type_name;
        }
    }

    return NULL;
}

bool
transpiler_mir_ssa_local_entry_has_source_def(const MIRRoutine *routine,
                                              const char *base_name)
{
    const MIRBasicBlock *block;

    if (routine == NULL || base_name == NULL
        || routine->entry_block >= routine->block_count) {
        return false;
    }
    block = &routine->blocks[routine->entry_block];
    for (size_t i = 0; i < block->source_local_def_count; i++) {
        const char *name = block->source_local_defs[i];
        if (name != NULL && strcmp(name, base_name) == 0)
            return true;
    }
    return false;
}

bool
transpiler_mir_ssa_local_routine_has_source_def(const MIRRoutine *routine,
                                                const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t i = 0; i < block->source_local_def_count; i++) {
            const char *name = block->source_local_defs[i];
            if (name != NULL && strcmp(name, base_name) == 0)
                return true;
        }
    }
    return false;
}

bool
transpiler_mir_ssa_local_routine_has_param_name(const MIRRoutine *routine,
                                                const char *base_name)
{
    if (routine == NULL || base_name == NULL
        || !transpiler_mir_routine_has_signature(routine)) {
        return false;
    }
    for (size_t i = 0; i < transpiler_mir_routine_param_count(routine); i++) {
        FuncParam *param = transpiler_mir_routine_param(routine, i);
        if (param != NULL
            && param->name != NULL
            && strcmp(param->name, base_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
transpiler_mir_ssa_local_routine_has_destructure_binding(
    const MIRRoutine *routine,
    const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            size_t binding_index = 0;

            if (inst->kind != MIR_INST_DESTRUCTURE)
                continue;
            if (mir_instruction_destructure_binding_index(inst, base_name,
                    &binding_index)) {
                return true;
            }
        }
    }
    return false;
}

static ASTNode *
transpiler_mir_view_constructor_call_from_source(ASTNode *source)
{
    ASTNode *value;

    if (source == NULL)
        return NULL;
    if (source->type == AST_CALL)
        return source;
    if (source->type == AST_LET_DECL)
        return ast_let_initializer(source);
    if (source->type != AST_ASSIGNMENT)
        return NULL;
    value = ast_assignment_value(source);
    return value != NULL && value->type == AST_CALL ? value : NULL;
}

static bool
transpiler_mir_register_base_local_view_fact(TranspilerCtx *ctx,
                                             const MIRRoutine *routine,
                                             const char *versioned_name,
                                             const char *base_name,
                                             const char *type_name)
{
    if (ctx == NULL || routine == NULL || versioned_name == NULL
        || base_name == NULL || type_name == NULL
        || !transpiler_type_name_is_view_like(type_name)) {
        return false;
    }
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode *call;
            ASTNode *callee_node;
            ASTNode *source_node;
            const char *callee;
            const char *source;
            const char *source_type;
            bool source_secure;

            if (inst->kind != MIR_INST_DEF
                || inst->result_name == NULL
                || strcmp(inst->result_name, versioned_name) != 0) {
                continue;
            }
            call = transpiler_mir_view_constructor_call_from_source(
                inst->expr0);
            if (call == NULL || call->type != AST_CALL)
                return false;
            callee_node = ast_call_callee(call);
            if (callee_node == NULL || callee_node->type != AST_IDENTIFIER
                || ast_call_arg_count(call) < 1) {
                return false;
            }
            source_node = ast_call_argument(call, 0);
            if (source_node == NULL || source_node->type != AST_IDENTIFIER)
                return false;
            callee = ast_identifier_name(callee_node);
            source = ast_identifier_name(source_node);
            if (callee == NULL || source == NULL
                || !pgy_codegen_call_name_is_view_constructor(callee)) {
                return false;
            }
            source_type = lookup_typed_var(ctx, source);
            source_secure = lookup_slot_is_secure(ctx, source);
            if (!source_secure
                && source_type != NULL
                && (strcmp(source_type, "SecureSlot") == 0
                    || strncmp(source_type, "SecureSlot<", 11) == 0)) {
                source_secure = true;
            }
            register_view_like_var(ctx, base_name, type_name, source,
                source_secure, false);
            return true;
        }
    }
    return false;
}

void
transpiler_mir_ssa_local_register_base_type_fact(
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const char *versioned_name,
    const char *base_name,
    const char *type_name)
{
    char inner_buf[128];
    bool is_secure;

    if (ctx == NULL || base_name == NULL || type_name == NULL
        || type_name[0] == '\0' || strcmp(type_name, "Unknown") == 0) {
        return;
    }
    if (transpiler_mir_register_base_local_view_fact(ctx, routine,
            versioned_name, base_name, type_name)) {
        return;
    }
    register_typed_var(ctx, base_name, type_name);
    is_secure = pgy_codegen_type_name_is_secure_slot(type_name);
    if (!is_secure && !pgy_codegen_type_name_is_slot(type_name))
        return;
    if (!slot_inner_type_name_copy(type_name, inner_buf, sizeof(inner_buf))
        || inner_buf[0] == '\0') {
        return;
    }
    register_slot_var(ctx, base_name, inner_buf, is_secure, false);
}
