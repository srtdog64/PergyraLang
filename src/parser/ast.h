/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PERGYRA_AST_H
#define PERGYRA_AST_H

#include <stdint.h>
#include <stdbool.h>
#include "../lexer/lexer.h"

/*
 * AST node types for Pergyra language constructs
 */
typedef enum
{
    /* Expressions */
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_FUNCTION_CALL,
    AST_MEMBER_ACCESS,
    AST_TYPE_PARAM,
    
    /* Statements */
    AST_LET_DECLARATION,
    AST_SLOT_CLAIM,
    AST_SLOT_WRITE,
    AST_SLOT_READ,
    AST_SLOT_RELEASE,
    AST_WITH_STATEMENT,
    AST_PARALLEL_BLOCK,
    AST_BLOCK,
    
    /* Program */
    AST_PROGRAM
} ASTNodeType;

/*
 * Value types for literal nodes
 */
typedef enum
{
    VALUE_INT,
    VALUE_FLOAT,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_SLOT_REF
} ValueType;

/*
 * Value structure for storing literal values
 */
typedef struct
{
    ValueType type;
    union {
        int64_t int_val;
        double  float_val;
        char   *string_val;
        bool    bool_val;
        struct {
            uint32_t slot_id;
            char    *type_name;
        } slot_ref;
    } data;
} Value;

/*
 * Forward declaration of AST node
 */
typedef struct ASTNode ASTNode;

/*
 * Literal node structure
 */
typedef struct
{
    Value value;
} LiteralNode;

/*
 * Identifier node structure
 */
typedef struct
{
    char *name;
    char *type_name;  /* For type inference or explicit type */
} IdentifierNode;

/*
 * Function call node structure
 */
typedef struct
{
    char     *function_name;
    ASTNode **arguments;
    size_t    arg_count;
    ASTNode  *type_params;  /* Generic type parameters */
} FunctionCallNode;

/*
 * Member access node structure
 */
typedef struct
{
    ASTNode *object;
    char    *member_name;
} MemberAccessNode;

/*
 * Type parameter node structure
 */
typedef struct
{
    char  **type_names;
    size_t  type_count;
} TypeParamNode;

/*
 * Let declaration node structure
 */
typedef struct
{
    char    *variable_name;
    char    *type_name;
    ASTNode *initializer;
} LetDeclarationNode;

/*
 * Slot claim node structure
 */
typedef struct
{
    char *type_name;
    char *slot_variable;
} SlotClaimNode;

/*
 * Slot write node structure
 */
typedef struct
{
    ASTNode *slot_ref;
    ASTNode *value;
} SlotWriteNode;

/*
 * Slot read node structure
 */
typedef struct
{
    ASTNode *slot_ref;
} SlotReadNode;

/*
 * Slot release node structure
 */
typedef struct
{
    ASTNode *slot_ref;
} SlotReleaseNode;

/*
 * With statement node structure
 */
typedef struct
{
    char    *type_name;
    char    *slot_alias;
    ASTNode *body;
} WithStatementNode;

/*
 * Parallel block node structure
 */
typedef struct
{
    ASTNode **statements;
    size_t    statement_count;
} ParallelBlockNode;

/*
 * Block node structure
 */
typedef struct
{
    ASTNode **statements;
    size_t    statement_count;
} BlockNode;

/*
 * Program node structure
 */
typedef struct
{
    ASTNode **declarations;
    size_t    declaration_count;
} ProgramNode;

/*
 * Main AST node structure
 */
struct ASTNode
{
    ASTNodeType type;
    uint32_t    line;
    uint32_t    column;
    
    union {
        LiteralNode        literal;
        IdentifierNode     identifier;
        FunctionCallNode   function_call;
        MemberAccessNode   member_access;
        TypeParamNode      type_param;
        LetDeclarationNode let_declaration;
        SlotClaimNode      slot_claim;
        SlotWriteNode      slot_write;
        SlotReadNode       slot_read;
        SlotReleaseNode    slot_release;
        WithStatementNode  with_statement;
        ParallelBlockNode  parallel_block;
        BlockNode          block;
        ProgramNode        program;
    } data;
};

/*
 * AST node creation functions
 */
ASTNode *ast_create_literal(Value value, uint32_t line, uint32_t column);
ASTNode *ast_create_identifier(const char *name, uint32_t line, uint32_t column);
ASTNode *ast_create_function_call(const char *name, ASTNode **args, size_t arg_count, 
                                  uint32_t line, uint32_t column);
ASTNode *ast_create_member_access(ASTNode *object, const char *member, 
                                  uint32_t line, uint32_t column);
ASTNode *ast_create_let_declaration(const char *var_name, const char *type_name, 
                                    ASTNode *initializer, uint32_t line, uint32_t column);
ASTNode *ast_create_slot_claim(const char *type_name, const char *slot_var, 
                               uint32_t line, uint32_t column);
ASTNode *ast_create_slot_write(ASTNode *slot_ref, ASTNode *value, 
                               uint32_t line, uint32_t column);
ASTNode *ast_create_slot_read(ASTNode *slot_ref, uint32_t line, uint32_t column);
ASTNode *ast_create_slot_release(ASTNode *slot_ref, uint32_t line, uint32_t column);
ASTNode *ast_create_with_statement(const char *type_name, const char *alias, 
                                   ASTNode *body, uint32_t line, uint32_t column);
ASTNode *ast_create_parallel_block(ASTNode **statements, size_t count, 
                                   uint32_t line, uint32_t column);
ASTNode *ast_create_block(ASTNode **statements, size_t count, 
                          uint32_t line, uint32_t column);
ASTNode *ast_create_program(ASTNode **declarations, size_t count);

/*
 * AST node destruction
 */
void ast_destroy_node(ASTNode *node);

/*
 * AST printing for debugging
 */
void        ast_print_node(const ASTNode *node, int indent);
const char *ast_node_type_to_string(ASTNodeType type);

/*
 * Value utility functions
 */
Value value_create_int(int64_t val);
Value value_create_float(double val);
Value value_create_string(const char *val);
Value value_create_bool(bool val);
void  value_destroy(Value *value);
void  value_print(const Value *value);

#endif /* PERGYRA_AST_H */