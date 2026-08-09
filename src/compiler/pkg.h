#ifndef PGY_PKG_H
#define PGY_PKG_H

#include <stdbool.h>

/* Seashell package command owner.
 *
 * pgy.toml is the source-of-truth for package entry discovery and backend
 * choice. Effect, authority, capability, registry, and dependency solving
 * surfaces remain fail-closed until their verifier/resolver owners exist.
 */
int driver_run_pkg_init(int argc, char *argv[]);
int driver_run_pkg_command(const char *launcher_path,
                           bool native_pipeline,
                           const char *verb,
                           int argc,
                           char *argv[]);

#endif
