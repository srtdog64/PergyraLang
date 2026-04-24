#ifndef PGY_RUNTIME_AUTHORITY_CONTRACT_H
#define PGY_RUNTIME_AUTHORITY_CONTRACT_H

#define PGY_ZONE_AUTHORITY_CODE_OK "ok"
#define PGY_ZONE_AUTHORITY_CODE_UNKNOWN "unknown"
#define PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE "missing-zone"
#define PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT "missing-participant"

#define PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE \
    "zone authority validation failed: null zone self"
#define PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT \
    "zone authority validation failed: null authority participant"

#define PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE \
    "[pgy][authority] zone '%s' entered with null self while validating '%s'\n"
#define PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT \
    "[pgy][authority] zone '%s' has null authority participant '%s'\n"

#endif /* PGY_RUNTIME_AUTHORITY_CONTRACT_H */

