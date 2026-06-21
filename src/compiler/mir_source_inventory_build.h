#ifndef PERGYRA_COMPILER_MIR_SOURCE_INVENTORY_BUILD_H
#define PERGYRA_COMPILER_MIR_SOURCE_INVENTORY_BUILD_H

/*
 * MIR source-inventory construction helpers.
 *
 * The deep-copy primitives for the per-block source arrays (AST statement
 * nodes, names, phi nodes) plus the non-CFG block source-inventory seeding
 * that records source provenance at MIR construction time. Split out of mir.c
 * so the CFG/body owner stays under the split-review limit; this is the
 * source-provenance machinery, a distinct responsibility from CFG lowering.
 */

#include "mir.h"
#include "../parser/ast_api.h"

/* Deep-copy `src_count` AST node pointers into a freshly allocated `*dst`.
 * On NULL/empty source, sets an empty inventory and succeeds. */
bool mir_copy_ast_nodes(ASTNode ***dst, size_t *dst_count,
                        ASTNode **src, size_t src_count);

/* Deep-copy `src_count` name pointers into a freshly allocated `*dst`. */
bool mir_copy_names(const char ***dst, size_t *dst_count,
                    const char **src, size_t src_count);

/* Deep-copy HIR phi nodes into freshly allocated MIR source phi nodes,
 * including their incoming-predecessor index arrays. */
bool mir_copy_phi_nodes(MIRSourcePhiNode **dst, size_t *dst_count,
                        const HIRPhiNode *src, size_t src_count);

/* Record a block's source line/column from `source_node` (cleared if NULL). */
void mir_block_record_source_location(MIRBasicBlock *block,
                                      const ASTNode *source_node);

/* Seed a non-CFG block's source-statement inventory from a function body. */
bool mir_seed_non_cfg_block_source_inventory(MIRBasicBlock *block,
                                             ASTNode *func_decl);

#endif /* PERGYRA_COMPILER_MIR_SOURCE_INVENTORY_BUILD_H */
