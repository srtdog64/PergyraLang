#ifndef PGY_MIR_SOURCE_LOCAL_TYPE_SHAPE_H
#define PGY_MIR_SOURCE_LOCAL_TYPE_SHAPE_H

#include "mir.h"

#define MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT 8
#define MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE 128

typedef struct MIRSourceLocalTypeScratch
{
    char buffers[MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT]
                [MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
    size_t next;
} MIRSourceLocalTypeScratch;

char *mir_source_local_type_scratch_next(MIRSourceLocalTypeScratch *scratch);
const char *mir_source_local_type_scratch_format(
    MIRSourceLocalTypeScratch *scratch,
    const char *outer,
    const char *inner);

bool mir_source_local_unwrap_slot_like_type(const char *type_name,
                                            char *out,
                                            size_t out_size);
bool mir_source_local_unwrap_iterable_type(const char *type_name,
                                           char *out,
                                           size_t out_size);
bool mir_source_local_unwrap_channel_type(const char *type_name,
                                          char *out,
                                          size_t out_size);
bool mir_source_local_unwrap_array_or_slice_type(const char *type_name,
                                                 char *out,
                                                 size_t out_size);
bool mir_source_local_unwrap_hash_map_key_type(const char *type_name,
                                               char *out,
                                               size_t out_size);
bool mir_source_local_unwrap_future_type(const char *type_name,
                                         char *out,
                                         size_t out_size,
                                         bool *is_remote_out);

#endif
