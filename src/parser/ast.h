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
typedef enum {
    PARAM_MODE_DEFAULT,   /* no qualifier — value type: copy, slot type: move */
    PARAM_MODE_OWN,       /* own — take ownership (move) */
    PARAM_MODE_REF        /* ref — borrow (ReadView, non-owning) */
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
};

/*
 * Main AST node structure
 */
struct ASTNode
{
    ASTNodeType type;
    AccessModifier access;
    bool        has_explicit_access;
    bool        is_exported;
    bool        has_explicit_export;
    bool        is_async_decl;
    
    /* Line and column information */
    uint32_t line;
    uint32_t column;
    char    *origin_path;
    
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
            bool           has_effects_clause;
            uint32_t       declared_effects;
            AccessModifier access;
            bool           has_explicit_access;
            bool           is_action;
            ASTNode**      required_abilities;
            size_t         required_ability_count;
            char*          within_zone;
            char*          causes_effect;
            char**         authorized_by;
            size_t         authorized_by_count;
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
            bool           is_struct;
            NominalDeclKind nominal_kind;
            StructuredComment* doc_comment;  /* Attached documentation */
        } class_decl;

        /* extern "C" { func ...; } */
        struct {
            char*    abi;
            ASTNode** declarations;
            size_t   count;
        } extern_block;
        
        /* Let declaration */
        struct {
            char*    name;
            ASTNode* type;        /* Optional type annotation */
            ASTNode* initializer;
            bool     is_mutable;
            bool     is_alias;
        } let_decl;

        /* type UserId = Int; */
        struct {
            char*    name;
            ASTNode* target_type;
        } type_alias;

        /* let (a, b, c) = expr; — positional destructuring */
        struct {
            char**   names;
            size_t   name_count;
            ASTNode* initializer;
        } let_destructure;
        
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
            char*    label;
            char*    variable;
            ASTNode* range_start;
            ASTNode* range_end;
            ASTNode* iterable;     /* for-in: the collection expression */
            ASTNode* body;
        } for_loop;

        /* While loop */
        struct {
            char*    label;
            ASTNode* condition;
            ASTNode* body;
        } while_loop;

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

        struct {
            char* label;
        } break_stmt;

        struct {
            char* label;
        } continue_stmt;
        
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

        /* Array literal [1, 2, 3] */
        struct {
            ASTNode** elements;
            size_t    count;
        } array_literal;

        /* enum Color { Red, Green, Blue }
         * enum Shape { Circle(Int), Rect(Int, Int), None }  — tagged union */
        struct {
            char*      name;
            char**     variants;        /* variant names */
            ASTNode*** variant_params;  /* variant_params[i] = param type nodes (NULL if no data) */
            size_t*    variant_param_counts; /* param count per variant (0 if no data) */
            size_t     variant_count;
            ASTNode**  methods;
            size_t     method_count;
        } enum_decl;

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
            bool           has_effects_clause;
            uint32_t       declared_effects;
            AccessModifier access;
            bool           is_async;
            StructuredComment* doc_comment;
        } async_func_decl;
        
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
        
        /* Match statement */
        struct {
            ASTNode* subject;           /* match target expression */
            ASTNode** cases;            /* AST_MATCH_CASE array */
            size_t case_count;
            ASTNode* default_body;      /* default block (optional) */
        } match_stmt;

        /* Match case */
        struct {
            ASTNode* pattern;           /* literal value or identifier */
            ASTNode** patterns;         /* OR patterns; pattern aliases patterns[0] */
            size_t pattern_count;
            ASTNode* guard;             /* optional if guard */
            ASTNode* body;              /* case body block */
        } match_case;

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
            bool is_blocking;   /* spawn_blocking: offload to blocking pool */
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
            GenericParams* generic_params;
            WhereClause* where_clause;
            AccessModifier access;
            bool has_explicit_access;
            bool is_innate;
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
            ASTNode* ability_ref;
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
            bool is_dynamic;               /* dyn modifier — runtime vtable swap */
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
        
        /* Roster declaration */
        struct {
            char* name;
            ASTNode** party_slots;     /* Party slots */
            size_t party_count;
            ASTNode** shared_fields;   /* Shared system data */
            size_t shared_count;
            ASTNode** methods;         /* System methods */
            size_t method_count;
            GenericParams* generic_params;
            StructuredComment* doc_comment;
        } roster_decl;
        
        /* Roster slot */
        struct {
            char* slot_name;
            char* party_type;          /* Required party type */
            bool is_array;             /* Array<Party> slot */
        } roster_slot;
        
        /* World declaration */
        struct {
            char* name;
            ASTNode** rosters;       /* Roster instances */
            size_t roster_count;
            ASTNode** zones;           /* Zone instances */
            size_t zone_count;
            ASTNode** shared_fields;   /* World-level data */
            size_t shared_count;
            ASTNode** methods;         /* World methods */
            size_t method_count;
            ASTNode** activations;
            size_t activate_count;
            ASTNode** deactivations;
            size_t deactivate_count;
            ASTNode** maintained_zones;
            size_t maintained_zone_count;
            ASTNode** states;
            size_t state_count;
            StructuredComment* doc_comment;
        } world_decl;
        
        /* World roster instance */
        struct {
            char* slot_name;
            char* roster_type;
            ASTNode* initializer;      /* Optional initialization */
        } world_roster;

        /* World zone instance */
        struct {
            char* slot_name;
            char* zone_type;
            ASTNode* initializer;      /* Optional initialization */
        } world_zone;

        /* World zone activation */
        struct {
            char* zone_slot_name;
            char* state_name;
        } world_activate;

        /* World zone deactivation */
        struct {
            char* zone_slot_name;
            char* state_name;
        } world_deactivate;

        /* World zone maintenance */
        struct {
            char* zone_slot_name;
            char* state_name;
        } world_maintain;

        /* World zone state alias */
        struct {
            char* state_name;
            char* zone_slot_name;
            WorldStateSourceKind source_kind;
            char* detail_name;
            char** input_names;
            size_t input_count;
        } world_state;

        /* Intent declaration */
        struct {
            char* name;
            ASTNode** involves;
            size_t involve_count;
            ASTNode** values;
            size_t value_count;
            ASTNode** steps;
            size_t step_count;
            bool is_concurrent;
            IntentRollbackPolicy rollback_policy;
            ASTNode* priority_expr;
            ASTNode* success_expr;
            ASTNode* failure_expr;
            StructuredComment* doc_comment;
            /* Intent-level defaults (propagated to steps) */
            char** default_who_names;
            size_t default_who_count;
            ASTNode* default_where_type;
        } intent_decl;

        /* Intent participant binding */
        struct {
            char* alias;
            ASTNode* subject_type;
        } intent_involves;

        /* Intent value binding */
        struct {
            char* alias;
            ASTNode* value_type;
        } intent_value;

        /* Intent step */
        struct {
            char* name;
            ASTNode* where_type;
            ASTNode* using_expr;
            ASTNode* intent_expr;
            char* transfer_from_alias;
            char* transfer_to_alias;
            char** who_names;
            size_t who_count;
            ASTNode** on_exprs;
            size_t on_expr_count;
            ASTNode** compensate_exprs;
            size_t compensate_expr_count;
            ASTNode* pre_expr;
            ASTNode* guard_expr;
            ASTNode* post_expr;
            ASTNode* invariant_expr;
            ASTNode** required_abilities;
            size_t required_ability_count;
            char* causes_effect;
            char** authorized_by;
            size_t authorized_by_count;
            ASTNode* expect_expr;
            bool inherited_who_from_action;
            bool inherited_where_from_action;
            bool inherited_requires_from_action;
            bool inherited_causes_from_action;
            bool inherited_authorized_by_from_action;
            bool derived_where_from_transfer;
            bool derived_using_from_transfer;
        } intent_step;

        /* Relation declaration */
        struct {
            char* name;
            ASTNode** slots;
            size_t slot_count;
            ASTNode** refreshes;
            size_t refresh_count;
            ASTNode** shared_fields;
            size_t shared_count;
            ASTNode** methods;
            size_t method_count;
            StructuredComment* doc_comment;
            /* between clause: relation X between Left, Right */
            RelationEndpointKind between_left_kind;
            RelationEndpointKind between_right_kind;
            ASTNode* between_left_type;   /* concrete named/generic type when kind == NAMED */
            ASTNode* between_right_type;  /* concrete named/generic type when kind == NAMED */
            bool between_left_many;   /* true if left[] */
            bool between_right_many;  /* true if right[] */
        } relation_decl;

        /* Effect declaration */
        struct {
            char* name;
            ASTNode** slots;
            size_t slot_count;
            ASTNode** refreshes;
            size_t refresh_count;
            ASTNode** shared_fields;
            size_t shared_count;
            ASTNode** methods;
            size_t method_count;
            StructuredComment* doc_comment;
        } effect_decl;

        /* Zone declaration */
        struct {
            char* name;
            ASTNode** slots;
            size_t slot_count;
            ASTNode** layer_slots;
            size_t layer_slot_count;
            ASTNode** applies;
            size_t apply_count;
            ASTNode** links;
            size_t link_count;
            ASTNode** detaches;
            size_t detach_count;
            ASTNode** unlinks;
            size_t unlink_count;
            ASTNode** refreshes;
            size_t refresh_count;
            ASTNode** maintained_effects;
            size_t maintained_effect_count;
            ASTNode** maintained_relations;
            size_t maintained_relation_count;
            ASTNode** maintained_states;
            size_t maintained_state_count;
            ASTNode** authorities;
            size_t authority_count;
            ASTNode** states;
            size_t state_count;
            ASTNode** shared_fields;
            size_t shared_count;
            ASTNode** methods;
            size_t method_count;
            StructuredComment* doc_comment;
        } zone_decl;

        /* Domain slot */
        struct {
            char* slot_name;
            ASTNode* type;
            bool is_subject;
            bool is_vessel;
            bool is_tobject;
            bool is_binding;
            ASTNode* initializer;
        } domain_slot;

        /* Zone relation/effect slot */
        struct {
            char* slot_name;
            char* layer_type;
            bool is_relation;
            bool is_pool;
            int  pool_capacity;
        } zone_layer_slot;

        /* Zone effect application */
        struct {
            char* effect_slot_name;
            char* target_slot_name;
            char* state_name;
            char* participant_slot_name;
        } zone_apply;

        /* Zone relation link */
        struct {
            char* relation_slot_name;
            char* left_slot_name;
            char* right_slot_name;
            char* state_name;
            char* participant_slot_name;
        } zone_link;

        /* Zone effect detachment */
        struct {
            char* effect_slot_name;
            char* target_slot_name;
            char* state_name;
            char* participant_slot_name;
        } zone_detach;

        /* Zone relation unlink */
        struct {
            char* relation_slot_name;
            char* left_slot_name;
            char* right_slot_name;
            char* state_name;
            char* participant_slot_name;
        } zone_unlink;

        /* Zone object refresh */
        struct {
            char* object_slot_name;
            char* source_slot_name;
            char* participant_slot_name;
            bool requires_dto;
            bool infer_target_kind;
            char** mapped_target_fields;
            char** mapped_source_fields;
            size_t field_map_count;
        } zone_refresh;

        /* Zone effect maintenance rule */
        struct {
            char* effect_slot_name;
            char* target_slot_name;
            char* participant_slot_name;
        } zone_maintain_effect;

        /* Zone relation maintenance rule */
        struct {
            char* relation_slot_name;
            char* left_slot_name;
            char* right_slot_name;
            char* participant_slot_name;
        } zone_maintain_relation;

        /* Zone lifecycle state maintenance rule */
        struct {
            char* state_name;
            char* participant_slot_name;
        } zone_maintain_state;

        /* Zone authority declaration */
        struct {
            char* subject_slot_name;
            ASTNode** required_abilities;
            size_t ability_count;
        } zone_authority;

        /* Zone lifecycle state alias */
        struct {
            char* state_name;
            bool is_relation;
            char* layer_slot_name;
            char* left_or_target_slot_name;
            char* right_slot_name;
        } zone_state;

        /* Event declaration */
        struct {
            char* name;
            ASTNode** params;          /* Event handler parameters */
            size_t param_count;
            ASTNode* return_type;      /* Usually Void */
            AccessModifier access;
        } event_decl;

        /* Event subscribe/unsubscribe */
        struct {
            ASTNode* event;            /* Event reference */
            ASTNode* handler;          /* Handler function/lambda */
        } event_op;

        /* Event invoke */
        struct {
            ASTNode* event;            /* Event reference; internal carrier.
                                        * Surface parser currently parses
                                        * `OnEvent(x)` as a regular AST_CALL
                                        * and this node is used by later
                                        * normalization/lowering paths. */
            ASTNode** arguments;       /* Arguments to pass */
            size_t arg_count;
        } event_invoke;

        /* Event handler type */
        struct {
            ASTNode** param_types;     /* Parameter types */
            size_t param_count;
            ASTNode* return_type;      /* Return type */
        } event_handler_type;

        /* Lambda expression */
        struct {
            ASTNode** params;          /* Lambda parameters */
            size_t param_count;
            ASTNode* body;             /* Expression or block */
            ASTNode* return_type;      /* Optional return type */
            bool is_async;             /* async lambda */
        } lambda_expr;

        /* Import declaration */
        struct {
            char* path;                /* Module path (string or identifier) */
        } import_decl;

        /* use pool; */
        struct {
            char* module_name;         /* Standard library module name */
        } use_decl;

        /* namespace Foo { ... } */
        struct {
            char*    name;
            ASTNode** statements;
            size_t   count;
        } namespace_decl;

        /* unsafe { ... } */
        struct {
            ASTNode* body;           /* Block */
        } unsafe_block;

        /* defer { ... }; or defer <expr>; */
        struct {
            ASTNode* body;           /* Block or expression */
        } defer_stmt;

        /* bind party.slot = RoleName; */
        struct {
            char* party_var;         /* "team" */
            char* slot_name;         /* "fighter" */
            char* role_name;         /* "Warrior" */
        } bind_stmt;
    } data;
};

