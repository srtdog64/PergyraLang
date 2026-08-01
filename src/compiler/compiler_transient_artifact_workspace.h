#ifndef PGY_COMPILER_TRANSIENT_ARTIFACT_WORKSPACE_H
#define PGY_COMPILER_TRANSIENT_ARTIFACT_WORKSPACE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    char directory[1024];
    char primary_path[1024];
    char secondary_path[1024];
    bool active;
} CompilerTransientArtifactWorkspace;

bool compiler_transient_artifact_workspace_open(
    const char *source_path,
    const char *base_directory,
    const char *primary_suffix,
    const char *secondary_suffix,
    CompilerTransientArtifactWorkspace *workspace);

void compiler_transient_artifact_workspace_close(
    CompilerTransientArtifactWorkspace *workspace);

#endif /* PGY_COMPILER_TRANSIENT_ARTIFACT_WORKSPACE_H */
