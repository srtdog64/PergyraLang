#include "mir_parallel_capture_facts.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/semantic.h"
#include "mir_base_helpers.h"

static bool
semantic_parallel_capture_facts_validate(const SemanticResult *semantic,
                                         char **error_message)
{
    size_t count =
        semantic_result_parallel_capture_boundary_count(semantic);

    for (size_t i = 0; i < count; i++) {
        const SemanticParallelCaptureBoundaryFact *boundary =
            semantic_result_parallel_capture_boundary_at(semantic, i);
        if (boundary == NULL || boundary->source_stable_id == 0
            || !boundary->sealed
            || (boundary->row_count > 0 && boundary->rows == NULL)) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "semantic parallel capture boundary[%zu] has invalid shape",
                    i);
            return false;
        }
        for (size_t k = 0; k < i; k++) {
            const SemanticParallelCaptureBoundaryFact *prior =
                semantic_result_parallel_capture_boundary_at(semantic, k);
            if (prior != NULL && prior->source_stable_id
                == boundary->source_stable_id) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "semantic parallel capture boundary[%zu] duplicates stable id %u",
                        i, boundary->source_stable_id);
                return false;
            }
        }
        for (size_t j = 0; j < boundary->row_count; j++) {
            const SemanticParallelCaptureDispositionRow *row =
                &boundary->rows[j];
            bool kind_valid =
                row->kind == SEMANTIC_PARALLEL_CAPTURE_SNAPSHOT_COPY
                    ? row->writer_task < boundary->task_count
                    : (row->kind
                            == SEMANTIC_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT
                       || row->kind
                            == SEMANTIC_PARALLEL_CAPTURE_JOIN_READONLY)
                        && row->writer_task == 0;
            if (row->name == NULL || row->name[0] == '\0' || !kind_valid) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "semantic parallel capture boundary[%zu] row[%zu] is invalid",
                        i, j);
                return false;
            }
            for (size_t k = 0; k < j; k++) {
                if (strcmp(boundary->rows[k].name, row->name) == 0) {
                    if (error_message != NULL)
                        *error_message = mir_strdup_fmt(
                            "semantic parallel capture boundary[%zu] duplicates row '%s'",
                            i, row->name);
                    return false;
                }
            }
        }
    }
    return true;
}

void
mir_parallel_capture_facts_clear(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->parallel_capture_boundary_count; i++) {
        MIRParallelCaptureBoundaryFact *boundary =
            &mir->parallel_capture_boundaries[i];
        for (size_t j = 0; j < boundary->row_count; j++)
            free(boundary->rows[j].name);
        free(boundary->rows);
    }
    free(mir->parallel_capture_boundaries);
    mir->parallel_capture_boundaries = NULL;
    mir->parallel_capture_boundary_count = 0;
}

bool
mir_import_parallel_capture_facts(MIRProgram *mir,
                                  const SemanticResult *semantic,
                                  char **error_message)
{
    size_t count;

    if (mir == NULL || semantic == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR parallel capture import requires semantic facts");
        return false;
    }
    count = semantic_result_parallel_capture_boundary_count(semantic);
    if (count == 0)
        return true;
    if (!semantic_parallel_capture_facts_validate(semantic, error_message))
        return false;
    if (count > SIZE_MAX / sizeof(*mir->parallel_capture_boundaries)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR parallel capture boundary inventory overflow");
        return false;
    }
    mir->parallel_capture_boundaries = calloc(
        count, sizeof(*mir->parallel_capture_boundaries));
    if (mir->parallel_capture_boundaries == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    mir->parallel_capture_boundary_count = count;

    for (size_t i = 0; i < count; i++) {
        const SemanticParallelCaptureBoundaryFact *source =
            semantic_result_parallel_capture_boundary_at(semantic, i);
        MIRParallelCaptureBoundaryFact *target =
            &mir->parallel_capture_boundaries[i];
        if (source == NULL) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "semantic parallel capture boundary[%zu] is missing", i);
            return false;
        }
        target->source_stable_id = source->source_stable_id;
        target->task_count = source->task_count;
        target->sealed = source->sealed;
        target->row_count = source->row_count;
        if (source->row_count == 0)
            continue;
        if (source->row_count > SIZE_MAX / sizeof(*target->rows)) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "semantic parallel capture boundary[%zu] row inventory overflow",
                    i);
            return false;
        }
        target->rows = calloc(source->row_count, sizeof(*target->rows));
        if (target->rows == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
        for (size_t j = 0; j < source->row_count; j++) {
            target->rows[j].name = pergyra_strdup(source->rows[j].name);
            if (target->rows[j].name == NULL) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup("out of memory");
                return false;
            }
            switch (source->rows[j].kind) {
            case SEMANTIC_PARALLEL_CAPTURE_SNAPSHOT_COPY:
                target->rows[j].kind = MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY;
                break;
            case SEMANTIC_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT:
                target->rows[j].kind =
                    MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT;
                break;
            case SEMANTIC_PARALLEL_CAPTURE_JOIN_READONLY:
                target->rows[j].kind = MIR_PARALLEL_CAPTURE_JOIN_READONLY;
                break;
            }
            target->rows[j].writer_task = source->rows[j].writer_task;
        }
    }
    return mir_validate_parallel_capture_facts(mir, error_message);
}

