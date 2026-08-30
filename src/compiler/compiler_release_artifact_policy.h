#ifndef PGY_COMPILER_RELEASE_ARTIFACT_POLICY_H
#define PGY_COMPILER_RELEASE_ARTIFACT_POLICY_H

#include "compiler.h"

/*
 * Final-link policy for the shipped primary artifact. The returned flag is a
 * single compiler-driver argument and is NULL for profiles that must retain
 * developer symbols. Every executable-producing backend consumes this owner.
 */
const char *compiler_release_artifact_link_flag(PgyOptProfile opt_profile);

#endif /* PGY_COMPILER_RELEASE_ARTIFACT_POLICY_H */
