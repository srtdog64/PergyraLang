#ifndef PGY_SELF_HOST_CHILD_IO_AUTHORITY_H
#define PGY_SELF_HOST_CHILD_IO_AUTHORITY_H

/* Grants the delegated self-host driver the file authority pgy already holds,
 * so it can read and write the paths named on pgy's own command line. */
void driver_authorize_self_host_child_io(void);

#endif /* PGY_SELF_HOST_CHILD_IO_AUTHORITY_H */