/* AST creation functions */
ASTNode* ast_create_program(void);
ASTNode* ast_create_function(const char* name);
ASTNode* ast_create_class(const char* name);
ASTNode* ast_create_subject(const char* name);
ASTNode* ast_create_vessel(const char* name);
ASTNode* ast_create_struct(const char* name);
ASTNode* ast_create_object(const char* name);
ASTNode* ast_create_tobject(const char* name);
ASTNode* ast_create_extern_block(const char* abi);
ASTNode* ast_create_let_declaration(const char* name);
ASTNode* ast_create_type_alias(const char* name, ASTNode* target_type);
ASTNode* ast_create_with_statement(void);
ASTNode* ast_create_parallel_block(void);
ASTNode* ast_create_block(void);
ASTNode* ast_create_for_loop(void);
ASTNode* ast_create_while_loop(void);
ASTNode* ast_create_match_statement(void);
ASTNode* ast_create_match_case(void);
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
void ast_destroy_structured_comment(StructuredComment* comment);
void ast_print(ASTNode* node, int indent);
const char* token_type_to_string(PgyTokenType type);

/* Role/Ability system AST creation functions */
ASTNode* ast_create_ability_declaration(const char* name);
ASTNode* ast_create_role_declaration(const char* name);
ASTNode* ast_create_include_statement(const char* role_name);
ASTNode* ast_create_require_field(const char* name);
ASTNode* ast_create_impl_ability(ASTNode* ability_ref);
ASTNode* ast_create_override_func(ASTNode* func_decl);

