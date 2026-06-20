#include "transpiler_mir_match_condition_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "codegen_match_subject_lookup.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_match_bindings.h"
#include "transpiler_mir_match_pattern_emit.h"
#include "transpiler_mir_match_payload_emit.h"
#include "transpiler_mir_match_region_emit.h"
#include "transpiler_type_require.h"

static bool
transpiler_mir_set_payload_binding_name(TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map,
                                        const char *binding,
                                        const char *emitted_name)
{
    const char *stable_name;

    if (ssa_map == NULL)
        return true;
    if (ctx == NULL || binding == NULL || emitted_name == NULL)
        return false;
    stable_name = transpiler_scratch_strdup(ctx, emitted_name);
    if (stable_name == NULL)
        return false;
    if (ctx->match_binding_alias_map != NULL) {
        transpiler_ssa_name_map_set(
            (TranspilerSSANameMap *)ctx->match_binding_alias_map,
            binding, stable_name);
    }
    return transpiler_ssa_name_map_set(ssa_map, binding, stable_name);
}

static bool
transpiler_mir_remap_payload_binding(TranspilerCtx *ctx,
                                     TranspilerSSANameMap *ssa_map,
                                     uint32_t case_stable_id,
                                     const char *binding)
{
    char emitted_name[256];

    if (binding == NULL || strcmp(binding, "_") == 0)
        return true;
    transpiler_mir_match_binding_name(case_stable_id, binding,
                                      emitted_name, sizeof(emitted_name));
    return transpiler_mir_set_payload_binding_name(ctx, ssa_map,
                                                   binding, emitted_name);
}

