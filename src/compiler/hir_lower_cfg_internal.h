#ifndef PERGYRA_HIR_LOWER_CFG_INTERNAL_H
#define PERGYRA_HIR_LOWER_CFG_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include "hir.h"

typedef struct
{
    bool        active;
    bool        view_is_write;
    const char *source_name;
    const char *view_name;
    ASTNode    *block_ast;
} HIRPinRegionContext;

typedef struct HIRLoopContext
{
    bool                    active;
    const char             *label;
    size_t                  break_target;
    size_t                  continue_target;
    const struct HIRLoopContext *parent;
} HIRLoopContext;

bool    hir_cfg_append_stmt(ASTNode ***items,
                            size_t *count,
                            size_t *capacity,
                            ASTNode *node);
bool    hir_cfg_append_resource_scope_exit(HIRBasicBlock *block,
                                           ASTNode *with_stmt);
void    hir_cfg_apply_pin_region(HIRBasicBlock *block, const HIRPinRegionContext *pin);
ssize_t hir_cfg_new_block(HIRBasicBlock **blocks, size_t *count, size_t *capacity);
ssize_t hir_cfg_new_region_block(HIRBasicBlock **blocks,
                                 size_t *count,
                                 size_t *capacity,
                                 const HIRPinRegionContext *pin);
bool    hir_cfg_set_goto(HIRBasicBlock *block, size_t succ);
bool    hir_cfg_set_branch(HIRBasicBlock *block,
                           ASTNode *condition,
                           size_t succ_true,
                           size_t succ_false);
bool    hir_cfg_set_branch_with_value(HIRBasicBlock *block,
                                      ASTNode *condition,
                                      ASTNode *value,
                                      size_t succ_true,
                                      size_t succ_false);
bool    hir_cfg_set_return(HIRBasicBlock *block, ASTNode *value);
bool    hir_cfg_set_unreachable(HIRBasicBlock *block);
bool    hir_cfg_resolve_loop_target(const HIRLoopContext *loop,
                                    const char *label,
                                    bool continue_target,
                                    size_t *target_out);

#endif
