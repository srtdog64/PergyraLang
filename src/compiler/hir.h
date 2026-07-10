#ifndef PERGYRA_HIR_H
#define PERGYRA_HIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../parser/ast.h"
#include "../common/arena.h"

typedef struct HIRProgram HIRProgram;
typedef struct HIRBasicBlock HIRBasicBlock;
typedef struct HIRPhiNode HIRPhiNode;

typedef enum
{
    HIR_TOPLEVEL_EXTERN,
    HIR_TOPLEVEL_TYPE,
    HIR_TOPLEVEL_ABILITY,
    HIR_TOPLEVEL_ROLE,
    HIR_TOPLEVEL_PARTY,
    HIR_TOPLEVEL_SYSTEMIC,
    HIR_TOPLEVEL_WORLD,
    HIR_TOPLEVEL_RELATION,
    HIR_TOPLEVEL_EFFECT,
    HIR_TOPLEVEL_ZONE,
    
    HIR_TOPLEVEL_EVENT,
    HIR_TOPLEVEL_INTENT,
    HIR_TOPLEVEL_FUNCTION,
    HIR_TOPLEVEL_EXECUTABLE
} HIRTopLevelKind;

typedef enum
{
    HIR_PHASE_EXTERN,
    HIR_PHASE_TYPE,
    HIR_PHASE_CAPABILITY,
    HIR_PHASE_DOMAIN,
    HIR_PHASE_ROUTINE,
    HIR_PHASE_EXECUTABLE
} HIRPhase;

typedef struct
{
    HIRTopLevelKind kind;
    ASTNode        *ast;
    const char     *name;
} HIRTopLevelItem;

typedef struct
{
    size_t           id;
    uint32_t         source_syntax_id;
    HIRTopLevelKind  kind;
    HIRPhase         phase;
    ASTNode         *ast;
    const char      *name;
} HIRDecl;

typedef struct
{
    uint32_t         routine_id;
    uint32_t         source_syntax_id;
    size_t           decl_id;
    HIRTopLevelKind  kind;
    const char      *name;
    const char      *owner_name;
    ASTNodeType      owner_ast_type;
    ASTNode         *ast;
    ASTNode         *body;
    bool             is_hosted;
    bool             is_action_like;
    bool             has_control_flow;
    bool             is_exported;
    bool             is_entry_reachable;
    const char     **signature_type_refs;
    size_t           signature_type_ref_count;
    size_t           signature_type_ref_capacity;
    const char     **direct_calls;
    uint32_t        *direct_call_decl_ids;
    size_t           direct_call_count;
    size_t           direct_call_capacity;
    uint32_t        *callee_routine_ids;
    size_t           callee_routine_count;
    size_t           callee_routine_capacity;
    size_t           reachable_block_count;
    size_t           dead_block_count;
    size_t           return_block_count;
    size_t           normal_exit_block_count;
    size_t           phi_candidate_count;
    size_t           phi_candidate_block_count;
    struct {
        struct HIRBasicBlock *blocks;
        size_t                block_count;
        size_t                block_capacity;
        size_t                entry_block;
    } cfg;
    bool             has_cfg;
    /* Pass-local scratch arena: reused across HIR analysis passes
     * (dominance, natural-loop walk, future CFG transforms).  Lifetime
     * binds to the enclosing HIRRoutine — initialised at construction,
     * destroyed in hir_destroy(). */
    PgyArena         scratch;
} HIRRoutine;

typedef struct
{
    const HIRRoutine *routines;
    size_t            count;
} HIRRoutineInventory;

typedef struct
{
    HIRRoutine *routines;
    size_t      count;
} HIRMutableRoutineInventory;

typedef enum
{
    HIR_BLOCK_FALLTHROUGH,
    HIR_BLOCK_GOTO,
    HIR_BLOCK_BRANCH,
    HIR_BLOCK_RETURN,
    HIR_BLOCK_UNREACHABLE
} HIRBlockTerminatorKind;

struct HIRPhiNode
{
    const char *name;
    size_t     *incoming_predecessors;
    size_t      incoming_predecessor_count;
};

struct HIRBasicBlock
{
    size_t                  id;
    ASTNode               **statements;
    size_t                  statement_count;
    size_t                  statement_capacity;
    bool                    is_pin_region;
    bool                    is_select_case_body;
    bool                    pin_view_is_write;
    const char             *pin_source_name;
    const char             *pin_view_name;
    ASTNode                *pin_block_ast;
    bool                    is_loop_header;
    bool                    is_reachable;
    size_t                  loop_depth;
    HIRBlockTerminatorKind  terminator_kind;
    ASTNode                *terminator_condition;
    ASTNode                *terminator_value;
    size_t                  succ_true;
    size_t                  succ_false;
    bool                    has_succ_true;
    bool                    has_succ_false;
    size_t                 *predecessors;
    size_t                  predecessor_count;
    size_t                  predecessor_capacity;
    size_t                  rpo_index;
    size_t                  immediate_dominator;
    bool                    has_immediate_dominator;
    size_t                 *dom_tree_children;
    size_t                  dom_tree_child_count;
    size_t                  dom_tree_child_capacity;
    const char            **local_defs;
    size_t                  local_def_count;
    size_t                  local_def_capacity;
    size_t                 *dominance_frontier;
    size_t                  dominance_frontier_count;
    size_t                  dominance_frontier_capacity;
    const char            **phi_candidates;
    size_t                  phi_candidate_count;
    size_t                  phi_candidate_capacity;
    HIRPhiNode            *phi_nodes;
    size_t                  phi_node_count;
};

