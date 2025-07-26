/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AST (Abstract Syntax Tree) definitions for Pergyra
 */

#ifndef PERGYRA_AST_H
#define PERGYRA_AST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../lexer/lexer.h"

/* Forward declarations */
typedef struct ASTNode ASTNode;
typedef struct GenericParams GenericParams;
typedef struct GenericParam GenericParam;
typedef struct WhereClause WhereClause;
typedef struct TypeConstraint TypeConstraint;
typedef struct FuncParam FuncParam;
typedef struct ClassField ClassField;

/* Access modifiers */
typedef enum {
    ACCESS_PUBLIC,
    ACCESS_PRIVATE,
    ACCESS_PROTECTED
} AccessModifier;

/* Structured comment tags */
typedef enum {
    DOC_TAG_WHAT,
    DOC_TAG_WHY,
    DOC_TAG_ALT,
    DOC_TAG_NEXT,
    DOC_TAG_PARAMS,
    DOC_TAG_RETURNS,
    DOC_TAG_THROWS,
    DOC_TAG_COMPLEXITY,
    DOC_TAG_INVARIANTS,
    DOC_TAG_EXAMPLE
} DocTagType;

typedef struct DocTag {
    DocTagType type;
    char* content;
} DocTag;

typedef struct StructuredComment {
    DocTag** tags;
    size_t tag_count;
    struct StructuredComment* next;  /* Linked list for multiple comment blocks */
} StructuredComment;

/*
 * AST Node Types
 */
typedef enum
{
    /* Program structure */
    AST_PROGRAM,
    AST_BLOCK,
    
    /* Declarations */
    AST_FUNC_DECL,
    AST_CLASS_DECL,
    AST_LET_DECL,
    AST_TYPE_ALIAS,
    AST_ACTOR_DECL,
    
    /* Statements */
    AST_WITH_STMT,
    AST_PARALLEL_BLOCK,
    AST_FOR_LOOP,
    AST_WHILE_LOOP,
    AST_IF_STMT,
    AST_RETURN,
    AST_EXPRESSION_STMT,
    AST_SELECT_STMT,
    
    /* Expressions */
    AST_BINARY,
    AST_UNARY,
    AST_CALL,
    AST_MEMBER_ACCESS,
    AST_ARRAY_ACCESS,
    AST_ASSIGNMENT,
    AST_AWAIT_EXPR,
    AST_CHANNEL_SEND,
    AST_CHANNEL_RECV,
    
    /* Literals */
    AST_NUMBER,
    AST_STRING,
    AST_BOOLEAN,
    AST_IDENTIFIER,
    
    /* Types */
    AST_TYPE,
    AST_GENERIC_TYPE,
    AST_CHANNEL_TYPE,
    AST_FUTURE_TYPE,
    
    /* Slot operations */
    AST_CLAIM_SLOT,
    AST_WRITE_SLOT,
    AST_READ_SLOT,
    AST_RELEASE_SLOT,
    
    /* Async operations */
    AST_ASYNC_BLOCK,
    AST_SPAWN_EXPR,
    AST_TASK_GROUP,
    
    /* Role and Ability system */
    AST_ABILITY_DECL,
    AST_ROLE_DECL,
    AST_INCLUDE_STMT,
    AST_REQUIRE_FIELD,
    AST_IMPL_ABILITY,
    AST_OVERRIDE_FUNC,
    
    /* Party system */
    AST_PARTY_DECL,
    AST_ROLE_SLOT,
    AST_PARTY_SHARED,
    AST_PARTY_METHOD,
    AST_CONTEXT_ACCESS,
    AST_PARTY_INSTANCE
} ASTNodeType;

/*
 * Generic type parameters
 */
struct GenericParam {
    char*    name;
    ASTNode* constraint;   /* Optional trait constraint */
    ASTNode* default_type; /* Optional default type */
};

struct GenericParams {
    GenericParam** params;
    size_t         count;
};

/*
 * Where clause constraints
 */
struct TypeConstraint {
    char*     type_param;
    ASTNode** bounds;      /* Trait bounds */
    size_t    bound_count;
};

struct WhereClause {
    TypeConstraint** constraints;
    size_t           count;
};

/*
 * Function parameter
 */
struct FuncParam {
    char*    name;
    ASTNode* type;
    ASTNode* default_value;  /* Optional */
};

/*
 * Class field
 */
struct ClassField {
    char*          name;
    ASTNode*       type;
    AccessModifier access;
    bool           is_mutable;
};

/*
 * Main AST node structure
 */
struct ASTNode
{
    ASTNodeType type;
    
    /* Line and column information */
    uint32_t line;
    uint32_t column;
    
