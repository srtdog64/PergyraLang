#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "semantic/capability_analyze.h"
#include "runtime/pgy_runtime_capability.h"
#include "runtime/pgy_runtime_file_mode_capability.h"

int
main(void)
{
    assert(capability_builtin_registry_ready());
    assert(capability_for_builtin("Args") == PGY_CAP_ENV);
    assert(capability_for_builtin("CompilerArtifactWrite") == PGY_CAP_IO_WRITE);
    assert(capability_for_builtin("DirWalk") == PGY_CAP_IO_READ);
    assert(capability_for_builtin("FileExists") == PGY_CAP_IO_READ);
    assert(capability_for_builtin("Now") == PGY_CAP_CLOCK);
    assert(capability_for_builtin("Random") == PGY_CAP_RANDOM);
    assert(capability_for_builtin("SeedRandom") == PGY_CAP_RANDOM);
    assert(capability_for_builtin("FileOpen") == PGY_CAP_NONE);
    assert(capability_for_builtin("NotABuiltin") == PGY_CAP_NONE);

    assert(pgy_file_mode_capability_mask("r") == PGY_CAP_IO_READ);
    assert(pgy_file_mode_capability_mask("w") == PGY_CAP_IO_WRITE);
    assert(pgy_file_mode_capability_mask("a") == PGY_CAP_IO_WRITE);
    assert(pgy_file_mode_capability_mask("r+") ==
           (PGY_CAP_IO_READ | PGY_CAP_IO_WRITE));
    assert(pgy_file_mode_capability_mask("") ==
           (PGY_CAP_IO_READ | PGY_CAP_IO_WRITE));
    return 0;
}