typedef enum
{
    HIR_DUMP_SUMMARY,
    HIR_DUMP_CFG,
    HIR_DUMP_DOM,
    HIR_DUMP_SSA
} HIRDumpMode;

typedef struct
{
    bool include_functions;
    bool include_intents;
    bool require_control_flow;
    bool require_action_like;
    bool require_cfg;
    bool require_entry_reachable;
} HIRRoutinePassFilter;

typedef bool (*HIRRoutinePassFn)(const HIRProgram *hir,
                                 const HIRRoutine *routine,
                                 void *userdata,
                                 char **error_message);

typedef struct
{
    const char       *name;
    HIRRoutinePassFilter filter;
    HIRRoutinePassFn  run;
    void             *userdata;
    size_t            routines_visited;
    size_t            routines_matched;
} HIRRoutinePass;

typedef struct
{
    bool include_functions;
    bool include_intents;
    bool require_cfg;
    bool require_entry_reachable;
    bool include_reachable_blocks;
    bool include_dead_blocks;
} HIRBlockPassFilter;

typedef bool (*HIRBlockPassFn)(const HIRProgram *hir,
                               const HIRRoutine *routine,
                               const HIRBasicBlock *block,
                               void *userdata,
                               char **error_message);

typedef struct
{
    const char        *name;
    HIRBlockPassFilter filter;
    HIRBlockPassFn     run;
    void              *userdata;
    size_t             routines_visited;
    size_t             blocks_visited;
    size_t             blocks_matched;
} HIRBlockPass;

struct HIRProgram
{
    HIRTopLevelItem  *items;
    size_t            item_count;
    size_t            item_capacity;
    HIRDecl          *decls;
    size_t            decl_count;
    size_t            decl_capacity;
    HIRRoutine       *routines;
    size_t            routine_count;
    size_t            routine_capacity;

    ASTNode         **externs;
    size_t            extern_count;
    size_t            extern_capacity;
    ASTNode         **types;
    size_t            type_count;
    size_t            type_capacity;
    ASTNode         **abilities;
    size_t            ability_count;
    size_t            ability_capacity;
    ASTNode         **roles;
    size_t            role_count;
    size_t            role_capacity;
    ASTNode         **parties;
    size_t            party_count;
    size_t            party_capacity;
    ASTNode         **rosters;
    size_t            roster_count;
    size_t            roster_capacity;
    ASTNode         **worlds;
    size_t            world_count;
    size_t            world_capacity;
    ASTNode         **relations;
    size_t            relation_count;
    size_t            relation_capacity;
    ASTNode         **effects;
    size_t            effect_count;
    size_t            effect_capacity;
    ASTNode         **zones;
    size_t            zone_count;
    size_t            zone_capacity;
    ASTNode         **subjects;
    size_t            subject_count;
    size_t            subject_capacity;
    ASTNode         **events;
    size_t            event_count;
    size_t            event_capacity;
    ASTNode         **intents;
    size_t            intent_count;
    size_t            intent_capacity;
    ASTNode         **functions;
    size_t            function_count;
    size_t            function_capacity;
    ASTNode         **executables;
    size_t            executable_count;
    size_t            executable_capacity;
    ASTNode          *synthetic_executable_func;

    bool              has_main_function;
};

HIRProgram *hir_lower(ASTNode *annotated_ast, char **error_message);
void        hir_destroy(HIRProgram *hir);
bool        hir_validate(const HIRProgram *hir, char **error_message);
void        hir_dump(const HIRProgram *hir, FILE *out);
void        hir_dump_mode(const HIRProgram *hir, FILE *out, HIRDumpMode mode);
const char *hir_top_level_kind_name(HIRTopLevelKind kind);
const char *hir_phase_name(HIRPhase phase);
const HIRDecl *hir_find_decl(const HIRProgram *hir,
                             const char *name,
                             HIRTopLevelKind kind);
const HIRRoutine *hir_find_routine(const HIRProgram *hir,
                                   const char *name,
                                   HIRTopLevelKind kind);
const HIRRoutine *hir_find_routine_by_id(const HIRProgram *hir,
                                         uint32_t routine_id);
void hir_routine_inventory_from_program(
        const HIRProgram *hir,
        HIRRoutineInventory *inventory);
const HIRRoutine *hir_routine_inventory_get(
        const HIRRoutineInventory *inventory,
        size_t index);
void hir_mutable_routine_inventory_from_program(
        HIRProgram *hir,
        HIRMutableRoutineInventory *inventory);
HIRRoutine *hir_mutable_routine_inventory_get(
        const HIRMutableRoutineInventory *inventory,
        size_t index);
bool hir_run_routine_pass(HIRProgram *hir,
                          HIRRoutinePass *pass,
                          char **error_message);
bool hir_run_block_pass(HIRProgram *hir,
                        HIRBlockPass *pass,
                        char **error_message);

#endif
