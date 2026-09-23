#include "self_host_child_io_authority.h"

#include "../common/string_compat.h"
#include "import_resolver_internal.h"
#include "path_utils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* The self-host driver is a Pergyra program, so its file access runs through
 * the runtime IO policy in pgy_runtime_lib_file_path_core.h, which denies
 * absolute paths. That default targets *compiled user programs*; the delegated
 * driver is the compiler itself, reading and writing exactly the paths the user
 * named on pgy's own command line -- paths the native pipeline already handles
 * without restriction.
 *
 * Without this grant, `pgy --emit-c /abs/path.pgy` died at exit 1 with no
 * diagnostic at all: the driver's read was denied and pgy_read_file maps a
 * denial to an empty string.
 *
 * An operator-declared PGY_IO_ROOT is left untouched. That is a deliberate
 * sandbox, and a compile whose paths fall outside it must fail closed rather
 * than be silently widened here.
 */
void
driver_authorize_self_host_child_io(void)
{
    const char *root = getenv("PGY_IO_ROOT");
    if (root != NULL && root[0] != '\0')
        return;
#ifdef _WIN32
    (void)_putenv_s("PGY_IO_ALLOW_ABSOLUTE", "1");
#else
    (void)setenv("PGY_IO_ALLOW_ABSOLUTE", "1", 1);
#endif
}

static bool
driver_path_has_parent_component(const char *path)
{
    const char *part = path;

    while (*part != '\0') {
        const char *end = part;
        while (*end != '\0' && *end != '/' && *end != '\\')
            end++;
        if (end - part == 2 && part[0] == '.' && part[1] == '.')
            return true;
        part = *end != '\0' ? end + 1 : end;
    }
    return false;
}

/* The child's runtime IO policy refuses any path with a `..` component, the
 * rule for compiled user programs, so `pgy x.pgy --emit-c -o ../gen/x.c`
 * failed at begin-temp with no cause. pgy resolves the directory of such an
 * output path here, where it still holds the user's own authority, and hands
 * the child an absolute path. Other paths pass through unchanged. NULL means
 * the directory cannot be resolved or the path names no file. */
char *
driver_self_host_child_output_path_dup(const char *output_path)
{
    const char *separator = NULL;
    const char *file_name;
    char *dir;
    char *canonical_dir;
    char *resolved;
    size_t dir_len;

    if (output_path == NULL || output_path[0] == '\0')
        return NULL;
    if (!driver_path_has_parent_component(output_path))
        return pergyra_strdup(output_path);
    for (const char *p = output_path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\')
            separator = p;
    }
    file_name = separator != NULL ? separator + 1 : output_path;
    if (separator == NULL || file_name[0] == '\0'
        || strcmp(file_name, "..") == 0 || strcmp(file_name, ".") == 0)
        return NULL;
    dir_len = (size_t)(separator - output_path);
    dir = (char *)malloc(dir_len + 1);
    if (dir == NULL)
        return NULL;
    memcpy(dir, output_path, dir_len);
    dir[dir_len] = '\0';
    canonical_dir = import_resolver_existing_final_identity_path_dup(dir);
    free(dir);
    if (canonical_dir == NULL)
        return NULL;
    resolved = path_join_dup(canonical_dir, file_name);
    free(canonical_dir);
    return resolved;
}
