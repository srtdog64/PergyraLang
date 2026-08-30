#include "compiler_release_artifact_policy.h"

const char *
compiler_release_artifact_link_flag(PgyOptProfile opt_profile)
{
    if (opt_profile != PGY_OPT_RELEASE)
        return NULL;

#ifdef __APPLE__
    /* Darwin ld spells the same release boundary as strip -S -x. */
    return "-Wl,-S,-x";
#else
    /* GCC/Clang driver spelling for stripping the final ELF/PE image. */
    return "-s";
#endif
}
