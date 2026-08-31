#ifndef PGY_SELF_HOST_PUBLIC_DIAGNOSTIC_WIRE_OWNER_H
#define PGY_SELF_HOST_PUBLIC_DIAGNOSTIC_WIRE_OWNER_H

#include <stddef.h>

/* Returns 1 after relay, 0 for an invalid envelope, and -1 on write failure. */
int driver_self_host_public_diagnostic_wire_relay(
    const unsigned char *payload,
    size_t payload_length);

#endif /* PGY_SELF_HOST_PUBLIC_DIAGNOSTIC_WIRE_OWNER_H */
