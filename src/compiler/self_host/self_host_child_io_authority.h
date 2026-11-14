#ifndef PGY_SELF_HOST_CHILD_IO_AUTHORITY_H
#define PGY_SELF_HOST_CHILD_IO_AUTHORITY_H

/* Grants the delegated self-host driver the file authority pgy already holds,
 * so it can read and write the paths named on pgy's own command line. */
void driver_authorize_self_host_child_io(void);

/* The output path to hand the child: absolute when the user's path has a
 * `..` component, which the child's IO policy refuses; otherwise a copy.
 * NULL when the directory cannot be resolved or the path names no file. */
char *driver_self_host_child_output_path_dup(const char *output_path);

#endif /* PGY_SELF_HOST_CHILD_IO_AUTHORITY_H */