    /* Node-specific data */
    union {
        /* Program root */
        struct {
            ASTNode** statements;
            size_t    count;
        } program;
        
        /* Function declaration */
        struct {
            char*          name;
            FuncParam**    params;
            size_t         param_count;
            ASTNode*       return_type;
            ASTNode*       body;
            GenericParams* generic_params;
            WhereClause*   where_clause;
            AccessModifier access;
            StructuredComment* doc_comment;  /* Attached documentation */
        } func_decl;
        
        /* Class declaration */
        struct {
            char*          name;
            ClassField**   fields;
            size_t         field_count;
            ASTNode**      methods;
            size_t         method_count;
            GenericParams* generic_params;
            WhereClause*   where_clause;
            StructuredComment* doc_comment;  /* Attached documentation */
        } class_decl;
        
        /* Let declaration */
        struct {
            char*    name;
            ASTNode* type;        /* Optional type annotation */
            ASTNode* initializer;
            bool     is_mutable;
        } let_decl;
        
        /* With statement */
        struct {
            ASTNode* slot_type;
            char*    alias;
            ASTNode* body;
            bool     is_secure;
            char*    security_level;
        } with_stmt;
        
        /* Parallel block */
        struct {
            ASTNode** tasks;
            size_t    task_count;
        } parallel;
        
        /* For loop */
        struct {
            char*    variable;
            ASTNode* range_start;
            ASTNode* range_end;
            ASTNode* body;
        } for_loop;
        
        /* If statement */
        struct {
            ASTNode* condition;
            ASTNode* then_branch;
            ASTNode* else_branch;
        } if_stmt;
        
        /* Return statement */
        struct {
            ASTNode* value;
        } return_stmt;
        
        /* Block */
        struct {
            ASTNode** statements;
            size_t    count;
        } block;
        
        /* Binary operation */
        struct {
            ASTNode* left;
            Token    op;
            ASTNode* right;
        } binary;
        
        /* Unary operation */
        struct {
            Token    op;
            ASTNode* operand;
        } unary;
        
        /* Function call */
        struct {
            ASTNode*  callee;
            ASTNode** arguments;
            size_t    arg_count;
        } call;
        
        /* Member access */
        struct {
            ASTNode* object;
            char*    name;
        } member;
        
        /* Array access */
        struct {
            ASTNode* array;
            ASTNode* index;
        } array_access;
        
        /* Assignment */
        struct {
            ASTNode* target;
            ASTNode* value;
        } assignment;
        
        /* Literals */
        struct {
            double value;
        } number;
        
        struct {
            char* value;
        } string;
        
        struct {
            bool value;
        } boolean;
        
        /* Identifier */
        struct {
            char* name;
        } identifier;
        
        /* Type */
        struct {
            char*          name;
            GenericParams* generic_args;
        } type;
        
        /* Async function declaration */
        struct {
            char*          name;
            FuncParam**    params;
            size_t         param_count;
            ASTNode*       return_type;
            ASTNode*       body;
            GenericParams* generic_params;
            WhereClause*   where_clause;
            AccessModifier access;
            bool           is_async;
            StructuredComment* doc_comment;
        } async_func_decl;
        
        /* Actor declaration */
        struct {
            char*          name;
            ClassField**   fields;
            size_t         field_count;
            ASTNode**      methods;
            size_t         method_count;
            GenericParams* generic_params;
            StructuredComment* doc_comment;
        } actor_decl;
        
        /* Await expression */
        struct {
            ASTNode* expression;
        } await_expr;
        
        /* Channel operations */
        struct {
            ASTNode* channel;
            ASTNode* value;
        } channel_send;
        
        struct {
            ASTNode* channel;
        } channel_recv;
        
        /* Select statement */
        struct {
            ASTNode** cases;
            size_t case_count;
            ASTNode* default_case;
        } select_stmt;
        
        /* Async block */
        struct {
            ASTNode** statements;
            size_t statement_count;
        } async_block;
        
        /* Spawn expression */
        struct {
            ASTNode* function;
            ASTNode** arguments;
            size_t arg_count;
        } spawn_expr;
        
        /* Channel type */
        struct {
            ASTNode* element_type;
            ASTNode* capacity;  /* Optional, null for unbuffered */
        } channel_type;
        
        /* Future type */
        struct {
            ASTNode* value_type;
        } future_type;
        
        /* Task group */
        struct {
            ASTNode** tasks;
            size_t task_count;
            bool wait_all;  /* true for all, false for any */
        } task_group;
        
        /* Ability declaration */
        struct {
            char* name;
            ASTNode** require_fields;
            size_t require_count;
            ASTNode** methods;
            size_t method_count;
            StructuredComment* doc_comment;
        } ability_decl;
        
