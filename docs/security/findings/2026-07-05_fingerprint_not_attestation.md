# Finding: hardware fingerprint fallback is a binding hint, not an attestation

- **Finding ID**: 2026-07-05-002
- **Date filed**: 2026-07-05
- **Audit run**: external red-team pass (Gemini/Antigravity), verified against
  current tree by Claude
- **Contract**: threat model tier A5 (host-drift / virtualized-identity
  attacker), `../02_red_team_threat_model.md`
- **Severity**: Informational (acknowledged inherent limit, not a code bug)
- **Status**: Wontfix (inherent — no hardware root of trust in software)

## Summary

`HardwareFingerprintGenerate` (`slot_security_fingerprint.c`) reads real
hardware identifiers (CPUID, board id, MAC) where the platform exposes them.
Where it cannot — inside a VM or container that virtualizes or withholds those
identifiers — it falls back to software-derived identity: `/etc/machine-id`
(then `/var/lib/dbus/machine-id`), hostname, and `uname` fields on Linux;
`GetComputerNameA` on Windows. All of these are **software-settable**, so an
attacker who controls the guest can make one machine present the fingerprint of
another and thereby satisfy a fingerprint-bound check it should not.

This is not a defect in the code. It is the fundamental limit of doing identity
binding without a hardware root of trust: any value software can read, software
can forge. The threat model already states this in the A5 row — "A5 is an
availability and binding contract, **not a promise that host hardware identity
is globally unspoofable.**" This finding records the specific fallback vectors
so the boundary is explicit, not assumed.

## What the red-team pass got wrong (stale), for the record

The external review also claimed the hash was the non-cryptographic FNV-1a and
that the fallback was hostname-primary. Both are false in the current tree:
`SecurityHashBytes64` uses `SecureHashSHA256` (`slot_security_fingerprint.c:30`),
and the Linux fallback reads `/etc/machine-id` before touching hostname. Those
specific recommendations were already implemented. Only the *inherent*
spoofability of any software fallback survives — which is this finding.

## Expected vs Actual

- **Expected (per A5)**: the fingerprint is a *binding hint* that ties a sealed
  token to the host it was minted on, best-effort, and degrades to a
  software-derived value when hardware identity is unavailable.
- **Actual**: exactly that. The gap is only that "binding hint, not
  attestation" was implied by A5 but not stated at the call site or in a
  contract. This finding closes that documentation gap.

## Root Cause

Inherent: software cannot attest hardware identity without a hardware root of
trust (TPM / secure enclave / platform attestation), which Pergyra's runtime
does not require and does not ship.

## Fix

- **Code change**: none warranted. A source comment at
  `SecurityFillFallbackIdentity` marks the fallback as spoofable-by-design and
  points here.
- **Doc update**: this finding + the A5 framing. Any beta-facing security doc
  that mentions the fingerprint must call it a *binding hint*, never an
  *attestation* or *tamper-proof device id* (marketing-drift guard).
- **If hardware attestation is ever required**: it is a new capability
  (TPM/enclave provider), tracked separately — not a patch to this fallback.

## Backend Parity Status

- C backend: not affected (shared runtime; behavior identical)
- LLVM backend: not affected (shared runtime)
- Backend-compare regression added: not applicable (no behavior change)

## Disclosure

Pergyra is pre-1.0 with no production users. Filed openly. Informational.

## References

- Threat model: `../02_red_team_threat_model.md` (tier A5)
- Marketing-language discipline: don't overclaim static/security strength
- Related finding: `2026-07-05_symlink_toctou_write.md` (same red-team pass)
