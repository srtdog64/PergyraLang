/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"
#include "compiler/rir.h"
#include "compiler/mir.h"
#include "compiler/mir_parallel_capture_facts.h"
#include "compiler/mir_abi_layout.h"
#include "compiler/mir_dce.h"
#include "compiler/mir_decl_headers.h"
#include "compiler/mir_type_helpers.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static bool
lower_mir_from_source(const char *source, HIRProgram **hir_out, RIRProgram **rir_out, MIRProgram **mir_out)
{
    bool ok = false;
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    char *rir_error = NULL;
    char *mir_error = NULL;

    *hir_out = NULL;
    *rir_out = NULL;
    *mir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        if (sem->resource_flow_fact_count > 0
            || sem->function_param_flow_fact_count > 0) {
            *hir_out = hir_lower_with_resource_and_param_flow_facts(
                sem->annotated_ast,
                sem->resource_flow_facts,
                sem->resource_flow_fact_count,
                sem->function_param_flow_facts,
                sem->function_param_flow_fact_count,
                &hir_error);
        } else {
            *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        }
        if (*hir_out != NULL
            && !hir_attach_iteration_type_facts(
                *hir_out,
                sem->iteration_type_facts,
                sem->iteration_type_fact_count,
                &hir_error)) {
            hir_destroy(*hir_out);
            *hir_out = NULL;
        }
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            *mir_out = mir_lower(*hir_out, *rir_out, sem, &mir_error);
    }

    ok = (*hir_out != NULL && *rir_out != NULL && *mir_out != NULL);
    if (!ok) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
        if (mir_error != NULL)
            fprintf(stderr, "MIR lowering error: %s\n", mir_error);
    }

    free(hir_error);
    free(rir_error);
    free(mir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}

static const MIRRoutine *
find_mir_routine(const MIRProgram *mir, const char *name, MIRScopeKind kind)
{
    if (mir == NULL)
        return NULL;
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (mir->routines[i].kind == kind
            && mir->routines[i].name != NULL
            && strcmp(mir->routines[i].name, name) == 0) {
            return &mir->routines[i];
        }
    }
    return NULL;
}

static MIRRoutine *
find_mir_routine_mut(MIRProgram *mir, const char *name, MIRScopeKind kind)
{
    if (mir == NULL)
        return NULL;
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (mir->routines[i].kind == kind
            && mir->routines[i].name != NULL
            && strcmp(mir->routines[i].name, name) == 0) {
            return &mir->routines[i];
        }
    }
    return NULL;
}

static bool
block_has_inst_kind(const MIRBasicBlock *block, MIRInstKind kind)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        if (block->instructions[i].kind == kind)
            return true;
    }
    return false;
}

