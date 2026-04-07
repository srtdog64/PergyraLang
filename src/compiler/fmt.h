#ifndef PGY_FMT_H
#define PGY_FMT_H

/* pgy fmt — source code formatter
 * Usage: pgy fmt <file.pgy> [--write]
 *   --write: overwrite file in-place (default: stdout)
 */
int driver_run_fmt_command(int argc, char *argv[]);

#endif
