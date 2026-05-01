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

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("??n"); g_pass++; } \
        else      { printf("?? (line %d)\n", __LINE__); g_fail++; } \
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
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            *mir_out = mir_lower(*hir_out, *rir_out, &mir_error);
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
    for (size_t i = 0; i < block->source_statement_count; i++) {
        if (block->source_statements[i] != NULL
            && block->source_statements[i]->type == type) {
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
                && inst->ast != NULL
                && inst->ast->type == AST_FOR_LOOP
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

#include "tests/mir/test_mir_lowering_part_a.cases.h"
#include "tests/mir/test_mir_lowering_part_b.cases.h"

static void
test_mir_lowering(void)
{
    test_mir_lowering_part_a();
    test_mir_lowering_part_b();
}

int
main(void)
{
    printf("=== Pergyra MIR Lowering Test Suite ===\n");
    test_mir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
