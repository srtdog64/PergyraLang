#include "io_boundary_builtin.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char *kPgyCompilerIOBoundaryBuiltinNames[] = {
    "FileClose",
    "FileExists",
    "FileOpen",
    "FileRead",
    "FileWrite",
    "Input",
    "Now",
    "ReadFile",
    "ReadLine",
    "Sleep",
    "WriteFile",
};

static int
pgy_compiler_io_boundary_name_compare(const void *key, const void *entry)
{
    return strcmp((const char *)key, *(const char * const *)entry);
}

bool
pgy_compiler_io_boundary_builtin_is_stable(const char *name)
{
    if (name == NULL)
        return false;
    return bsearch(name,
                   kPgyCompilerIOBoundaryBuiltinNames,
                   sizeof(kPgyCompilerIOBoundaryBuiltinNames)
                       / sizeof(kPgyCompilerIOBoundaryBuiltinNames[0]),
                   sizeof(kPgyCompilerIOBoundaryBuiltinNames[0]),
                   pgy_compiler_io_boundary_name_compare) != NULL;
}
