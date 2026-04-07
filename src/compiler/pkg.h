#ifndef PGY_PKG_H
#define PGY_PKG_H

/* pgy init — create pgy.toml manifest
 * pgy install — install dependencies (future)
 *
 * Usage:
 *   pgy init                  → create pgy.toml in current dir
 *   pgy init <project-name>   → create pgy.toml with name
 */
int driver_run_pkg_init(int argc, char *argv[]);

#endif
