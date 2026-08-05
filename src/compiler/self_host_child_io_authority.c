#include "self_host_child_io_authority.h"

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
