/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend — converts annotated Pergyra AST to C source code.
 *
 * Strategy:
 *   Pergyra Slot<T>          → PgySlot_<T> struct  (pgy_runtime.h)
 *   ClaimSlot<T>()           → pgy_claim_<t>()
 *   Write(slot, val)         → pgy_write_<t>(&slot, val)
 *   Read(slot)               → pgy_read_<t>(&slot)
 *   Release(slot)            → pgy_release_<t>(&slot)
 *   with slot<T> as s { }   → { PgySlot_T s = ...; ... pgy_release(&s); }
 *   Parallel { A() B() }    → _Pragma("omp parallel sections") { ... }
 *   func F(x: Int) -> Int   → int F(int x)
 *   class Foo { }           → typedef struct Foo { ... } Foo;
 *   let x: Int = 42         → int x = 42;
 */

#ifndef PERGYRA_TRANSPILER_H
#define PERGYRA_TRANSPILER_H

#include <stdio.h>
#include <stdbool.h>
#include "../parser/ast.h"
#include "../compiler/hir.h"
#include "../semantic/type_system.h"
#include "../semantic/semantic.h"

/* -----------------------------------------------------------------
 * Output buffer — grows dynamically
 * ----------------------------------------------------------------- */

typedef struct
{
    char  *data;
    size_t len;
    size_t cap;
} CodeBuf;

CodeBuf *codebuf_create(void);
void     codebuf_destroy(CodeBuf *buf);
void     codebuf_write(CodeBuf *buf, const char *fmt, ...);
void     codebuf_write_raw(CodeBuf *buf, const char *s, size_t n);
bool     codebuf_dump_file(const CodeBuf *buf, const char *path);

/* -----------------------------------------------------------------
 * Slot variable tracking — maps variable name → inner type name
 * ----------------------------------------------------------------- */

#define MAX_SLOT_VARS 256

typedef struct
{
    char name[64];         /* variable name, e.g. "msg"     */
    char inner_type[32];   /* Pergyra type, e.g. "String"   */
    bool is_secure;        /* SecureSlot?                    */
} SlotVarEntry;

typedef struct
{
    char name[64];
    char type_name[128];
} TypedVarEntry;

/* -----------------------------------------------------------------
 * Transpiler context
 * ----------------------------------------------------------------- */

typedef struct
{
    CodeBuf *out;          /* main output buffer            */
    CodeBuf *decls;        /* prototypes and forward decls  */
    CodeBuf *helpers;      /* late helper definitions       */
    int      indent;       /* current indent level          */
    bool     in_parallel;  /* inside a Parallel block       */
    ASTNode  *program;     /* owning AST program            */

    /* Unique counter for anonymous temp variables */
    int      tmp_counter;

    /* Slot variable → inner type mapping */
    SlotVarEntry slot_vars[MAX_SLOT_VARS];
    int          slot_var_count;
    TypedVarEntry typed_vars[MAX_SLOT_VARS];
    int           typed_var_count;

    /* Counter for unique parallel wrapper function names */
    unsigned int  parallel_id;

    /* Parallel variable capture: when emitting a parallel wrapper body,
     * identifiers from the outer scope are accessed through _pctx->name */
    bool  in_parallel_wrapper;
    int   par_capture_slot_end;    /* slot_vars[0..end) are captured  */
    int   par_capture_typed_end;   /* typed_vars[0..end) are captured */
} TranspilerCtx;

TranspilerCtx *transpiler_ctx_create(void);
void           transpiler_ctx_destroy(TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Main entry point
 *
 * Usage:
 *   SemanticResult *sem = semantic_analyze(ast);
 *   HIRProgram *hir = hir_lower(sem->annotated_ast, NULL);
 *   TranspileResult *res = transpile(hir, "out.c");
 * ----------------------------------------------------------------- */

typedef struct
{
    bool  success;
    char *error_message;  /* NULL on success */
} TranspileResult;

TranspileResult *transpile(const HIRProgram *hir, const char *output_path);
void             transpile_result_destroy(TranspileResult *res);

/* -----------------------------------------------------------------
 * Per-node emitters (public for testing)
 * ----------------------------------------------------------------- */

void emit_program(const HIRProgram *hir, TranspilerCtx *ctx);
void emit_statement(ASTNode *node, TranspilerCtx *ctx);
void emit_block(ASTNode *node, TranspilerCtx *ctx);

/* Declarations */
void emit_func_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_class_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_extern_block(ASTNode *node, TranspilerCtx *ctx);
void emit_let_decl(ASTNode *node, TranspilerCtx *ctx);

/* Statements */
void emit_if_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_for_loop(ASTNode *node, TranspilerCtx *ctx);
void emit_while_loop(ASTNode *node, TranspilerCtx *ctx);
void emit_return_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_with_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_parallel_block(ASTNode *node, TranspilerCtx *ctx);

/* Expressions — return a C expression string (caller frees) */
char *emit_expression(ASTNode *node, TranspilerCtx *ctx);
char *emit_call(ASTNode *node, TranspilerCtx *ctx);
char *emit_binary(ASTNode *node, TranspilerCtx *ctx);
char *emit_unary(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Type mapping helpers
 * ----------------------------------------------------------------- */

/* "Int" → "int", "String" → "char*", "Slot<Int>" → "PgySlot_Int" */
const char *pergyra_type_to_c(const char *pergyra_type_name);

/* "Int" → "int", used for slot operation suffixes */
const char *pergyra_primitive_to_c(const char *name);

/* "Slot<Int>" → "Int",  "SecureSlot<String>" → "String" */
const char *slot_inner_type_name(const char *slot_type_name);

/* -----------------------------------------------------------------
 * Built-in call emitters
 * ----------------------------------------------------------------- */

char *emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_write(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_read(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_release(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_log(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);
char *emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Role/Ability system emitters
 * ----------------------------------------------------------------- */

void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Async system emitters
 * ----------------------------------------------------------------- */

void emit_actor_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_select_stmt(ASTNode *node, TranspilerCtx *ctx);
char *emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx);
char *emit_channel_send(ASTNode *node, TranspilerCtx *ctx);
char *emit_channel_recv(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Event system emitters
 * ----------------------------------------------------------------- */

void emit_event_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx);
void emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx);
void emit_event_invoke(ASTNode *node, TranspilerCtx *ctx);
char *emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Role/Ability system emitters
 * ----------------------------------------------------------------- */

void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_include_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_impl_ability(ASTNode *node, TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_H */
