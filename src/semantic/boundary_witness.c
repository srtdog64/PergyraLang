#include "boundary_witness.h"
#include "type_checker.h"

bool
pgy_boundary_witness_guard_accepts(size_t current_readers,
                                   bool current_writer,
                                   PgyBoundaryWitnessOp op)
{
    switch (op) {
    case PGY_BOUNDARY_WITNESS_OP_ACQ_READ:
        return !current_writer;
    case PGY_BOUNDARY_WITNESS_OP_ACQ_WRITE:
        return current_readers == 0 && !current_writer;
    case PGY_BOUNDARY_WITNESS_OP_RELEASE:
        return true;
    }
    return false;
}

bool
pgy_boundary_witness_summary_is_guard_consistent(
    const PgyBoundaryWitnessSummary *summary)
{
    size_t guarded_count;
    size_t guard_count;
    size_t op_count;

    if (summary == NULL)
        return true;

    guarded_count =
        summary->acq_read_count + summary->acq_write_count;
    guard_count =
        summary->guard_no_current_writer_count
        + summary->guard_no_current_access_count;
    op_count = guarded_count + summary->release_count;

    return summary->guard_violation_count == 0
        && guard_count == guarded_count
        && summary->accepted_count + summary->rejected_count == op_count;
}

void
semantic_boundary_witness_record_acq_read(struct SemanticContext *ctx,
                                          bool accepted)
{
    if (ctx == NULL)
        return;
    ctx->boundary_witness_summary.acq_read_count++;
    ctx->boundary_witness_summary.guard_no_current_writer_count++;
    if (accepted)
        ctx->boundary_witness_summary.accepted_count++;
    else
        ctx->boundary_witness_summary.rejected_count++;
}

void
semantic_boundary_witness_record_acq_write(struct SemanticContext *ctx,
                                           bool accepted)
{
    if (ctx == NULL)
        return;
    ctx->boundary_witness_summary.acq_write_count++;
    ctx->boundary_witness_summary.guard_no_current_access_count++;
    if (accepted)
        ctx->boundary_witness_summary.accepted_count++;
    else
        ctx->boundary_witness_summary.rejected_count++;
}

void
semantic_boundary_witness_record_release(struct SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    ctx->boundary_witness_summary.release_count++;
    ctx->boundary_witness_summary.accepted_count++;
}

void
semantic_boundary_witness_record_guard_violation(struct SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    ctx->boundary_witness_summary.guard_violation_count++;
}