static bool
block_has_entry_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->ssa_entry_value_count; i++) {
        if (block->ssa_entry_values[i] != NULL
            && strncmp(block->ssa_entry_values[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_exit_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->ssa_exit_value_count; i++) {
        if (block->ssa_exit_values[i] != NULL
            && strncmp(block->ssa_exit_values[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_inst_named(const MIRBasicBlock *block, const char *name)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        if (block->instructions[i].name != NULL
            && strcmp(block->instructions[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool
test_block_has_forward_edge_to(const MIRBasicBlock *block, size_t target)
{
    if (block == NULL)
        return false;
    return (block->has_succ_true && block->succ_true == target)
        || (block->has_succ_false && block->succ_false == target)
        || (block->has_cleanup_succ && block->cleanup_succ == target)
        || (block->has_rollback_succ && block->rollback_succ == target)
        || (block->has_invalidation_succ && block->invalidation_succ == target);
}

static bool
block_rename_inst_named(MIRBasicBlock *block, const char *old_name, const char *new_name)
{
    if (block == NULL || old_name == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        MIRInstruction *inst = &block->instructions[i];
        if (inst->name != NULL && strcmp(inst->name, old_name) == 0) {
            inst->name = new_name;
            return true;
        }
    }
    return false;
}

static bool
block_has_inst_named_with_slot(const MIRBasicBlock *block, const char *name, const char *slot_anchor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->name != NULL
            && strcmp(inst->name, name) == 0
            && inst->slot_anchor != NULL
            && strcmp(inst->slot_anchor, slot_anchor) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_inst_named_args(const MIRBasicBlock *block,
                          const char *name,
                          const char *arg0,
                          const char *arg1)
{
    if (block == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->name == NULL || strcmp(inst->name, name) != 0)
            continue;
        if (arg0 != NULL) {
            if (inst->arg0 == NULL || strcmp(inst->arg0, arg0) != 0)
                continue;
        }
        if (arg1 != NULL) {
            if (inst->arg1 == NULL || strcmp(inst->arg1, arg1) != 0)
                continue;
        }
        return true;
    }
    return false;
}

static bool
routine_has_stmt_call_named(const MIRRoutine *routine, const char *callee_name)
{
    if (routine == NULL || callee_name == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const ASTNode *ast = inst->ast;
            if (inst->kind != MIR_INST_STMT || ast == NULL || ast->type != AST_CALL)
                continue;
            if (ast->data.call.callee == NULL
                || ast->data.call.callee->type != AST_IDENTIFIER
                || ast->data.call.callee->data.identifier.name == NULL) {
                continue;
            }
            if (strcmp(ast->data.call.callee->data.identifier.name, callee_name) == 0)
                return true;
        }
    }
    return false;
}

static bool
routine_has_stmt_call_fact_named(const MIRRoutine *routine, const char *callee_name)
{
    if (routine == NULL || callee_name == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_STMT
                && inst->arg0 != NULL
                && strcmp(inst->arg0, callee_name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool
routine_has_def_call_fact_named(const MIRRoutine *routine, const char *callee_name)
{
    if (routine == NULL || callee_name == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_DEF
                && inst->arg1 != NULL
                && strcmp(inst->arg1, callee_name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool
routine_has_stmt_ast_type(const MIRRoutine *routine, ASTNodeType type)
{
    if (routine == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_STMT
                && inst->ast != NULL
                && inst->ast->type == type) {
                return true;
            }
        }
    }
    return false;
}

static bool
block_source_has_stmt_type(const MIRBasicBlock *block, ASTNodeType type)
{
    if (block == NULL)
        return false;
    if (block->source_statement_inventory.items == NULL)
        return false;
    for (size_t i = 0; i < block->source_statement_inventory.count; i++) {
        ASTNode *stmt = block->source_statement_inventory.items[i];
        if (stmt != NULL && stmt->type == type) {
            return true;
        }
    }
    return false;
}

static bool
routine_has_inst_kind(const MIRRoutine *routine, MIRInstKind kind)
{
    if (routine == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block_has_inst_kind(block, kind))
            return true;
    }
    return false;
}

static bool
routine_has_complete_loop_init_for(const MIRRoutine *routine, const char *variable)
{
    if (routine == NULL || variable == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_LOOP_INIT
                && inst->arg0 != NULL
                && strcmp(inst->arg0, variable) == 0
                && inst->expr0 != NULL
                && inst->expr1 != NULL) {
                return true;
            }
        }
    }
    return false;
}

static bool
routine_has_complete_loop_branch_for(const MIRRoutine *routine, const char *variable)
{
    if (routine == NULL || variable == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_BRANCH
                && (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                    || inst->branch_shape == MIR_BRANCH_FOR_IN)
                && inst->arg0 != NULL
                && strcmp(inst->arg0, variable) == 0
                && inst->expr0 != NULL
                && inst->expr1 != NULL) {
                return true;
            }
        }
    }
    return false;
}

static bool
routine_has_return_source_terminator(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_RETURN
                && inst->has_source_terminator_kind
                && inst->source_terminator_kind == HIR_BLOCK_RETURN
                && inst->source_terminator_kind != HIR_BLOCK_GOTO) {
                return true;
            }
        }
    }
    return false;
}

static bool
block_has_phi_result_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_PHI
            && inst->result_name != NULL
            && strncmp(inst->result_name, prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_renamed_local_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->renamed_local_count; i++) {
        if (block->renamed_locals[i] != NULL
            && strncmp(block->renamed_locals[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool
block_has_use_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        for (size_t j = 0; j < inst->use_count; j++) {
            if (inst->uses[j] != NULL
                && strncmp(inst->uses[j], prefix, prefix_len) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool
block_has_live_out_prefix(const MIRBasicBlock *block, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    for (size_t i = 0; i < block->live_out_name_count; i++) {
        if (block->live_out_names[i] != NULL
            && strncmp(block->live_out_names[i], prefix, prefix_len) == 0) {
            return true;
        }
    }
    return false;
}

static const MIRValueSummary *
find_value_summary_prefix_with_use(const MIRRoutine *routine, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    const MIRValueSummary *best = NULL;
    if (routine == NULL)
        return NULL;
    for (size_t i = 0; i < routine->value_summary_count; i++) {
        const MIRValueSummary *summary = &routine->value_summaries[i];
        if (summary->name != NULL
            && strncmp(summary->name, prefix, prefix_len) == 0) {
            if (best == NULL || summary->use_count > best->use_count)
                best = summary;
        }
    }
    return best;
}

static const MIRValueSummary *
find_value_summary_with_slot(const MIRRoutine *routine, const char *prefix, const char *slot_anchor)
{
    size_t prefix_len = strlen(prefix);
    if (routine == NULL)
        return NULL;
    for (size_t i = 0; i < routine->value_summary_count; i++) {
        const MIRValueSummary *summary = &routine->value_summaries[i];
        if (summary->name != NULL
            && strncmp(summary->name, prefix, prefix_len) == 0
            && summary->slot_anchor != NULL
            && strcmp(summary->slot_anchor, slot_anchor) == 0) {
            return summary;
        }
    }
    return NULL;
}

static void
trim_line_end(char *line)
{
    size_t len;
    if (line == NULL)
        return;
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

static bool
format_expected_native_resource_row_suffix(size_t native_index,
                                           char *dst,
                                           size_t dst_size)
{
    const char *domain = mir_abi_resource_runtime_row_domain(native_index);
    const char *type_name = mir_abi_resource_runtime_row_type_name(native_index);
    const char *operation = mir_abi_resource_runtime_row_operation(native_index);
    const char *symbol = mir_abi_resource_runtime_row_symbol(native_index);
    const char *target_kind = mir_abi_resource_runtime_row_target_kind(native_index);
    const char *materialization =
        mir_abi_resource_runtime_row_materialization(native_index);
    const char *call_shape =
        mir_abi_resource_runtime_row_call_shape(native_index);
    int written;

    if (domain == NULL || type_name == NULL || operation == NULL ||
        symbol == NULL || target_kind == NULL || materialization == NULL ||
        call_shape == NULL || dst == NULL || dst_size == 0)
        return false;

    written = snprintf(dst, dst_size,
                       "%s|%s.%s|%s|%s|%s|%s",
                       domain, type_name, operation, symbol, target_kind,
                       materialization, call_shape);
    return written >= 0 && (size_t)written < dst_size;
}

static bool
runtime_call_abi_expected_native_rows_match(void)
{
    char path[1024];
    char line[512];
    char expected[512];
    size_t native_index = 0;
    FILE *fp;
    int written = snprintf(
        path,
        sizeof(path),
        "%s/src/self_hosted/compiler/expected/runtime_call_abi_rows.txt",
        PGY_PROJECT_ROOT);

    if (written < 0 || (size_t)written >= sizeof(path))
        return false;

    fp = fopen(path, "rb");
    if (fp == NULL)
        return false;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *row_suffix;
        trim_line_end(line);

        if (strstr(line, "|native-resource|") != NULL) {
            row_suffix = strchr(line, '|');
            if (row_suffix == NULL ||
                native_index >= mir_abi_resource_runtime_row_count() ||
                !format_expected_native_resource_row_suffix(native_index,
                                                            expected,
                                                            sizeof(expected)) ||
                strcmp(row_suffix + 1, expected) != 0) {
                fclose(fp);
                return false;
            }
            native_index++;
        }
    }

    fclose(fp);
    return native_index == mir_abi_resource_runtime_row_count();
}

static void
test_mir_carries_function_param_flow_summary(void)
{
    static const char *source =
        "subject Vec2 { let x: Int; let y: Int; }\n"
        "func Recur(ref slot: Slot<Vec2>) -> Void {\n"
        "  Recur(slot);\n"
        "  Write(slot, Vec2(1, 2));\n"
        "}\n"
        "func Main() -> Void {\n"
        "  let slot: Slot<Vec2> = Vec2(0, 0);\n"
        "  Recur(slot);\n"
        "  Release(slot);\n"
        "}\n";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    const MIRRoutine *recur;
    char *error = NULL;
    bool carried = false;
    bool validated = false;

    if (!lower_mir_from_source(source, &hir, &rir, &mir)) {
        TEST("MIR carries HIR function parameter flow summaries");
        EXPECT(false);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        return;
    }
    recur = find_mir_routine(mir, "Recur", MIR_SCOPE_FUNCTION);
    if (recur != NULL)
        carried = mir_routine_function_param_flow_summary_count(recur) > 0
            && recur->source_syntax_id != 0
            && mir_routine_function_param_flow_summary_at(recur, 0) != NULL;
    validated = mir_validate(mir, &error);
    TEST("MIR carries HIR function parameter flow summaries by stable identity");
    EXPECT(carried && validated);
    free(error);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}

#include "tests/mir/test_mir_lowering_part_a_1.cases.h"
#include "tests/mir/test_mir_lowering_part_a_2.cases.h"
#include "tests/mir/test_mir_lowering_part_b_1.cases.h"
#include "tests/mir/test_mir_lowering_part_b_2.cases.h"
#include "tests/mir/test_mir_lowering_part_c.cases.h"
#include "tests/mir/test_mir_lowering_part_c_2.cases.h"
#include "tests/mir/test_mir_lowering_part_c_3.cases.h"
#include "tests/mir/test_mir_lowering_part_d.cases.h"
#include "tests/mir/test_mir_lowering_part_d_2.cases.h"
#include "tests/mir/test_mir_lowering_part_e.cases.h"
#include "tests/mir/test_mir_lowering_part_g.cases.h"
#include "tests/mir/test_mir_lowering_part_h.cases.h"
#include "tests/mir/test_mir_lowering_part_h_2.cases.h"
#include "tests/mir/test_mir_lowering_part_i.cases.h"

static void
test_mir_lowering(void)
{
    test_mir_carries_function_param_flow_summary();
    test_mir_lowering_part_a();
    test_mir_lowering_part_b();
    test_mir_lowering_part_c();
    test_mir_lowering_part_c_2();
    test_mir_lowering_part_c_3();
    test_mir_lowering_part_d();
    test_mir_lowering_part_d_2();
    test_mir_lowering_part_e();
    test_mir_lowering_part_g();
    test_mir_lowering_part_h();
    test_mir_lowering_part_h_2();
    test_mir_lowering_part_i();
}

int
main(void)
{
    printf("=== Pergyra MIR Lowering Test Suite ===\n");
    test_mir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
