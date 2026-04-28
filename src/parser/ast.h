/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AST (Abstract Syntax Tree) definitions for Pergyra
 */

#ifndef PERGYRA_AST_H
#define PERGYRA_AST_H

#include "ast_types.h"
#include "../lexer/lexer.h"
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
            bool      is_pin_block;
            bool      pin_view_is_write;
            char*     pin_source_name;
            char*     pin_view_name;
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
            ASTNode*       callee;
            ASTNode**      arguments;
            size_t         arg_count;
            GenericParams* generic_args; /* optional: callee<T, U> type args */
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

        /* Tuple literal (a, b, c) */
        struct {
            ASTNode** elements;
            size_t    count;
        } tuple_literal;

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
            bool is_long;   /* true if source had 'L' suffix (int64_t literal) */
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
            /* Non-NULL when this AST_TYPE represents a tuple type (T, U, V).
             * In that case `name` is set to "Tuple" as a sentinel marker. */
            ASTNode**      tuple_elements;
            size_t         tuple_element_count;
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
            ASTNode** bindings;
            size_t binding_count;
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
            bool derived_where_from_using;
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
            bool derive_target_kind;
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

#include "ast_api.h"
#endif /* PERGYRA_AST_H */
