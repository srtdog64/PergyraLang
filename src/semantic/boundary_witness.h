#ifndef PERGYRA_BOUNDARY_WITNESS_H
#define PERGYRA_BOUNDARY_WITNESS_H

#include <stdbool.h>
#include <stddef.h>

struct SemanticContext;

typedef enum
{
    PGY_BOUNDARY_WITNESS_OP_ACQ_READ,
    PGY_BOUNDARY_WITNESS_OP_ACQ_WRITE,
    PGY_BOUNDARY_WITNESS_OP_RELEASE
} PgyBoundaryWitnessOp;

typedef struct
{
    size_t acq_read_count;
    size_t acq_write_count;
    size_t release_count;
    size_t guard_no_current_writer_count;
    size_t guard_no_current_access_count;
    size_t accepted_count;
    size_t rejected_count;
    size_t guard_violation_count;
} PgyBoundaryWitnessSummary;

bool pgy_boundary_witness_guard_accepts(size_t current_readers,
                                        bool current_writer,
                                        PgyBoundaryWitnessOp op);
bool pgy_boundary_witness_summary_is_guard_consistent(
    const PgyBoundaryWitnessSummary *summary);

void semantic_boundary_witness_record_acq_read(struct SemanticContext *ctx,
                                               bool accepted);
void semantic_boundary_witness_record_acq_write(struct SemanticContext *ctx,
                                                bool accepted);
void semantic_boundary_witness_record_release(struct SemanticContext *ctx);
void semantic_boundary_witness_record_guard_violation(
    struct SemanticContext *ctx);

#endif /* PERGYRA_BOUNDARY_WITNESS_H */
