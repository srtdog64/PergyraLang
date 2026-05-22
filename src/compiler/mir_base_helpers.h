#ifndef PGY_MIR_BASE_HELPERS_H
#define PGY_MIR_BASE_HELPERS_H

#include "mir.h"

bool append_instruction(MIRBasicBlock *block, MIRInstruction inst);
bool mir_commit_instruction(MIRRoutine *routine,
                            MIRBasicBlock *block,
                            MIRInstruction *inst);
char *mir_strdup_fmt(const char *fmt, ...);
bool insert_instruction(MIRBasicBlock *block, size_t index, MIRInstruction inst);
bool append_name(const char ***names,
                 size_t *count,
                 size_t *capacity,
                 const char *name);
bool append_owned_name(const char ***names,
                       size_t *count,
                       size_t *capacity,
                       char *name);
bool append_name_unique(const char ***names,
                        size_t *count,
                        size_t *capacity,
                        const char *name);
bool append_block(MIRRoutine *routine, MIRBasicBlock block);
bool append_routine(MIRProgram *mir, MIRRoutine routine);
bool mir_add_def_instruction(MIRRoutine *routine,
                             MIRBasicBlock *block,
                             size_t insert_index,
                             const char *base_name,
                             const char *result_name);
char *mir_make_versioned_name(const char *base, size_t version);
bool copy_indices(size_t **dst,
                  size_t *dst_count,
                  const size_t *src,
                  size_t src_count);
bool copy_versions(size_t **dst, const size_t *src, size_t count);
bool mir_block_has_predecessor(const MIRBasicBlock *block, size_t predecessor);
bool mir_store_block_versions(MIRBasicBlock *block,
                              bool is_entry,
                              const size_t *versions,
                              size_t count);
MIRScopeKind mir_scope_kind_from_hir(const HIRRoutine *routine);
const RIRScope *mir_find_matching_rir_scope(const RIRProgram *rir,
                                            const HIRRoutine *routine);

#endif
