/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AST (Abstract Syntax Tree) definitions for Pergyra
 */

#ifndef PERGYRA_AST_H
#define PERGYRA_AST_H

#include "ast_types.h"
#include "ast_domain_data.h"
#include "ast_module_data.h"
#include "../lexer/lexer.h"

struct ASTNode
{
    ASTNodeType type;
    AccessModifier access;
    bool        has_explicit_access;
    bool        is_exported;
    bool        has_explicit_export;
    bool        is_async_decl;
    uint32_t    stable_id;
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
            size_t    capacity;
        } program;
        
        /* Function declaration */
        struct {
            char*          name;
            FuncParam**    params;
            size_t         param_count;
            size_t         param_capacity;
            ASTNode*       return_type;
            char*          semantic_return_type_name;
            ASTNode*       body;
            GenericParams* generic_params;
            WhereClause*   where_clause;
            bool           has_effects_clause;
            uint32_t       declared_effects;
            bool           has_caps_clause;       /* `with caps ...` present */
            uint32_t       declared_capabilities; /* PGY_CAP_* bits declared */
            AccessModifier access;
            bool           has_explicit_access;
            bool           is_action;
            ASTNode**      required_abilities;
            size_t         required_ability_count;
            size_t         required_ability_capacity;
            char*          within_zone;
            char*          causes_effect;
            char**         authorized_by;
            size_t         authorized_by_count;
            size_t         authorized_by_capacity;
            StructuredComment* doc_comment;  /* Attached documentation */
        } func_decl;
        
        /* Class declaration */
        struct {
            char*          name;
            ClassField**   fields;
            size_t         field_count;
            size_t         field_capacity;
            ASTNode**      methods;
            size_t         method_count;
            size_t         method_capacity;
            GenericParams* generic_params;
            WhereClause*   where_clause;
            bool           is_struct;
            NominalDeclKind nominal_kind;
            StructuredComment* doc_comment;  /* Attached documentation */
            /* Class-body destructuring field groups: each entry is an
             * AST_LET_DESTRUCTURE node (names[] + initializer). The placeholder
             * fields for each name also live in `fields`; semantic resolves
             * their types from the initializer's tuple, and the constructor
             * evaluates each initializer once and assigns the elements. */
            ASTNode**      field_destructures;
            size_t         field_destructure_count;
            size_t         field_destructure_capacity;
        } class_decl;

        /* extern "C" { func ...; } */
        struct {
            char*    abi;
            ASTNode** declarations;
            size_t   count;
            size_t   capacity;
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
            size_t   name_capacity;
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
            size_t    task_capacity;
            /* Join form (docs/181 SS1, rung 0): `parallel (x in xs)`.
             * tasks[0] is the replicated body; the runtime fans out one
             * task per element. */
            bool      is_join_form;
            char*     join_element;    /* element binding name */
            ASTNode*  join_collection; /* collection expr; index form: range start */
            /* Index form (docs/181 R1): `parallel (i in lo..hi)`.
             * Non-NULL range end selects the index mode; join_collection
             * then holds the range start expression. */
            ASTNode*  join_range_end;
            /* Index-disjointness facts (docs/181 R1): checker-sealed
             * names of arrays whose every body access is `name[binding]`
             * (task i owns index i). Emitters admit array captures from
             * this list only. */
            char**    join_index_arrays;
            size_t    join_index_array_count;
            /* Snapshot-read facts (docs/181 R5): checker-sealed names of
             * arrays the body never writes and only ever uses as
             * `name[<any expr>]` reads. Free-index reads are safe
             * against a written array only if the backings are
             * disjoint; the emitters close that residual with a
             * fail-closed alias check at fan-out entry. */
            char**    join_readonly_arrays;
            size_t    join_readonly_array_count;
            /* Expression form (docs/181 R2): checker-sealed primitive
             * result type name of the final `give` (NULL = statement
             * form). Emitters derive Array<R> from this fact instead of
             * re-inferring the body. */
            char*     join_give_type_name;
            /* Reduce combinator (docs/181 R4): parse-time closed set
             * ("sum"/"product"/"min"/"max"; NULL = all-join collect).
             * The expression form folds per-task give values in INDEX
             * order (a fixed left fold, so Float results are
             * deterministic and byte-equal across backends); Int/Long
             * lanes ride the checked-arith exports. */
            char*     join_reduce_op;
            /* any-join (docs/181 R3): first give wins a CAS on a shared
             * decision cell; later tasks skip at the entry safe point
             * and queued tasks are cancel-hinted (cooperative protocol,
             * SS2.4 -- loop back-edge / channel-entry safe points are a
             * later slice). Element mode + expression form only. */
            bool      join_is_any;
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

        /* Give statement (docs/181 R2): the per-task result of an
         * expression-form parallel join body. */
        struct {
            ASTNode* value;
        } give_stmt;

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
            size_t    capacity;
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
            char**         arg_names; /* optional per-argument names; NULL entry means positional */
            size_t         arg_count;
            size_t         arg_capacity;
            GenericParams* generic_args; /* optional: callee<T, U> type args */
            uint32_t       semantic_callee_decl_id;
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

        /* Map literal { key: value, ... } */
        struct {
            ASTNode** keys;
            ASTNode** values;
            size_t    count;
        } map_literal;

        /* Set literal { a, b, c } / {} */
        struct {
            ASTNode** elements;
            size_t    count;
        } set_literal;

        /* Cast expr as Type */
        struct {
            ASTNode* operand;
            char*    target_type;
        } cast;

        /* Type-test expr is Type */
        struct {
            ASTNode* operand;
            char*    target_type;
        } type_test;

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
            size_t     method_capacity;
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
            bool is_float;  /* true if source had a decimal point */
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
            size_t         param_capacity;
            ASTNode*       return_type;
            char*          semantic_return_type_name;
            ASTNode*       body;
            GenericParams* generic_params;
            WhereClause*   where_clause;
            bool           has_effects_clause;
            uint32_t       declared_effects;
            bool           has_caps_clause;       /* `with caps ...` present */
            uint32_t       declared_capabilities; /* PGY_CAP_* bits declared */
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
            size_t case_capacity;
            ASTNode* default_case;
        } select_stmt;
        
        /* Match statement */
        struct {
            ASTNode* subject;           /* match target expression */
            ASTNode** cases;            /* AST_MATCH_CASE array */
            size_t case_count;
            size_t case_capacity;
            ASTNode* default_body;      /* default block (optional) */
        } match_stmt;

        /* Match case */
        struct {
            ASTNode* pattern;           /* literal value or identifier */
            ASTNode** patterns;         /* OR patterns; pattern aliases patterns[0] */
            size_t pattern_count;
            size_t pattern_capacity;
            ASTNode* guard;             /* optional if guard */
            ASTNode* body;              /* case body block */
        } match_case;

        /* Async block */
        struct {
            ASTNode** statements;
            size_t statement_count;
            size_t statement_capacity;
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
            size_t require_capacity;
            ASTNode** methods;
            size_t method_count;
            size_t method_capacity;
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
            size_t include_capacity;
            ASTNode** impl_abilities;  /* Abilities implemented */
            size_t impl_count;
            size_t impl_capacity;
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
            size_t method_capacity;
        } impl_ability;
        
        /* Override function */
        struct {
            ASTNode* func_decl;
            bool calls_super;
        } override_func;
        
        ASTPartyDeclData party_decl;
        ASTRoleSlotData role_slot;
        ASTPartySharedData party_shared;
        ASTContextAccessData context_access;
        ASTPartyInstanceData party_instance;
        ASTRosterDeclData roster_decl;
        ASTRosterSlotData roster_slot;
        ASTWorldDeclData world_decl;
        ASTWorldRosterData world_roster;
        ASTWorldZoneData world_zone;
        ASTWorldActivateData world_activate;
        ASTWorldDeactivateData world_deactivate;
        ASTWorldMaintainData world_maintain;
        ASTWorldStateData world_state;
        ASTIntentDeclData intent_decl;
        ASTIntentInvolvesData intent_involves;
        ASTIntentValueData intent_value;
        ASTIntentStepData intent_step;
        ASTRelationDeclData relation_decl;
        ASTEffectDeclData effect_decl;
        ASTZoneDeclData zone_decl;
        ASTDomainSlotData domain_slot;
        ASTZoneLayerSlotData zone_layer_slot;
        ASTZoneApplyData zone_apply;
        ASTZoneLinkData zone_link;
        ASTZoneDetachData zone_detach;
        ASTZoneUnlinkData zone_unlink;
        ASTZoneRefreshData zone_refresh;
        ASTZoneMaintainEffectData zone_maintain_effect;
        ASTZoneMaintainRelationData zone_maintain_relation;
        ASTZoneMaintainStateData zone_maintain_state;
        ASTZoneAuthorityData zone_authority;
        ASTZoneStateData zone_state;

        /* lifecycle Subject { Op: From -> To; } */
        struct {
            char* subject;
            LifecycleTransitionDecl* transitions;
            size_t transition_count;
            size_t transition_capacity;
        } lifecycle_decl;

        /* Event declaration */
        struct {
            char* name;
            ASTNode** params;          /* Event handler parameters */
            size_t param_count;
            size_t param_capacity;
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
            ASTNode* event;            /* Internal carrier; surface OnEvent(x) parses as AST_CALL. */
            ASTNode** arguments;       /* Arguments to pass */
            size_t arg_count;
        } event_invoke;

        /* Event handler type */
        struct {
            ASTNode** param_types;     /* Parameter types */
            size_t param_count;
            size_t param_capacity;
            ASTNode* return_type;      /* Return type */
        } event_handler_type;

        /* Lambda expression */
        struct {
            ASTNode** params;          /* Lambda parameters */
            size_t param_count;
            size_t param_capacity;
            ASTNode* body;             /* Expression or block */
            ASTNode* return_type;      /* Optional return type */
            bool is_async;             /* async lambda */
            LambdaCapture* captures;   /* Closure captures (semantic-filled) */
            size_t capture_count;
            size_t capture_capacity;
        } lambda_expr;

        ASTImportDeclData import_decl;
        ASTUseDeclData use_decl;
        ASTNamespaceDeclData namespace_decl;

        /* unsafe { ... } / unsafe(cap) { ... } / unsafe label { ... } */
        struct {
            ASTNode* body;           /* Block */
            char*    capability;     /* optional capability label, else NULL */
        } unsafe_block;

        /* transaction { ... } -- atomic/saga scope (saga CFG emitted in codegen) */
        struct {
            ASTNode*  body;                  /* Block */
            ASTNode** compensations;         /* `compensate <expr>;` handlers */
            size_t    compensation_count;    /* run in reverse order on `fail` */
            size_t    compensation_capacity;
        } transaction_block;

        /* fail; or fail <expr>; -- triggers transaction rollback */
        struct {
            ASTNode* reason;         /* optional reason expression, may be NULL */
        } fail_stmt;

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
