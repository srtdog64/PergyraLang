#ifndef PERGYRA_MIR_PARALLEL_CAPTURE_FACTS_H
#define PERGYRA_MIR_PARALLEL_CAPTURE_FACTS_H

#include "mir.h"

bool mir_import_parallel_capture_facts(MIRProgram *mir,
                                       const SemanticResult *semantic,
                                       char **error_message);
bool mir_validate_parallel_capture_facts(const MIRProgram *mir,
                                         char **error_message);
void mir_parallel_capture_facts_clear(MIRProgram *mir);
size_t mir_parallel_capture_boundary_count(const MIRProgram *mir);
const MIRParallelCaptureBoundaryFact *mir_parallel_capture_boundary_at(
    const MIRProgram *mir, size_t index);
const MIRParallelCaptureBoundaryFact *mir_parallel_capture_boundary_find(
    const MIRProgram *mir, uint32_t source_stable_id);
const MIRParallelCaptureDispositionRow *mir_parallel_capture_disposition_find(
    const MIRParallelCaptureBoundaryFact *boundary,
    const char *name,
    MIRParallelCaptureDispositionKind kind);

#endif