        /* Role declaration */
        struct {
            char* name;
            ASTNode* for_type;  /* The struct this role is for */
            ASTNode** includes; /* Other roles to include */
            size_t include_count;
            ASTNode** impl_abilities;  /* Abilities implemented */
            size_t impl_count;
            ASTNode* parallel_block;  /* Optional parallel on block */
            GenericParams* generic_params;
            WhereClause* where_clause;
            StructuredComment* doc_comment;
        } role_decl;
        
        /* Include statement */
        struct {
            char* role_name;
            GenericParams* type_args;  /* For generic roles */
        } include_stmt;
        
        /* Require field */
        struct {
            char* name;
            ASTNode* type;
        } require_field;
        
        /* Impl ability block */
        struct {
            char* ability_name;
            ASTNode** methods;
            size_t method_count;
        } impl_ability;
        
        /* Override function */
        struct {
            ASTNode* func_decl;
            bool calls_super;
        } override_func;
        
        /* Party declaration */
        struct {
            char* name;
            ASTNode** role_slots;      /* Required roles */
            size_t role_count;
            ASTNode** shared_fields;   /* Shared data */
            size_t shared_count;
            ASTNode** methods;         /* Party methods */
            size_t method_count;
            ASTNode* extends;          /* Parent party (optional) */
            GenericParams* generic_params;
            StructuredComment* doc_comment;
        } party_decl;
        
        /* Role slot in party */
        struct {
            char* slot_name;
            ASTNode** required_abilities;  /* Ability requirements */
            size_t ability_count;
            bool is_array;                 /* Array<T> slot */
        } role_slot;
        
        /* Party shared field */
        struct {
            char* name;
            ASTNode* type;
            ASTNode* initializer;
            AccessModifier access;
        } party_shared;
        
        /* Context access */
        struct {
            char* method_name;     /* GetRole, FindRole, etc */
            char* role_slot_name;  /* Which slot to access */
            ASTNode* ability_type; /* Expected ability */
        } context_access;
        
        /* Party instance creation */
        struct {
            char* party_type;
            struct {
                char* slot_name;
                ASTNode* value;
            }* assignments;
            size_t assignment_count;
        } party_instance;
    } data;
};

/* AST creation functions */
ASTNode* ast_create_program(void);
ASTNode* ast_create_function(const char* name);
ASTNode* ast_create_class(const char* name);
ASTNode* ast_create_let_declaration(const char* name);
ASTNode* ast_create_with_statement(void);
ASTNode* ast_create_parallel_block(void);
ASTNode* ast_create_block(void);
ASTNode* ast_create_for_loop(void);
ASTNode* ast_create_if_statement(void);
ASTNode* ast_create_return_statement(void);
ASTNode* ast_create_binary(ASTNode* left, Token op, ASTNode* right);
ASTNode* ast_create_unary(Token op, ASTNode* operand);
ASTNode* ast_create_call(ASTNode* callee);
ASTNode* ast_create_member_access(ASTNode* object, const char* member);
ASTNode* ast_create_array_access(ASTNode* array, ASTNode* index);
ASTNode* ast_create_assignment(ASTNode* target, ASTNode* value);
ASTNode* ast_create_number(const char* value);
ASTNode* ast_create_string(const char* value);
ASTNode* ast_create_boolean(bool value);
ASTNode* ast_create_identifier(const char* name);
ASTNode* ast_create_type(const char* name);

/* Async AST creation functions */
ASTNode* ast_create_async_function(const char* name, bool is_async);
ASTNode* ast_create_actor(const char* name);
ASTNode* ast_create_await_expression(ASTNode* expression);
ASTNode* ast_create_channel_send(ASTNode* channel, ASTNode* value);
ASTNode* ast_create_channel_recv(ASTNode* channel);
ASTNode* ast_create_select_statement(void);
ASTNode* ast_create_async_block(void);
ASTNode* ast_create_spawn_expression(ASTNode* function);
ASTNode* ast_create_channel_type(ASTNode* element_type);
ASTNode* ast_create_future_type(ASTNode* value_type);
ASTNode* ast_create_task_group(bool wait_all);

/* AST manipulation functions */
void ast_add_statement(ASTNode* parent, ASTNode* statement);
void ast_add_parallel_task(ASTNode* parallel, ASTNode* task);
void ast_add_argument(ASTNode* call, ASTNode* arg);

/* AST utility functions */
void ast_destroy(ASTNode* node);
void ast_print(ASTNode* node, int indent);
const char* token_type_to_string(TokenType type);

#endif /* PERGYRA_AST_H */