size_t
mir_parallel_capture_boundary_count(const MIRProgram *mir)
{
    return mir != NULL ? mir->parallel_capture_boundary_count : 0;
}

const MIRParallelCaptureBoundaryFact *
mir_parallel_capture_boundary_at(const MIRProgram *mir, size_t index)
{
    if (mir == NULL || index >= mir->parallel_capture_boundary_count)
        return NULL;
    return &mir->parallel_capture_boundaries[index];
}

const MIRParallelCaptureBoundaryFact *
mir_parallel_capture_boundary_find(const MIRProgram *mir,
                                   uint32_t source_stable_id)
{
    if (mir == NULL || source_stable_id == 0)
        return NULL;
    for (size_t i = 0; i < mir->parallel_capture_boundary_count; i++) {
        const MIRParallelCaptureBoundaryFact *boundary =
            &mir->parallel_capture_boundaries[i];
        if (boundary->source_stable_id == source_stable_id)
            return boundary;
    }
    return NULL;
}

const MIRParallelCaptureDispositionRow *
mir_parallel_capture_disposition_find(
    const MIRParallelCaptureBoundaryFact *boundary,
    const char *name,
    MIRParallelCaptureDispositionKind kind)
{
    if (boundary == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < boundary->row_count; i++) {
        const MIRParallelCaptureDispositionRow *row = &boundary->rows[i];
        if (row->kind == kind && row->name != NULL
            && strcmp(row->name, name) == 0)
            return row;
    }
    return NULL;
}

bool
mir_validate_parallel_capture_facts(const MIRProgram *mir,
                                    char **error_message)
{
    if (mir == NULL)
        return false;
    if (mir->parallel_capture_boundary_count > 0
        && mir->parallel_capture_boundaries == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR parallel capture boundary inventory is missing");
        return false;
    }
    for (size_t i = 0; i < mir->parallel_capture_boundary_count; i++) {
        const MIRParallelCaptureBoundaryFact *boundary =
            &mir->parallel_capture_boundaries[i];
        if (boundary->source_stable_id == 0 || !boundary->sealed
            || (boundary->row_count > 0 && boundary->rows == NULL)) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR parallel capture boundary[%zu] has invalid shape", i);
            return false;
        }
        for (size_t k = 0; k < i; k++) {
            if (mir->parallel_capture_boundaries[k].source_stable_id
                == boundary->source_stable_id) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR parallel capture boundary[%zu] duplicates stable id %u",
                        i, boundary->source_stable_id);
                return false;
            }
        }
        for (size_t j = 0; j < boundary->row_count; j++) {
            const MIRParallelCaptureDispositionRow *row = &boundary->rows[j];
            bool kind_valid = row->kind == MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY
                ? row->writer_task < boundary->task_count
                : (row->kind == MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT
                   || row->kind == MIR_PARALLEL_CAPTURE_JOIN_READONLY)
                    && row->writer_task == 0;
            if (row->name == NULL || row->name[0] == '\0' || !kind_valid) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR parallel capture boundary[%zu] row[%zu] is invalid",
                        i, j);
                return false;
            }
            for (size_t k = 0; k < j; k++) {
                if (strcmp(boundary->rows[k].name, row->name) == 0) {
                    if (error_message != NULL)
                        *error_message = mir_strdup_fmt(
                            "MIR parallel capture boundary[%zu] duplicates row '%s'",
                            i, row->name);
                    return false;
                }
            }
        }
    }
    return true;
}
