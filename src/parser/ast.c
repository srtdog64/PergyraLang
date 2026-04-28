/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST (Abstract Syntax Tree) implementation
 */

#include "ast.h"
#include <stdlib.h>

// ============= AST mutation helpers =============

void ast_add_statement(ASTNode* parent, ASTNode* statement) {
    if (parent->type == AST_PROGRAM) {
        parent->data.program.count++;
        parent->data.program.statements = realloc(
            parent->data.program.statements,
            parent->data.program.count * sizeof(ASTNode*)
        );
        parent->data.program.statements[parent->data.program.count - 1] = statement;
    } else if (parent->type == AST_BLOCK) {
        parent->data.block.count++;
        parent->data.block.statements = realloc(
            parent->data.block.statements,
            parent->data.block.count * sizeof(ASTNode*)
        );
        parent->data.block.statements[parent->data.block.count - 1] = statement;
    } else if (parent->type == AST_EXTERN_BLOCK) {
        parent->data.extern_block.count++;
        parent->data.extern_block.declarations = realloc(
            parent->data.extern_block.declarations,
            parent->data.extern_block.count * sizeof(ASTNode*)
        );
        parent->data.extern_block.declarations[parent->data.extern_block.count - 1] = statement;
    } else if (parent->type == AST_NAMESPACE_DECL) {
        parent->data.namespace_decl.count++;
        parent->data.namespace_decl.statements = realloc(
            parent->data.namespace_decl.statements,
            parent->data.namespace_decl.count * sizeof(ASTNode*)
        );
        parent->data.namespace_decl.statements[parent->data.namespace_decl.count - 1] = statement;
    }
}

// parallel ?쒖뒪??異붽?
void ast_add_parallel_task(ASTNode* parallel, ASTNode* task) {
    if (parallel->type != AST_PARALLEL_BLOCK) return;
    
    parallel->data.parallel.task_count++;
    parallel->data.parallel.tasks = realloc(
        parallel->data.parallel.tasks,
        parallel->data.parallel.task_count * sizeof(ASTNode*)
    );
    parallel->data.parallel.tasks[parallel->data.parallel.task_count - 1] = task;
}

// ?⑥닔 ?몄옄 異붽?
void ast_add_argument(ASTNode* call, ASTNode* arg) {
    if (call->type != AST_CALL) return;
    
    call->data.call.arg_count++;
    call->data.call.arguments = realloc(
        call->data.call.arguments,
        call->data.call.arg_count * sizeof(ASTNode*)
    );
    call->data.call.arguments[call->data.call.arg_count - 1] = arg;
}
