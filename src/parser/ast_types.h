/*
 * Copyright (c) 2025 Pergyra Language Project
 * Shared AST type declarations.
 */

#ifndef PERGYRA_AST_TYPES_H
#define PERGYRA_AST_TYPES_H
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

typedef enum {
    NOMINAL_DECL_CLASS,
    NOMINAL_DECL_SUBJECT,
    NOMINAL_DECL_VESSEL,
    NOMINAL_DECL_STRUCT,
    NOMINAL_DECL_OBJECT,
    NOMINAL_DECL_TOBJECT
} NominalDeclKind;

typedef enum {
    RELATION_ENDPOINT_NAMED,
    RELATION_ENDPOINT_SUBJECT,
    RELATION_ENDPOINT_OBJECT,
    RELATION_ENDPOINT_CLASS,
    RELATION_ENDPOINT_TOBJECT
} RelationEndpointKind;

typedef enum {
    WORLD_STATE_SOURCE_ZONE,
    WORLD_STATE_SOURCE_PROJECTION,
    WORLD_STATE_SOURCE_LAYER,
    WORLD_STATE_SOURCE_STATE,
    WORLD_STATE_SOURCE_ALL,
    WORLD_STATE_SOURCE_ANY
} WorldStateSourceKind;

typedef enum {
    INTENT_ROLLBACK_FULL,
    INTENT_ROLLBACK_CURRENT,
    INTENT_ROLLBACK_NONE
} IntentRollbackPolicy;

/* Structured comment tags */
typedef enum {
    DOC_TAG_WHAT,
    DOC_TAG_WHY,
    DOC_TAG_ALT,
    DOC_TAG_NEXT,
    DOC_TAG_EFFECTS,
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
    size_t tag_capacity;
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
    AST_EXTERN_BLOCK,
    AST_LET_DECL,
    AST_LET_DESTRUCTURE,   /* let (a, b) = expr; */
    AST_TYPE_ALIAS,
    
    /* Statements */
    AST_WITH_STMT,
    AST_PARALLEL_BLOCK,
    AST_FOR_LOOP,
    AST_WHILE_LOOP,
    AST_IF_STMT,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_ENUM_DECL,
    AST_SELECT_STMT,
    AST_MATCH_STMT,
    AST_MATCH_CASE,
    
    /* Expressions */
    AST_BINARY,
    AST_UNARY,
    AST_CALL,
    AST_MEMBER_ACCESS,
    AST_ARRAY_ACCESS,
    AST_ARRAY_LITERAL,
    AST_TUPLE_LITERAL,     /* (a, b, c) — tuple construction */
    AST_MAP_LITERAL,       /* { key: value, ... } — map construction */
    AST_CAST,              /* expr as Type — scalar conversion */
    AST_TYPE_TEST,         /* expr is Type — scalar type predicate */
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
    AST_CHANNEL_TYPE,
    AST_FUTURE_TYPE,
    
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
    AST_PARTY_INSTANCE,
    
    /* Roster and World */
    AST_ROSTER_DECL,
    AST_SYSTEMIC_SLOT,
    AST_WORLD_DECL,
    AST_WORLD_SYSTEMIC,
    AST_WORLD_ZONE,
    AST_WORLD_ACTIVATE,
    AST_WORLD_DEACTIVATE,
    AST_WORLD_MAINTAIN,
    AST_WORLD_STATE,
    AST_INTENT_DECL,
    AST_INTENT_INVOLVES,
    AST_INTENT_VALUE,
    AST_INTENT_STEP,
    AST_RELATION_DECL,
    AST_EFFECT_DECL,
    AST_ZONE_DECL,
    AST_DOMAIN_SLOT,
    AST_ZONE_LAYER_SLOT,
    AST_ZONE_APPLY,
    AST_ZONE_LINK,
    AST_ZONE_DETACH,
    AST_ZONE_UNLINK,
    AST_ZONE_REFRESH,
    AST_ZONE_MAINTAIN_EFFECT,
    AST_ZONE_MAINTAIN_RELATION,
    AST_ZONE_MAINTAIN_STATE,
    AST_ZONE_AUTHORITY,
    AST_ZONE_STATE,

    /* Event system (C# style) */
    AST_EVENT_DECL,
    AST_EVENT_SUBSCRIBE,
    AST_EVENT_UNSUBSCRIBE,
    AST_EVENT_INVOKE,
    AST_EVENT_HANDLER_TYPE,
    AST_LAMBDA_EXPR,

    /* Module system */
    AST_IMPORT_DECL,
    AST_USE_DECL,
    AST_NAMESPACE_DECL,

    /* Safety */
    AST_UNSAFE_BLOCK,
    AST_TRANSACTION_BLOCK,
    AST_FAIL_STMT,
    AST_DEFER_STMT,

    /* Dynamic role binding */
    AST_BIND_STMT
} ASTNodeType;

/*
 * Generic type parameters
 */
struct GenericParam {
    char*    name;
    ASTNode* constraint;   /* Optional ability-style constraint */
    ASTNode* default_type; /* Optional default type */
};

struct GenericParams {
    GenericParam** params;
    size_t         count;
    size_t         capacity;
};

/*
 * Where clause constraints
 */
struct TypeConstraint {
    char*     type_param;
    ASTNode** bounds;      /* Trait bounds */
    size_t    bound_count;
    size_t    bound_capacity;
};

struct WhereClause {
    TypeConstraint** constraints;
    size_t           count;
    size_t           capacity;
};

/*
 * Function parameter
 */
typedef enum {
    PARAM_MODE_DEFAULT,   /* no qualifier — value type: copy, slot type: move */
    PARAM_MODE_OWN,       /* own — take ownership (move) */
    PARAM_MODE_REF,       /* ref — borrow (ReadView, non-owning) */
    PARAM_MODE_MUT_REF    /* inout value-result mutable parameter */
} ParamMode;

struct FuncParam {
    char*      name;
    ASTNode*   type;
    ASTNode*   default_value;  /* Optional */
    ParamMode  mode;           /* own / ref / default */
};

/*
 * Class field
 */
struct ClassField {
    char*          name;
    ASTNode*       type;
    AccessModifier access;
    bool           has_explicit_access;
    bool           is_mutable;
    bool           is_vessel_field;
    ASTNode*       default_value;  /* optional `= expr` field default */
};

/*
 * Main AST node structure
 */
#endif /* PERGYRA_AST_TYPES_H */