static bool
transpiler_mir_remap_case_bindings(TranspilerCtx *ctx,
                                   TranspilerSSANameMap *ssa_map,
                                   const MIRInstruction *inst)
{
    ASTNode *case_node;
    ASTNode *pattern_node;
    const char *kind = NULL;
    const char *binding = NULL;
    uint32_t case_stable_id;

    if (ctx == NULL || inst == NULL)
        return true;
    case_node = mir_instruction_source_payload(inst);
    case_stable_id = mir_instruction_source_stable_id(inst);
    if (case_node == NULL || case_node->type != AST_MATCH_CASE)
        return true;
    pattern_node = ast_match_case_pattern(case_node);
    if (pattern_node == NULL)
        return true;
    if (transpiler_mir_is_option_destructor(pattern_node, &kind, &binding)
        || transpiler_mir_is_result_destructor(pattern_node, &kind, &binding)) {
        (void)kind;
        return transpiler_mir_remap_payload_binding(ctx, ssa_map,
                                                    case_stable_id, binding);
    }

    {
        const char *enum_vname = NULL;
        const char *enum_ename = NULL;
        const char **enum_bindings = NULL;
        const char **enum_binding_type_names = NULL;
        size_t enum_bind_count = 0;
        const char *enum_bindings_buf[8];
        const char *enum_binding_type_names_buf[8];
        if (transpiler_match_is_enum_variant_destructor(pattern_node, ctx,
                &enum_vname, &enum_ename, &enum_bindings,
                &enum_binding_type_names, &enum_bind_count,
                enum_bindings_buf, enum_binding_type_names_buf,
                sizeof(enum_bindings_buf)
                    / sizeof(enum_bindings_buf[0]))) {
            (void)enum_vname;
            (void)enum_ename;
            (void)enum_binding_type_names;
            for (size_t i = 0; i < enum_bind_count; i++) {
                if (enum_bindings == NULL)
                    continue;
                if (!transpiler_mir_remap_payload_binding(
                        ctx, ssa_map, case_stable_id, enum_bindings[i])) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool
transpiler_mir_emit_match_case_body_binding(CodeBuf *buf,
                                            const MIRRoutine *routine,
                                            const MIRBasicBlock *block,
                                            TranspilerCtx *ctx,
                                            TranspilerSSANameMap *ssa_map)
{
    const MIRInstruction *branch_inst;
    ASTNode *case_node;
    ASTNode *pattern_node;
    ASTNode *subject_node;
    char *subject;
    const char *kind = NULL;
    const char *binding = NULL;
    uint32_t case_stable_id;

    if (buf == NULL || routine == NULL || block == NULL || ctx == NULL) {
        return true;
    }
    branch_inst = transpiler_mir_find_incoming_match_branch(routine, block);
    if (branch_inst == NULL)
        return true;
    case_node = mir_instruction_source_payload(branch_inst);
    case_stable_id = mir_instruction_source_stable_id(branch_inst);
    if (case_node == NULL || case_node->type != AST_MATCH_CASE)
        return true;
    pattern_node = ast_match_case_pattern(case_node);
    if (pattern_node == NULL)
        return true;
    subject_node = pgy_codegen_match_subject_for_branch(branch_inst);
    if (subject_node == NULL)
        return true;
    subject = emit_expression_with_ssa_map(subject_node, ctx, ssa_map);
    if (subject == NULL)
        return false;

    if (transpiler_mir_is_option_destructor(pattern_node, &kind, &binding)
        || transpiler_mir_is_result_destructor(pattern_node, &kind, &binding)) {
        if (binding != NULL) {
            char emitted_name[256];
            transpiler_mir_match_binding_name(case_stable_id, binding,
                                              emitted_name,
                                              sizeof(emitted_name));
            if (ast_match_case_guard(case_node) != NULL) {
                if (!transpiler_mir_set_payload_binding_name(
                        ctx, ssa_map, binding, emitted_name)) {
                    free(subject);
                    return false;
                }
                free(subject);
                return true;
            }
            transpiler_mir_emit_match_payload_binding(buf, ctx, subject_node,
                                                      subject, kind, binding,
                                                      emitted_name);
            if (!transpiler_mir_set_payload_binding_name(
                    ctx, ssa_map, binding, emitted_name)) {
                free(subject);
                return false;
            }
        }
        free(subject);
        return true;
    }

    {
        const char *enum_vname = NULL;
        const char *enum_ename = NULL;
        const char **enum_bindings = NULL;
        const char **enum_binding_type_names = NULL;
        size_t enum_bind_count = 0;
        const char *enum_bindings_buf[8];
        const char *enum_binding_type_names_buf[8];
        if (transpiler_match_is_enum_variant_destructor(pattern_node, ctx,
                &enum_vname, &enum_ename, &enum_bindings,
                &enum_binding_type_names, &enum_bind_count,
                enum_bindings_buf, enum_binding_type_names_buf,
                sizeof(enum_bindings_buf)
                    / sizeof(enum_bindings_buf[0]))) {
            (void)enum_ename;
            for (size_t b = 0; b < enum_bind_count; b++) {
                char bt_buf[256];
                const char *bt_c_type = "int32_t";
                const char *binding_name;
                char emitted_name[256];
                if (enum_bindings == NULL || enum_bindings[b] == NULL)
                    continue;
                binding_name = enum_bindings[b];
                if (strcmp(binding_name, "_") == 0)
                    continue;
                if (enum_binding_type_names != NULL
                    && enum_binding_type_names[b] != NULL) {
                    if (!transpiler_require_type_name_c_type_copy(
                            ctx, enum_binding_type_names[b],
                            "MIR enum match payload binding",
                            bt_buf, sizeof(bt_buf))) {
                        free(subject);
                        return false;
                    }
                    bt_c_type = bt_buf;
                }
                transpiler_mir_match_binding_name(case_stable_id,
                                                  binding_name,
                                                  emitted_name,
                                                  sizeof(emitted_name));
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s %s = (%s).%s._%zu;\n",
                    bt_c_type, emitted_name, subject, enum_vname, b);
                if (!transpiler_mir_set_payload_binding_name(
                        ctx, ssa_map, binding_name, emitted_name)) {
                    free(subject);
                    return false;
                }
            }
        }
    }

    free(subject);
    return true;
}

bool
transpiler_mir_remap_active_match_bindings(const MIRRoutine *routine,
                                           const MIRBasicBlock *block,
                                           TranspilerCtx *ctx,
                                           TranspilerSSANameMap *ssa_map)
{
    size_t target_id;

    if (routine == NULL || block == NULL || ctx == NULL)
        return true;
    target_id = block->id;
    if (target_id >= routine->block_count)
        return true;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *candidate = &routine->blocks[i];
        if (!transpiler_mir_case_true_region_contains(routine, candidate,
                                                      target_id)) {
            continue;
        }
        for (size_t j = 0; j < candidate->instruction_count; j++) {
            const MIRInstruction *inst = &candidate->instructions[j];
            if (inst->kind != MIR_INST_BRANCH
                || inst->branch_shape != MIR_BRANCH_MATCH_CASE) {
                continue;
            }
            if (!transpiler_mir_remap_case_bindings(ctx, ssa_map, inst)) {
                return false;
            }
        }
    }
    return true;
}

char *
transpiler_mir_render_match_case_condition(const MIRInstruction *inst,
                                           TranspilerCtx *ctx,
                                           TranspilerSSANameMap *ssa_map)
{
    ASTNode *subject_node;
    ASTNode *guard_node;
    char *subject;
    char *cond = NULL;
    char *guard = NULL;
    char *guard_payload_assign = NULL;
    bool has_guard;
    uint32_t case_stable_id;
    size_t pattern_count;

    if (inst == NULL || ctx == NULL) {
        return NULL;
    }
    case_stable_id = mir_instruction_source_stable_id(inst);
    pattern_count = mir_instruction_match_pattern_count(inst);
    if (pattern_count == 0) {
        return NULL;
    }
    guard_node = mir_instruction_match_guard(inst);
    has_guard = guard_node != NULL;

    subject_node = pgy_codegen_match_subject_for_branch(inst);
    if (subject_node == NULL)
        return NULL;

    subject = emit_expression_with_ssa_map(subject_node, ctx, ssa_map);
    if (subject == NULL)
        return NULL;

    if (pattern_count > 1) {
        for (size_t i = 0; i < pattern_count; i++) {
            char *pat = emit_expression_with_ssa_map(
                mir_instruction_match_pattern_at(inst, i), ctx, ssa_map);
            char *next = NULL;
            if (pat == NULL)
                continue;
            next = cond == NULL
                ? strdup_fmt("(%s == %s)", subject, pat)
                : strdup_fmt("(%s || (%s == %s))", cond, subject, pat);
            free(cond);
            free(pat);
            cond = next;
        }
    } else {
        const char *kind = NULL;
        const char *binding = NULL;
        ASTNode *pattern_node = mir_instruction_match_pattern_at(inst, 0);
        if (transpiler_mir_is_option_destructor(
                pattern_node, &kind, &binding)) {
            const char *tag = pgy_codegen_match_variant_c_option_tag(
                pgy_codegen_match_variant_lookup(kind));
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
            if (has_guard && binding != NULL) {
                const char *field =
                    transpiler_mir_match_payload_field(kind);
                char emitted_name[256];
                transpiler_mir_match_binding_name(case_stable_id, binding,
                                                  emitted_name,
                                                  sizeof(emitted_name));
                if (!transpiler_mir_declare_guard_payload_binding(
                        ctx->out, ctx, subject_node, kind, binding,
                        emitted_name)) {
                    free(subject);
                    free(cond);
                    return NULL;
                }
                if (ssa_map != NULL
                    && !transpiler_mir_set_payload_binding_name(
                        ctx, ssa_map, binding, emitted_name)) {
                    free(subject);
                    free(cond);
                    return NULL;
                }
                if (field != NULL) {
                    guard_payload_assign = strdup_fmt("%s = (%s).%s",
                                                      emitted_name,
                                                      subject,
                                                      field);
                }
            }
        } else if (transpiler_mir_is_result_destructor(
                       pattern_node, &kind, &binding)) {
            const char *tag = pgy_codegen_match_variant_c_result_tag(
                pgy_codegen_match_variant_lookup(kind));
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
            if (has_guard && binding != NULL) {
                const char *field =
                    transpiler_mir_match_payload_field(kind);
                char emitted_name[256];
                transpiler_mir_match_binding_name(case_stable_id, binding,
                                                  emitted_name,
                                                  sizeof(emitted_name));
                if (!transpiler_mir_declare_guard_payload_binding(
                        ctx->out, ctx, subject_node, kind, binding,
                        emitted_name)) {
                    free(subject);
                    free(cond);
                    return NULL;
                }
                if (ssa_map != NULL
                    && !transpiler_mir_set_payload_binding_name(
                        ctx, ssa_map, binding, emitted_name)) {
                    free(subject);
                    free(cond);
                    return NULL;
                }
                if (field != NULL) {
                    guard_payload_assign = strdup_fmt("%s = (%s).%s",
                                                      emitted_name,
                                                      subject,
                                                      field);
                }
            }
        } else {
            const char *enum_vname = NULL;
            const char *enum_ename = NULL;
            const char **enum_bindings = NULL;
            const char **enum_binding_type_names = NULL;
            size_t enum_bind_count = 0;
            const char *enum_bindings_buf[8];
            const char *enum_binding_type_names_buf[8];
            if (transpiler_match_is_enum_variant_destructor(pattern_node, ctx,
                    &enum_vname, &enum_ename, &enum_bindings,
                    &enum_binding_type_names, &enum_bind_count,
                    enum_bindings_buf, enum_binding_type_names_buf,
                    sizeof(enum_bindings_buf)
                        / sizeof(enum_bindings_buf[0]))) {
                for (size_t b = 0; b < enum_bind_count; b++) {
                    if (enum_bindings == NULL || enum_bindings[b] == NULL)
                        continue;
                    char bt_buf[256];
                    const char *bt_c_type = "int32_t";
                    if (enum_binding_type_names != NULL
                        && enum_binding_type_names[b] != NULL) {
                        if (!transpiler_require_type_name_c_type_copy(
                                ctx, enum_binding_type_names[b],
                                "MIR enum match payload binding",
                                bt_buf, sizeof(bt_buf))) {
                            free(subject);
                            free(cond);
                            free(guard);
                            return NULL;
                        }
                        bt_c_type = bt_buf;
                    }
                    /*
                     * Wildcard `_` bindings are discarded — body never
                     * references them. Rename per-(variant, slot) to avoid
                     * C function-scope redefinition when multiple cases
                     * use `_`. Non-wildcard names keep their identity so
                     * the SSA-map-driven body resolution still works.
                     */
                    const char *emitted_name = enum_bindings[b];
                    char wildcard_buf[64];
                    if (emitted_name != NULL
                        && strcmp(emitted_name, "_") == 0) {
                        int wn = snprintf(wildcard_buf, sizeof(wildcard_buf),
                            "_pgy_match_discard_%s_%zu",
                            enum_vname, b);
                        if (wn > 0
                            && (size_t)wn < sizeof(wildcard_buf)) {
                            emitted_name = wildcard_buf;
                        }
                    }
                    write_indent_to(ctx->out, ctx->indent);
                    codebuf_write(ctx->out,
                        "%s %s = (%s).%s._%zu;\n",
                        bt_c_type, emitted_name, subject, enum_vname, b);
                    if (emitted_name != enum_bindings[b]) {
                        write_indent_to(ctx->out, ctx->indent);
                        codebuf_write(ctx->out, "(void)%s;\n", emitted_name);
                    }
                }
                cond = strdup_fmt("(%s).tag == %s_TAG_%s",
                    subject, enum_ename, enum_vname);
            } else {
                char *pat = emit_expression_with_ssa_map(
                    pattern_node, ctx, ssa_map);
                if (pat != NULL)
                    cond = strdup_fmt("%s == %s", subject, pat);
                free(pat);
            }
        }
    }

    if (guard_node != NULL) {
        guard = emit_expression_with_ssa_map(guard_node, ctx, ssa_map);
        if (guard != NULL && cond != NULL) {
            char *with_guard = guard_payload_assign != NULL
                ? strdup_fmt("(%s) && ((%s), (%s))",
                             cond, guard_payload_assign, guard)
                : strdup_fmt("(%s) && (%s)", cond, guard);
            free(cond);
            cond = with_guard;
        }
    }

    free(guard_payload_assign);
    free(guard);
    free(subject);
    return cond;
}
