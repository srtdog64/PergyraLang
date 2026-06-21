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

#endif /* PGY_RUNTIME_CAPABILITY_H */
