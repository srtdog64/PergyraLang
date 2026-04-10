#ifndef PGY_DEBUGGER_H
#define PGY_DEBUGGER_H

/* pgy debug <file.pgy>
 *
 * Interactive step debugger.
 * Commands:
 *   n / next     — step to next statement
 *   c / continue — run to next breakpoint or end
 *   b <line>     — set breakpoint at line
 *   cl <line>    — clear breakpoint at line
 *   info break   — list breakpoints
 *   bt           — show current backtrace frame
 *   q / quit     — exit debugger
 *   l / list     — show current source context
 */
int driver_run_debug_command(int argc, char *argv[]);

#endif