/* Roster/World system AST creation functions */
ASTNode* ast_create_roster_declaration(const char* name);
ASTNode* ast_create_roster_slot(const char* slot_name, const char* party_type);
ASTNode* ast_create_world_declaration(const char* name);
ASTNode* ast_create_world_roster(const char* slot_name, const char* roster_type);
ASTNode* ast_create_world_zone(const char* slot_name, const char* zone_type);
ASTNode* ast_create_world_activate(const char* zone_slot_name);
ASTNode* ast_create_world_deactivate(const char* zone_slot_name);
ASTNode* ast_create_world_maintain(const char* zone_slot_name);
ASTNode* ast_create_world_state(const char* state_name, const char* zone_slot_name,
                                WorldStateSourceKind source_kind,
                                const char* detail_name);
ASTNode* ast_create_world_state_compose(const char* state_name,
                                        WorldStateSourceKind source_kind,
                                        const char** input_names,
                                        size_t input_count);
ASTNode* ast_create_intent_declaration(const char* name);
ASTNode* ast_create_intent_involves(const char* alias);
ASTNode* ast_create_intent_value(const char* alias);
ASTNode* ast_create_intent_step(const char* name);
ASTNode* ast_create_relation_declaration(const char* name);
ASTNode* ast_create_effect_declaration(const char* name);
ASTNode* ast_create_zone_declaration(const char* name);
ASTNode* ast_create_domain_slot(const char* slot_name, bool is_subject);
ASTNode* ast_create_zone_layer_slot(const char* slot_name, const char* layer_type, bool is_relation);
ASTNode* ast_create_zone_apply(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_link(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_detach(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_unlink(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_refresh(const char* object_slot_name, const char* source_slot_name);
ASTNode* ast_create_zone_maintain_effect(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_maintain_relation(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_maintain_state(const char* state_name);
ASTNode* ast_create_zone_authority(const char* subject_slot_name);
ASTNode* ast_create_zone_state(const char* state_name, bool is_relation,
                               const char* layer_slot_name,
                               const char* left_or_target_slot_name,
                               const char* right_slot_name);

/* Party system AST creation functions */
ASTNode* ast_create_party_declaration(const char* name);
ASTNode* ast_create_role_slot(const char* slot_name);
ASTNode* ast_create_party_shared(const char* name);
ASTNode* ast_create_context_access(const char* method_name, const char* slot_name);
ASTNode* ast_create_party_instance(const char* party_type);

/* Event system AST creation functions */
ASTNode* ast_create_event_declaration(const char* name);
ASTNode* ast_create_event_subscribe(ASTNode* event, ASTNode* handler);
ASTNode* ast_create_event_unsubscribe(ASTNode* event, ASTNode* handler);
ASTNode* ast_create_event_invoke(ASTNode* event);
ASTNode* ast_create_event_handler_type(void);
ASTNode* ast_clone(ASTNode* node);
ASTNode* ast_create_lambda_expression(void);

/* Module system AST creation */
ASTNode* ast_create_import_declaration(const char* path);
ASTNode* ast_create_namespace_declaration(const char* name);
ASTNode* ast_create_unsafe_block(ASTNode* body);
ASTNode* ast_create_defer_statement(ASTNode* body);
ASTNode* ast_create_bind_statement(const char* party_var, const char* slot_name, const char* role_name);

#endif /* PERGYRA_AST_H */
