#ifndef PERGYRA_MIR_H
#define PERGYRA_MIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "hir.h"
#include "rir.h"

typedef struct MIRProgram MIRProgram;

typedef enum
{
    MIR_SCOPE_FUNCTION,
    MIR_SCOPE_METHOD,
    MIR_SCOPE_INTENT
} MIRScopeKind;

typedef enum
{
    MIR_INST_RESOURCE_OP,
    MIR_INST_PHI_PLACEHOLDER,
    MIR_INST_BRANCH,
    MIR_INST_RETURN,
    MIR_INST_CLEANUP_EDGE
} MIRInstKind;

typedef struct
{
    size_t          id;
    MIRInstKind     kind;
    const char     *name;
    const char     *arg0;
    const char     *arg1;
    const RIROp    *rir_op;
    ASTNode        *ast;
} MIRInstruction;

typedef struct
{
    size_t          id;
    bool            is_entry;
    bool            is_reachable;
    bool            is_cleanup;
    size_t          source_hir_block_id;
    size_t         *predecessors;
    size_t          predecessor_count;
    size_t          succ_true;
    size_t          succ_false;
    bool            has_succ_true;
    bool            has_succ_false;
    MIRInstruction *instructions;
    size_t          instruction_count;
} MIRBasicBlock;

typedef struct
{
    size_t             id;
    MIRScopeKind       kind;
    const char        *owner_name;
    const char        *name;
    const HIRRoutine  *hir_routine;
    const RIRScope    *rir_scope;
    MIRBasicBlock     *blocks;
    size_t             block_count;
    size_t             entry_block;
    size_t             cleanup_block;
    bool               has_cleanup_block;
    size_t             instruction_count;
    size_t             cleanup_instruction_count;
} MIRRoutine;

struct MIRProgram
{
    MIRRoutine *routines;
    size_t      routine_count;
};

MIRProgram *mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message);
void        mir_destroy(MIRProgram *mir);
void        mir_dump(const MIRProgram *mir, FILE *out);

const char *mir_scope_kind_name(MIRScopeKind kind);
const char *mir_inst_kind_name(MIRInstKind kind);

#endif
