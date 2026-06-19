#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H

#include "transpiler_mir_ssa_map.h"

bool transpiler_mir_seed_block_phi_names(const MIRBasicBlock *block,
                                         TranspilerSSANameMap *ssa_map_out);
ASTNode *transpiler_mir_find_stmt_for_inst(const MIRInstruction *inst);
bool transpiler_mir_inst_is_cfg_container(const MIRInstruction *inst);
bool transpiler_mir_def_uses_source_statement_emit(const MIRInstruction *inst,
                                                   const ASTNode *stmt,
                                                   ASTNodeType expected_type);
bool transpiler_mir_def_uses_source_local_decl_emit(const MIRInstruction *inst,
                                                    const ASTNode *stmt);
bool transpiler_mir_def_uses_channel_receive_statement_emit(
    const MIRInstruction *inst,
    const ASTNode *stmt,
    ASTNodeType expected_type);
bool transpiler_mir_def_uses_select_receive_statement_emit(
    const MIRInstruction *inst,
    const ASTNode *stmt,
    ASTNodeType expected_type);
bool transpiler_mir_seed_pin_view_alias(const MIRBasicBlock *block,
                                        TranspilerSSANameMap *ssa_map_out);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H */
