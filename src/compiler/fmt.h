#ifndef PGY_FMT_H
#define PGY_FMT_H

/* pgy fmt — source code formatter
 * Usage: pgy fmt <file.pgy> [--write] [--check]
 *   --write: overwrite file in-place (default: stdout)
 *   --check: return non-zero if file is not formatted
 */
int driver_run_fmt_command(const char *launcher_path, int argc, char *argv[]);

#endif
