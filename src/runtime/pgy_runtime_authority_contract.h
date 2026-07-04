#ifndef PGY_RUNTIME_AUTHORITY_CONTRACT_H
#define PGY_RUNTIME_AUTHORITY_CONTRACT_H

#define PGY_ZONE_AUTHORITY_CODE_OK "ok"
#define PGY_ZONE_AUTHORITY_CODE_UNKNOWN "unknown"
#define PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE "missing-zone"
#define PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT "missing-participant"
#define PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH "authority-token-mismatch"

#define PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE \
    "zone authority validation failed: null zone self"
#define PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT \
    "zone authority validation failed: null authority participant"
#define PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH \
    "zone authority validation failed: authority token mismatch"

#endif /* PGY_RUNTIME_AUTHORITY_CONTRACT_H */
