#ifndef PERGYRA_TRANSPILER_MIR_SSA_MAP_H
#define PERGYRA_TRANSPILER_MIR_SSA_MAP_H

#include "transpiler.h"

#define TRANSPILE_SSA_MAP_CAPACITY 256
#define TRANSPILE_SSA_NAME_BUCKETS 1024

typedef struct
{
    bool in_use;
    const char *base_name;
    const char *versioned_name;
} TranspilerSSANameBucket;

typedef struct
{
    TranspilerSSANameBucket buckets[TRANSPILE_SSA_NAME_BUCKETS];
} TranspilerSSANameMap;

bool transpiler_parse_versioned_name(const char *versioned,
                                     char *base,
                                     size_t base_size,
                                     size_t *version_out);
void transpiler_ssa_map_clear(TranspilerSSANameMap *map);
bool transpiler_ssa_name_map_set(TranspilerSSANameMap *map,
                                 const char *base_name,
                                 const char *versioned_name);
bool transpiler_collect_ssa_name_entries(const char **versioned_values,
                                         size_t value_count,
                                         const char **base_names,
                                         const char **versioned_names,
                                         size_t max_entries,
                                         size_t *map_count_out);
void transpiler_free_ssa_name_entries(const char **base_names,
                                      size_t entry_count);
bool transpiler_rebuild_ssa_map(TranspilerSSANameMap *ssa_map,
                                const char **base_names,
                                const char **versioned_names,
                                size_t map_count);
const char *transpiler_resolve_ssa_name(const TranspilerSSANameMap *ssa_map,
                                        const char *base_name);
void transpiler_emit_mir_block_mapping_comment(CodeBuf *out,
                                               int indent,
                                               const char *routine_name,
                                               const MIRRoutine *routine,
                                               const MIRBasicBlock *block);

#endif /* PERGYRA_TRANSPILER_MIR_SSA_MAP_H */
