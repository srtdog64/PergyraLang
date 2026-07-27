#ifndef PGY_RUNTIME_CAPABILITY_H
#define PGY_RUNTIME_CAPABILITY_H

/*
 * Content capability vocabulary (the runtime-enforced effect boundary).
 *
 * Pergyra's effect system declares, per function, which ambient authorities a
 * piece of content uses. This header is the runtime half: a process-wide granted
 * set (the "manifest") that gates ambient-authority operations fail-closed. A
 * loader running untrusted distributable content sets a restricted manifest
 * (pgy_cap_set_manifest_export) before running it; any gated operation outside
 * the grant panics with class `capability-denied`. The default grant is
 * PGY_CAP_ALL so ordinary (trusted) programs are unaffected until a manifest is
 * imposed. The static AIR side proves declared-effects ⊇ used-effects so the
 * manifest cannot under-declare; this runtime gate enforces it at the boundary.
 *
 * One bit per capability so a manifest is a single mask. RENDER/AUDIO/INPUT are
 * reserved for the media runtime; the IO/NETWORK/CLOCK/RANDOM/ENV bits are the
 * fingerprinting/exfiltration surface that makes untrusted content dangerous.
 */

#define PGY_CAP_NONE       (0u)
#define PGY_CAP_IO_READ    (1u << 0)   /* read files / stdin */
#define PGY_CAP_IO_WRITE   (1u << 1)   /* write files / persist */
#define PGY_CAP_NETWORK    (1u << 2)   /* send / receive */
#define PGY_CAP_CLOCK      (1u << 3)   /* wall-clock time (fingerprinting) */
#define PGY_CAP_RANDOM     (1u << 4)   /* entropy / RNG */
#define PGY_CAP_ENV        (1u << 5)   /* process args / environment */
#define PGY_CAP_RENDER     (1u << 6)   /* reserved: canvas/WebGL draw */
#define PGY_CAP_AUDIO      (1u << 7)   /* reserved: audio output */
#define PGY_CAP_INPUT      (1u << 8)   /* reserved: pointer/key input */
#define PGY_CAP_ALL        (0xFFFFFFFFu)

#include <stdlib.h>
#include <string.h>

/*
 * Host-imposed grant channel (the symmetric mirror of the budget's
 * PGY_BUDGET_* env limits): if PGY_CAP_GRANT is set in the environment, the
 * runtime restricts the process-wide granted set to exactly the named
 * capabilities before the first gated op. A loader runs untrusted content with,
 * e.g., PGY_CAP_GRANT="io_read,clock" and every ungranted ambient op then
 * fail-closes. Tokens are case-insensitive cap names separated by any of
 * ", ;|+". Unknown tokens are ignored (they cannot widen the grant). "all"
 * grants everything, "none"/empty grants nothing. When PGY_CAP_GRANT is unset
 * the default PGY_CAP_ALL stands, so trusted programs are unaffected.
 */

/* One token -> its bit, or PGY_CAP_NONE for an unknown name. */
static inline unsigned
pgy_cap_token_bit(const char *tok, size_t len)
{
    struct { const char *name; unsigned bit; } table[] = {
#define PGY_CALLABLE_CONTRACT_RUNTIME_CAP(spelling, mask_symbol) \
        { spelling, mask_symbol },
#include "pgy_callable_contract_capability_projection.h"
#undef PGY_CALLABLE_CONTRACT_RUNTIME_CAP
        { "all",      PGY_CAP_ALL },
        { "none",     PGY_CAP_NONE },
    };
    size_t i;

    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strlen(table[i].name) == len &&
            strncmp(tok, table[i].name, len) == 0)
            return table[i].bit;
    }
    return PGY_CAP_NONE;
}

/* Parse PGY_CAP_GRANT into *mask_out. Returns 1 if the var was present (the
 * mask is then authoritative, even when it resolves to PGY_CAP_NONE), else 0. */
static inline int
pgy_cap_env_grant(unsigned *mask_out)
{
    const char *s = getenv("PGY_CAP_GRANT");
    unsigned mask = PGY_CAP_NONE;
    const char *p;
    size_t i;
    char buf[16];

    if (s == NULL)
        return 0;
    p = s;
    while (*p != '\0') {
        size_t len = 0;

        while (*p == ',' || *p == ' ' || *p == ';' || *p == '|' || *p == '+')
            p++;
        while (p[len] != '\0' && p[len] != ',' && p[len] != ' ' &&
               p[len] != ';' && p[len] != '|' && p[len] != '+')
            len++;
        if (len == 0)
            break;
        /* lowercase into a small fixed buffer; over-long tokens are unknown */
        if (len < sizeof(buf)) {
            for (i = 0; i < len; i++) {
                char c = p[i];
                buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
            }
            mask |= pgy_cap_token_bit(buf, len);
        }
        p += len;
    }
    *mask_out = mask;
    return 1;
}

#endif /* PGY_RUNTIME_CAPABILITY_H */
